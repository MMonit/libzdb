#include "Config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include <sys/wait.h>

#include "zdb.h"
#include "Thread.h"


/**
 * SQLite busy handler tests: a connection that finds the database write-locked waits for the lock in libzdb's busy handler, which retries as soon as a
 * connection of this process ends a write transaction.
 *
 * 1. Hand-off: a writer holds the write lock 150 ms, another one waits: the time from the commit to the waiter's success.
 * 2. Timeout: the lock is held 1.2 s: the waiter fails with "database is locked" after the busy timeout (500 ms).
 * 3. Chunked writer: a writer takes the lock 40 times for 50 ms with a 2 ms pause, 8 threads insert meanwhile: failed inserts and the longest wait.
 * 4. Another process: a child process holds and releases the lock; its commit wakes nobody up here, the waiter notices the release by polling.
 */

#define BUSY_TIMEOUT 500 // ms


static char db[256];
static ConnectionPool_T pool;


static long long now_us(void) {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return (long long)t.tv_sec * 1000000LL + t.tv_nsec / 1000;
}


static void sleep_ms(int ms) {
        usleep(ms * 1000);
}


static ConnectionPool_T pool_new(void) {
        char url[512];
        snprintf(url, sizeof(url), "sqlite://%s?journal_mode=wal&synchronous=normal&busy_timeout=%d", db, BUSY_TIMEOUT);
        ConnectionPool_T p = ConnectionPool_new(URL_new(url));
        ConnectionPool_setInitialConnections(p, 2);
        ConnectionPool_setMaxConnections(p, 20);
        ConnectionPool_start(p);
        return p;
}


static void pool_free(ConnectionPool_T *p) {
        URL_T url = ConnectionPool_getURL(*p);
        ConnectionPool_free(p);
        URL_free(&url);
}


// Insert one row in autocommit mode. Returns the µs it took, or -1 if it failed (database is locked)
static long long insert(Connection_T con) {
        volatile long long t0 = now_us(), took = -1;
        TRY
        {
                Connection_execute(con, "INSERT INTO t (v) VALUES (1);");
                took = now_us() - t0;
        }
        CATCH(SQLException)
        {
                took = -1;
        }
        END_TRY;
        return took;
}


// ---------------------------------------------------------------- Holders


typedef struct Hold {
        int times;             // how many times to take the lock
        int hold;              // ms to hold it
        int pause;             // ms between
        volatile long long released; // µs, when the last commit returned
} Hold;


static void *holder(void *arg) {
        Hold *h = arg;
        Connection_T con = ConnectionPool_getConnection(pool);
        assert(con);
        for (int i = 0; i < h->times; i++) {
                Connection_beginTransactionType(con, TRANSACTION_IMMEDIATE);
                Connection_execute(con, "INSERT INTO t (v) VALUES (0);");
                sleep_ms(h->hold);
                Connection_commit(con);
                h->released = now_us();
                if (h->pause)
                        sleep_ms(h->pause);
        }
        Connection_close(con);
        return NULL;
}


// ---------------------------------------------------------------- Test 1


static void handoff(void) {
        long long lat[10];
        for (int i = 0; i < 10; i++) {
                Hold h = {.times = 1, .hold = 150};
                Thread_T t;
                Thread_create(t, holder, &h);
                sleep_ms(20); // the holder has the lock
                Connection_T con = ConnectionPool_getConnection(pool);
                long long took = insert(con);
                long long done = now_us();
                Connection_close(con);
                Thread_join(t);
                assert(took >= 0);
                lat[i] = done > h.released ? done - h.released : 0;
        }
        long long max = 0, sum = 0;
        for (int i = 0; i < 10; i++) {
                sum += lat[i];
                if (lat[i] > max)
                        max = lat[i];
        }
        printf("\tcommit to waiter's success: mean %6.2f ms, max %6.2f ms\n", sum / 10 / 1000., max / 1000.);
        assert(max < 20000);
}


// ---------------------------------------------------------------- Test 2


static void timeout(void) {
        Hold h = {.times = 1, .hold = 1200};
        Thread_T t;
        Thread_create(t, holder, &h);
        sleep_ms(20);
        Connection_T con = ConnectionPool_getConnection(pool);
        long long t0 = now_us();
        long long took = insert(con);
        long long waited = now_us() - t0;
        Connection_close(con);
        Thread_join(t);
        printf("\twaiter failed after %6.1f ms (busy timeout %d ms)\n", waited / 1000., BUSY_TIMEOUT);
        assert(took == -1);
        assert(waited >= (BUSY_TIMEOUT - 20) * 1000LL && waited <= (BUSY_TIMEOUT + 200) * 1000LL);
}


// ---------------------------------------------------------------- Test 3


#define WRITERS 8
#define INSERTS 20

typedef struct Writer {
        int failed;
        long long longest; // µs
        atomic_bool *stop;
} Writer;


static void *writer(void *arg) {
        Writer *w = arg;
        Connection_T con = ConnectionPool_getConnection(pool);
        assert(con);
        unsigned seed = (unsigned)(uintptr_t)w;
        for (int i = 0; i < INSERTS && ! atomic_load(w->stop); i++) {
                long long took = insert(con);
                if (took < 0)
                        w->failed++;
                else if (took > w->longest)
                        w->longest = took;
                sleep_ms(rand_r(&seed) % 20);
        }
        Connection_close(con);
        return NULL;
}


static void chunked(void) {
        atomic_bool stop = false;
        Hold h = {.times = 40, .hold = 50, .pause = 2};
        Writer w[WRITERS] = {};
        Thread_T th, tw[WRITERS];
        Thread_create(th, holder, &h);
        sleep_ms(10);
        for (int i = 0; i < WRITERS; i++) {
                w[i].stop = &stop;
                Thread_create(tw[i], writer, &w[i]);
        }
        Thread_join(th);
        atomic_store(&stop, true);
        int failed = 0;
        long long longest = 0;
        for (int i = 0; i < WRITERS; i++) {
                Thread_join(tw[i]);
                failed += w[i].failed;
                if (w[i].longest > longest)
                        longest = w[i].longest;
        }
        printf("\t%d writers, %d inserts each: %3d failed (database is locked), longest successful wait %6.1f ms\n", WRITERS, INSERTS, failed, longest / 1000.);
        assert(failed == 0);
}


// ---------------------------------------------------------------- Test 4


static void otherProcess(void) {
        int fd[2];
        assert(pipe(fd) == 0);
        pid_t pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
                // The child: its own pool, its commit wakes up nobody in the parent
                close(fd[0]);
                pool = pool_new();
                Hold h = {.times = 1, .hold = 150};
                holder(&h);
                long long released = h.released;
                assert(write(fd[1], &released, sizeof(released)) == sizeof(released));
                close(fd[1]);
                pool_free(&pool);
                _exit(0);
        }
        close(fd[1]);
        sleep_ms(40); // the child has the lock
        Connection_T con = ConnectionPool_getConnection(pool);
        long long took = insert(con);
        long long done = now_us();
        Connection_close(con);
        long long released = 0;
        assert(read(fd[0], &released, sizeof(released)) == sizeof(released));
        close(fd[0]);
        int status;
        waitpid(pid, &status, 0);
        assert(took >= 0);
        long long lat = done > released ? done - released : 0;
        printf("\tcommit in another process to waiter's success: %6.2f ms\n", lat / 1000.);
        assert(lat < 30000);
}


// ---------------------------------------------------------------- Main


int main(void) {
        snprintf(db, sizeof(db), "/tmp/zdb-busy-%d.db", (int)getpid());
        printf("============> Start SQLite busy handler Tests (%s, busy_timeout %d ms)\n\n", db, BUSY_TIMEOUT);
        void (*tests[])(void) = {handoff, timeout, chunked, otherProcess};
        const char *names[] = {"Hand-off after a commit", "Busy timeout", "Writers next to a chunked writer (50 ms lock, 2 ms pause)", "Commit in another process"};
        for (int i = 0; i < 4; i++) {
                printf("=> Test%d: %s\n", i + 1, names[i]);
                pool = pool_new();
                Connection_T con = ConnectionPool_getConnection(pool);
                Connection_execute(con, "CREATE TABLE IF NOT EXISTS t (v INTEGER);");
                Connection_close(con);
                tests[i]();
                pool_free(&pool);
                printf("=> Test%d: OK\n\n", i + 1);
        }
        char f[300];
        unlink(db);
        snprintf(f, sizeof(f), "%s-wal", db); unlink(f);
        snprintf(f, sizeof(f), "%s-shm", db); unlink(f);
        printf("============> SQLite busy handler Tests: OK\n\n");
        return 0;
}
