#include "Config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#include "zdb.h"

#include "Thread.h"
#include "Vector.h"
#include "AssertException.h"


/**
 * libzdb connection pool unity tests.
 */
#define BSIZE 2048

#define SCHEMA_MYSQL      "CREATE TABLE zild_t(id INTEGER AUTO_INCREMENT PRIMARY KEY, name VARCHAR(255), percent REAL, image BLOB);"
#define SCHEMA_POSTGRESQL "CREATE TABLE zild_t(id SERIAL PRIMARY KEY, name VARCHAR(255), percent REAL, image BYTEA);"
#define SCHEMA_SQLITE     "CREATE TABLE zild_t(id INTEGER PRIMARY KEY, name VARCHAR(255), percent REAL, image BLOB);"
#define SCHEMA_ORACLE     "CREATE TABLE zild_t(id NUMBER GENERATED AS IDENTITY, name VARCHAR(255), percent REAL, image BLOB);"

#if HAVE_STRUCT_TM_TM_GMTOFF
#define TM_GMTOFF tm_gmtoff
#else
#define TM_GMTOFF tm_wday
#endif


static void TabortHandler(const char *error) {
        fprintf(stdout, "Error: %s\n", error);
        exit(1);
}

static void testPool(const char *testURL) {
        URL_T url;
        char *schema;
        ConnectionPool_T pool;
        char *data[]= {"Fry", "Leela", "Bender", "Farnsworth",
                "Zoidberg", "Amy", "Hermes", "Nibbler", "Cubert",
                "Zapp", "Joey Mousepad", "ЯΣ༆", 0}; 
        
        if (Str_startsWith(testURL, "mysql")) {
                schema = SCHEMA_MYSQL;
        } else if (Str_startsWith(testURL, "postgresql")) {
                schema = SCHEMA_POSTGRESQL;
        } else if (Str_startsWith(testURL, "sqlite")) {
                schema = SCHEMA_SQLITE;
        } else if (Str_startsWith(testURL, "oracle")) {
                schema = SCHEMA_ORACLE;
        }
        else {
                printf("Unsupported database protocol\n");
                exit(1);
        }

        
        printf("=> Test1: create/destroy\n");
        {
                pool = ConnectionPool_new(URL_new(testURL));
                assert(pool);
                url = ConnectionPool_getURL(pool);
                ConnectionPool_free(&pool);
                assert(! pool);
                URL_free(&url);
        }
        printf("=> Test1: OK\n\n");
        
        printf("=> Test2: NULL value\n");
        {
                url = URL_new(NULL);
                assert(! url);
                TRY
                {
                        pool = ConnectionPool_new(url);
                        printf("\tResult: Test failed -- exception not thrown\n");
                        exit(1);
                }
                CATCH(AssertException)
                {
                        // OK
                }
                END_TRY;
        }
        printf("=> Test2: OK\n\n");
        
        printf("=> Test3: start/stop\n");
        {
                url = URL_new(testURL);
                pool = ConnectionPool_new(url);
                assert(pool);
                ConnectionPool_setReaper(pool, 0); // disable reaper
                ConnectionPool_start(pool);
                assert(ConnectionPool_size(pool) == ConnectionPool_getInitialConnections(pool));
                ConnectionPool_stop(pool);
                ConnectionPool_free(&pool);
                assert(pool==NULL);
                URL_free(&url);
                // Regression: disabling the reaper AFTER start() must still join the reaper
                // thread on stop/free. Otherwise free() destroys the mutex/cond the reaper is
                // still waiting on (setReaper(0) only cleared doSweep, and stop() used that
                // stale flag to decide whether to join the thread).
                {
                        url = URL_new(testURL);
                        pool = ConnectionPool_new(url);
                        assert(pool);
                        ConnectionPool_start(pool);        // reaper thread started (sweep on by default)
                        ConnectionPool_setReaper(pool, 0); // disable reaper AFTER it was started
                        volatile int clean = 0;
                        TRY {
                                ConnectionPool_stop(pool);
                                ConnectionPool_free(&pool);
                                clean = 1;
                        } ELSE {
                                clean = 0; // buggy: destroying the mutex/cond the reaper waits on throws
                        } END_TRY;
                        assert(clean);
                        assert(pool == NULL);
                        URL_free(&url);
                }
                // Test that exception is thrown on start error
                TRY
                {
                        url = URL_new("not://a/database");
                        pool = ConnectionPool_new(url);
                        assert(pool);
                        ConnectionPool_start(pool);
                        printf("\tResult: Test failed -- exception not thrown\n");
                        exit(1);
                }
                CATCH(SQLException) {
                        // OK
                }
                FINALLY {
                        ConnectionPool_free(&pool);
                        assert(pool==NULL);
                        URL_free(&url);
                }
                END_TRY;
        }
        printf("=> Test3: OK\n\n");
        
        printf("=> Test4: Connection execute & transaction\n");
        {
                int i;
                Connection_T con;
                url = URL_new(testURL);
                pool = ConnectionPool_new(url);
                assert(pool);
                assert(ConnectionPool_getType(pool) > CONNECTIONPOOL_NONE);
                ConnectionPool_setReaper(pool, 0); // disable reaper
                ConnectionPool_setAbortHandler(pool, TabortHandler);
                ConnectionPool_start(pool);
                con = ConnectionPool_getConnection(pool);
                assert(con);
                TRY Connection_execute(con, "drop table zild_t;"); ELSE END_TRY;
                Connection_execute(con, "%s", schema);
                Connection_beginTransaction(con);
                /* Insert values into database and assume that auto increment of id works */
                for (i = 0; data[i]; i++)
                        Connection_execute(con, "insert into zild_t (name, percent) values('%s', %d.%d);", data[i], i+1, i);
                // Assert that the last insert statement added one row
                assert(Connection_rowsChanged(con) == 1);
                /* Assert that last row id works for MySQL and SQLite. Neither Oracle nor PostgreSQL
                 support last row id directly. The way to do this in PostgreSQL is to return the id
                 on insert and in libzdb execute the statement using executeQuery */
                if (IS(URL_getProtocol(url), "sqlite") || IS(URL_getProtocol(url), "mysql"))
                        assert(Connection_lastRowId(con) == 12);
                Connection_commit(con);
                printf("\tResult: table zild_t successfully created\n");
                // Regression (SQLite): if commit fails, the connection must stay marked
                // in-transaction so it is rolled back on close / return-to-pool. A deferred
                // foreign-key violation makes COMMIT fail while leaving the transaction open.
                // Before the fix Connection_commit() cleared the flag *before* calling the
                // delegate, so a failed commit left the connection looking idle and its
                // uncommitted transaction was never rolled back.
                if (Str_startsWith(testURL, "sqlite")) {
                        printf("\tResult: check failed commit keeps transaction open..");
                        Connection_execute(con, "PRAGMA foreign_keys = ON;");
                        Connection_execute(con, "drop table if exists fk_child;");
                        Connection_execute(con, "drop table if exists fk_parent;");
                        Connection_execute(con, "create table fk_parent(id integer primary key);");
                        Connection_execute(con, "create table fk_child(pid integer references fk_parent(id) deferrable initially deferred);");
                        Connection_beginTransaction(con);
                        Connection_execute(con, "insert into fk_child values(999);"); // no matching parent row
                        volatile int threw = 0, stillInTxn = -1;
                        TRY {
                                Connection_commit(con); // deferred FK check fails at COMMIT
                        } CATCH(SQLException) {
                                threw = 1;
                                stillInTxn = Connection_inTransaction(con);
                        } END_TRY;
                        assert(threw);              // commit did fail
                        assert(stillInTxn == true); // still in-transaction after the failed commit
                        Connection_rollback(con);   // and a rollback cleanly ends it
                        assert(! Connection_inTransaction(con));
                        Connection_execute(con, "drop table if exists fk_child;");
                        Connection_execute(con, "drop table if exists fk_parent;");
                        printf("success\n");
                }
                // Regression (PostgreSQL): a password containing a single quote must be
                // escaped in the libpq conninfo string, otherwise the connection string is
                // malformed (or a crafted value could inject conninfo parameters). Create a
                // role whose password contains a quote and connect as it. TRY-guarded, so it
                // is skipped where the test role cannot create roles.
                if (Str_startsWith(testURL, "postgres")) {
                        volatile int roleCreated = 0;
                        TRY {
                                Connection_execute(con, "drop role if exists zdb_quote;");
                                Connection_execute(con, "create role zdb_quote login password 'pa''ss';"); // password is pa'ss
                                roleCreated = 1;
                        } ELSE {
                                printf("\t(skipping quote-password test: cannot create role)\n");
                        } END_TRY;
                        if (roleCreated) {
                                printf("\tResult: check quote in password is escaped..");
                                char *qs = Str_cat("postgresql://%s:%d%s?user=zdb_quote&password=pa'ss",
                                                   URL_getHost(url), URL_getPort(url), URL_getPath(url));
                                URL_T qurl = URL_new(qs);
                                ConnectionPool_T qpool = ConnectionPool_new(qurl);
                                ConnectionPool_setReaper(qpool, 0);
                                ConnectionPool_start(qpool);
                                Connection_T qcon = ConnectionPool_getConnection(qpool);
                                assert(qcon); // buggy: connect fails because the quote breaks conninfo
                                ResultSet_T qr = Connection_executeQuery(qcon, "select 42;");
                                assert(ResultSet_next(qr));
                                assert(ResultSet_getInt(qr, 1) == 42);
                                Connection_close(qcon);
                                ConnectionPool_free(&qpool);
                                URL_free(&qurl);
                                FREE(qs);
                                Connection_execute(con, "drop role zdb_quote;");
                                printf("success\n");
                        }
                }
                Connection_close(con);
        }
        printf("=> Test4: OK\n\n");
        
        
        printf("=> Test5: Prepared Statement\n");
        {
                int i;
                char blob[8192];
                char *images[]= {"Ceci n'est pas une pipe", "Mona Lisa",
                        "Bryllup i Hardanger", "The Scream",
                        "Vampyre", "Balcony", "Cycle", "Day & Night",
                        "Hand with Reflecting Sphere",
                        "Drawing Hands", "Ascending and Descending", 0};
                Connection_T con = ConnectionPool_getConnection(pool);
                assert(con);
                // 1. Prepared statement, perform a nonsense update to test rowsChanged
                PreparedStatement_T p1 = Connection_prepareStatement(con, "update zild_t set image=?");
                // Use setBlob for BLOB columns - setString with non-hex fails on Oracle (ORA-01465)
                PreparedStatement_setBlob(p1, 1, "xxx", 3);
                PreparedStatement_execute(p1);
                printf("\tRows changed: %lld\n", PreparedStatement_rowsChanged(p1));
                // Assert that all 12 rows in the data set was changed
                assert(PreparedStatement_rowsChanged(p1) == 12);
                // 2. Prepared statement, update the table proper with "images".
                PreparedStatement_T pre = Connection_prepareStatement(con, "update zild_t set image=? where id=?");
                assert(pre);
                assert(PreparedStatement_getParameterCount(pre) == 2);
                for (i = 0; images[i]; i++) {
                        PreparedStatement_setBlob(pre, 1, images[i], (int)strlen(images[i])+1);
                        PreparedStatement_setInt(pre, 2, i + 1);
                        PreparedStatement_execute(pre);
                }
                /* Add a database null blob value for id = 5 */
                PreparedStatement_setNull(pre, 1);
                PreparedStatement_setInt(pre, 2, 5);
                PreparedStatement_execute(pre);
                /* Add a database null value for id = 1 */
                PreparedStatement_setNull(pre, 1);
                PreparedStatement_setInt(pre, 2, 1);
                PreparedStatement_execute(pre);
                /* Add a large blob */
                memset(blob, 'x', 8192);
                blob[8191] = 0;
                /* Mark start and end */
                *blob='S'; blob[8190] = 'E';
                PreparedStatement_setBlob(pre, 1, blob, 8192);
                PreparedStatement_setInt(pre, 2, i + 1);
                PreparedStatement_execute(pre);
                printf("\tResult: prepared statement successfully executed\n");
                Connection_close(con);
        }
        printf("=> Test5: OK\n\n");
        
        
        printf("=> Test6: Result Sets\n");
        {
                int i;
                int imagesize = 0;
                Connection_T con = ConnectionPool_getConnection(pool);
                assert(con);
                Connection_setQueryTimeout(con, 3000);
                assert(Connection_getQueryTimeout(con) == 3000);
                ResultSet_T rset = Connection_executeQuery(con, "select id, name, percent, image from zild_t where id < %d order by id;", 100);
                assert(rset);
                printf("\tResult:\n");
                printf("\tNumber of columns in resultset: %d\n\t", ResultSet_getColumnCount(rset));
                assert(4==ResultSet_getColumnCount(rset));

                // Regression test: out-of-range column index must return NULL, not read past the column array
                assert(ResultSet_getColumnName(rset, 0) == NULL);
                assert(ResultSet_getColumnName(rset, ResultSet_getColumnCount(rset) + 1) == NULL);

                i = 1;
                printf("%-5s", ResultSet_getColumnName(rset, i++));
                printf("%-16s", ResultSet_getColumnName(rset, i++));
                printf("%-10s", ResultSet_getColumnName(rset, i++));
                printf("%-16s", ResultSet_getColumnName(rset, i++));
                printf("\n\t------------------------------------------------------\n");
                while (ResultSet_next(rset)) {
                        int id = ResultSet_getIntByName(rset, "id");
                        const char *name = ResultSet_getString(rset, 2);
                        double percent = ResultSet_getDoubleByName(rset, "percent");
                        const char *blob = (char*)ResultSet_getBlob(rset, 4, &imagesize);
                        printf("\t%-5d%-16s%-10.2f%-16.38s\n", id, valueOr(name, "null"), percent, valueOr(blob, "null"));
                }
                // Column count
                rset = Connection_executeQuery(con, "select image from zild_t where id=12;");
                assert(1 == ResultSet_getColumnCount(rset));
                
                // Assert that types are interchangeable and that all data is returned
                while (ResultSet_next(rset)) {
                        const char *image = ResultSet_getStringByName(rset, "image");
                        const void *blob = ResultSet_getBlobByName(rset, "image", &imagesize);
                        // Oracle does not support getting blob as string
                        if (! Str_startsWith(testURL, "oracle")) {
                                assert(image && blob);
                                // getString on a BLOB returns the raw bytes (length 8191 up to the
                                // trailing NUL); on a PostgreSQL bytea it returns the escaped text
                                // form (e.g. "\x...."), which is longer than the raw 8192 bytes.
                                if (Str_startsWith(testURL, "postgres"))
                                        assert(strlen(image) > 8192);
                                else
                                        assert(strlen(image) + 1 == 8192);
                        }
                        assert(imagesize == 8192);
                }
                
                printf("\tResult: check isnull..");
                rset = Connection_executeQuery(con, "select id, image from zild_t where id in(1,5,2);");
                while (ResultSet_next(rset)) {
                        int id = ResultSet_getIntByName(rset, "id");
                        if (id == 1 || id == 5) {
                                assert(ResultSet_isnull(rset, 2) == true);
                                assert(ResultSet_isnullByName(rset, "image") == true);

                                // Regression test for SQLite: getDateTime()/getTimestamp() on a
                                // NULL column must not crash.
                                if (Str_startsWith(testURL, "sqlite")) {
                                        assert(ResultSet_getDateTime(rset, 2).tm_year == 0);
                                        assert(ResultSet_getTimestamp(rset, 2) == 0);
                                }
                        } else {
                                assert(ResultSet_isnull(rset, 2) == false);
                                assert(ResultSet_isnullByName(rset, "image") == false);
                        }
                }
                printf("success\n");

                // Regression (Oracle): ResultSet_getBlob() on a non-LOB column must
                // return the fetched value without corrupting the OCIDefineByPos
                // buffer (previously it RESIZE/FREE'd the define buffer and called
                // OCILobRead2() with a NULL locator -> heap corruption/use-after-free).
                if (Str_startsWith(testURL, "oracle")) {
                        printf("\tResult: check getBlob on non-LOB column..");
                        int bsize = 0;
                        rset = Connection_executeQuery(con, "select name from zild_t where name = 'Leela';");
                        assert(ResultSet_next(rset));
                        const char *nb = (const char*)ResultSet_getBlob(rset, 1, &bsize);
                        assert(nb && bsize == 5); // "Leela" returned as raw bytes
                        assert(memcmp(nb, "Leela", 5) == 0);
                        printf("success\n");
                }

                // Regression (SQLite): a failing bind must raise an exception, not be
                // silently swallowed and then executed with the parameter left unbound
                // (silent data loss). A bind size exceeding SQLITE_LIMIT_LENGTH (default
                // 1e9) yields SQLITE_TOOBIG; SQLite validates the length before touching
                // the buffer, so a small buffer with an oversized size triggers it cheaply.
                if (Str_startsWith(testURL, "sqlite")) {
                        printf("\tResult: check bind error is not swallowed..");
                        Connection_execute(con, "drop table if exists toobig_t;");
                        Connection_execute(con, "create table toobig_t(x text);");
                        PreparedStatement_T pt = Connection_prepareStatement(con, "insert into toobig_t values (?);");
                        char smallbuf[16] = "xxxx";
                        volatile int threw = 0;
                        TRY {
                                PreparedStatement_setSString(pt, 1, smallbuf, 1500000000); // > SQLITE_LIMIT_LENGTH
                                PreparedStatement_execute(pt);
                        } CATCH(SQLException) {
                                threw = 1;
                        } END_TRY;
                        assert(threw); // before the fix no exception was raised
                        // ... and no row was silently inserted with a NULL x
                        rset = Connection_executeQuery(con, "select count(*) from toobig_t;");
                        assert(ResultSet_next(rset));
                        assert(ResultSet_getInt(rset, 1) == 0);
                        Connection_execute(con, "drop table if exists toobig_t;");
                        printf("success\n");
                }

                // Regression (SQLite): empty or comment-only SQL makes sqlite3_prepare_v2
                // return SQLITE_OK with a NULL statement. This must raise a clean
                // SQLException, not an AssertException (or a NULL-deref in NDEBUG builds).
                if (Str_startsWith(testURL, "sqlite")) {
                        printf("\tResult: check empty/comment-only SQL raises SQLException..");
                        volatile int threwQ = 0, threwP = 0;
                        TRY { Connection_executeQuery(con, "-- just a comment"); }
                        CATCH(SQLException) { threwQ = 1; } END_TRY;
                        TRY { Connection_prepareStatement(con, "   "); }
                        CATCH(SQLException) { threwP = 1; } END_TRY;
                        assert(threwQ && threwP); // buggy: AssertException instead
                        printf("success\n");
                }

                // Regression (SQLite): a multi-statement Connection_execute() runs each
                // statement exactly once. (The BUSY/LOCKED retry now re-runs only the
                // failing statement, not the whole string, so already-committed
                // statements are never repeated.)
                if (Str_startsWith(testURL, "sqlite")) {
                        printf("\tResult: check multi-statement execute..");
                        Connection_execute(con, "drop table if exists multi_t;");
                        Connection_execute(con, "create table multi_t(n int);");
                        Connection_execute(con, "insert into multi_t values (1); insert into multi_t values (2);");
                        rset = Connection_executeQuery(con, "select count(*) from multi_t;");
                        assert(ResultSet_next(rset));
                        assert(ResultSet_getInt(rset, 1) == 2); // each insert ran once
                        Connection_execute(con, "drop table if exists multi_t;");
                        printf("success\n");
                }

                // Regression (PostgreSQL): getBlob() must decode the bytea into an owned
                // buffer, not mutate the shared PGresult in place. Otherwise a second
                // getBlob() on the same cell re-decodes already-decoded bytes, and a
                // getString() afterwards returns the mutated binary. The raw bytes
                // 0x5C 0x78 0x30 0x30 (the text "\x00") make an in-place re-decode mangle
                // the value, so this catches the regression.
                if (Str_startsWith(testURL, "postgres")) {
                        printf("\tResult: check bytea getBlob is not decoded in place..");
                        Connection_execute(con, "drop table if exists bytea_t;");
                        Connection_execute(con, "create table bytea_t(x bytea);");
                        Connection_execute(con, "insert into bytea_t values (decode('5c783030','hex'));");
                        ResultSet_T br = Connection_executeQuery(con, "select x from bytea_t;");
                        assert(ResultSet_next(br));
                        int n1 = 0, n2 = 0;
                        const unsigned char *b1 = ResultSet_getBlob(br, 1, &n1);
                        unsigned char saved[8];
                        assert(n1 == 4);
                        memcpy(saved, b1, n1);
                        assert(memcmp(saved, "\x5c\x78\x30\x30", 4) == 0);
                        // Repeated getBlob on the same cell must return the same bytes
                        const unsigned char *b2 = ResultSet_getBlob(br, 1, &n2);
                        assert(n2 == 4 && memcmp(b2, saved, 4) == 0);
                        // getString on the same column must still return the server's text
                        const char *bs = ResultSet_getString(br, 1);
                        assert(bs && bs[0] == '\\' && bs[1] == 'x');
                        Connection_execute(con, "drop table if exists bytea_t;");
                        printf("success\n");
                }

                // Regression (PostgreSQL): setSString() must honor the caller-supplied
                // length. libpq ignores paramLengths for text-format params and reads to
                // the NUL, so before the fix the whole string was sent; the value is now
                // bound in binary format with the given length.
                if (Str_startsWith(testURL, "postgres")) {
                        printf("\tResult: check setSString honors length..");
                        Connection_execute(con, "drop table if exists sstr_t;");
                        Connection_execute(con, "create table sstr_t(x varchar(64));");
                        PreparedStatement_T ps = Connection_prepareStatement(con, "insert into sstr_t values (?);");
                        PreparedStatement_setSString(ps, 1, "hello world", 5); // only the first 5 chars
                        PreparedStatement_execute(ps);
                        ResultSet_T sr = Connection_executeQuery(con, "select x from sstr_t;");
                        assert(ResultSet_next(sr));
                        assert(IS(ResultSet_getString(sr, 1), "hello")); // before the fix: "hello world"
                        Connection_execute(con, "drop table if exists sstr_t;");
                        printf("success\n");
                }

                printf("\tResult: check max rows..");
                Connection_setMaxRows(con, 3);
                rset = Connection_executeQuery(con, "select id from zild_t;");
                assert(rset);
                i = 0;
                while (ResultSet_next(rset)) i++;
                assert((i)==3);
                printf("success\n");
                
                printf("\tResult: check prepared statement resultset..");
                Connection_setMaxRows(con, 0);
                PreparedStatement_T pre = Connection_prepareStatement(con, "select name from zild_t where id=?");
                assert(pre);
                PreparedStatement_setInt(pre, 1, 2);
                ResultSet_T names = PreparedStatement_executeQuery(pre);
                assert(names);
                assert(ResultSet_next(names));
                assert(Str_isEqual("Leela", ResultSet_getString(names, 1)));
                printf("success\n");
                
                printf("\tResult: check prepared statement re-execute..");
                PreparedStatement_setInt(pre, 1, 1);
                names = PreparedStatement_executeQuery(pre);
                assert(names);
                assert(ResultSet_next(names));
                assert(Str_isEqual("Fry", ResultSet_getString(names, 1)));
                printf("success\n");
                
                printf("\tResult: check prepared statement without in-params..");
                pre = Connection_prepareStatement(con, "select name from zild_t;");
                assert(pre);
                names = PreparedStatement_executeQuery(pre);
                assert(names);
                for (i = 0; ResultSet_next(names); i++);
                assert(i==12);
                printf("success\n");
                
                // Test prefetch unless database is SQLite or Postgres for which prefetch is n/a
                if (Str_startsWith(testURL, "mysql") || Str_startsWith(testURL, "oracle")) {
                        printf("\tResult: check fetch-size..");
                        assert(Connection_getFetchSize(con) == SQL_DEFAULT_PREFETCH_ROWS);
                        Connection_setFetchSize(con, 50);
                        assert(Connection_getFetchSize(con) == 50);
                        ResultSet_T fs = Connection_executeQuery(con, "select * from zild_t;");
                        assert(fs);
                        // Assert that result set inherits Connection fetch-size
                        assert(ResultSet_getFetchSize(fs) == 50);
                        ResultSet_setFetchSize(fs, 12);
                        assert(ResultSet_getFetchSize(fs) == 12);
                        // Iterate with a small fetch-size (prefetch) active and confirm
                        // every row is returned. Exercises _setFetchSize, whose prefetch
                        // count must be handed to mysql as an unsigned long, not an int.
                        int fetched = 0;
                        while (ResultSet_next(fs)) fetched++;
                        assert(fetched == 12);
                        printf("success\n");
                }

                // Regression (MySQL): beginTransactionType() with a non-default isolation
                // level sends "SET TRANSACTION ...; START TRANSACTION;" as two statements.
                // With CLIENT_MULTI_STATEMENTS enabled, both results must be drained or the
                // next command on the connection fails with CR_COMMANDS_OUT_OF_SYNC.
                if (Str_startsWith(testURL, "mysql")) {
                        printf("\tResult: check multi-statement transaction is drained..");
                        volatile int ok = 0;
                        TRY {
                                Connection_beginTransactionType(con, TRANSACTION_READ_COMMITTED);
                                ResultSet_T r = Connection_executeQuery(con, "select count(*) from zild_t;");
                                assert(ResultSet_next(r));
                                assert(ResultSet_getInt(r, 1) == 12);
                                Connection_commit(con);
                                ok = 1;
                        } CATCH(SQLException) {
                                ok = 0; // buggy: the next command throws CR_COMMANDS_OUT_OF_SYNC
                        } END_TRY;
                        assert(ok);
                        printf("success\n");
                }

                // Regression (MySQL): a non-NULL zero-length blob must be stored as an
                // empty blob, not SQL NULL (consistent with setString() and PostgreSQL).
                if (Str_startsWith(testURL, "mysql")) {
                        printf("\tResult: check empty blob is not stored as NULL..");
                        Connection_execute(con, "drop table if exists eblob_t;");
                        Connection_execute(con, "create table eblob_t(id int, b blob);");
                        PreparedStatement_T p = Connection_prepareStatement(con, "insert into eblob_t values (?, ?);");
                        PreparedStatement_setInt(p, 1, 1);
                        char dummy[1] = {0};
                        PreparedStatement_setBlob(p, 2, dummy, 0); // non-NULL pointer, zero length
                        PreparedStatement_execute(p);
                        ResultSet_T r = Connection_executeQuery(con, "select b from eblob_t where id = 1;");
                        assert(ResultSet_next(r));
                        assert(ResultSet_isnull(r, 1) == false); // before the fix: stored as NULL
                        int bsize = -1;
                        ResultSet_getBlob(r, 1, &bsize);
                        assert(bsize == 0); // empty blob
                        Connection_execute(con, "drop table if exists eblob_t;");
                        printf("success\n");
                }

                /* Need to close and release statements before
                   we can drop the table, sqlite need this */
                Connection_clear(con);
                Connection_execute(con, "drop table zild_t;");
                Connection_close(con);
                ConnectionPool_stop(pool);
                ConnectionPool_free(&pool);
                assert(pool==NULL);
                URL_free(&url);
        }
        printf("=> Test6: OK\n\n");
        
        
        printf("=> Test7: reaper start/stop\n");
        {
                int i;
                Vector_T v = Vector_new(20);
                url = URL_new(testURL);
                pool = ConnectionPool_new(url);
                assert(pool);
                ConnectionPool_setInitialConnections(pool, 4);
                ConnectionPool_setMaxConnections(pool, 20);
                ConnectionPool_setConnectionTimeout(pool, 2);
                ConnectionPool_setReaper(pool, 2);
                ConnectionPool_setAbortHandler(pool, TabortHandler);
                ConnectionPool_start(pool);
                assert(4==ConnectionPool_size(pool));
                printf("Creating 20 Connections..");
                for (i = 0; i<20; i++) {
                        Connection_T con = ConnectionPool_getConnection(pool);
                        assert(con);
                        Vector_push(v, con);
                        if (i < 19)
                                assert(!ConnectionPool_isFull(pool));
                }
                assert(ConnectionPool_size(pool) == 20);
                assert(ConnectionPool_active(pool) == 20);
                assert(ConnectionPool_isFull(pool));
                printf("success\n");
                printf("Closing Connections down to initial..");
                while (! Vector_isEmpty(v))
                        Connection_close(Vector_pop(v));
                assert(ConnectionPool_active(pool) == 0);
                assert(ConnectionPool_size(pool) == 20);
                printf("success\n");
                printf("Please wait 5 sec for reaper to harvest closed connections..");
                Connection_T con = ConnectionPool_getConnection(pool); // Activate one connection to verify the reaper does not close any active
                fflush(stdout);
                sleep(5);
                assert(5 == ConnectionPool_size(pool)); // 4 initial connections + the one active we got above
                assert(1 == ConnectionPool_active(pool));
                assert(!ConnectionPool_isFull(pool));
                printf("success\n");
                Connection_close(con);
                assert(0 == ConnectionPool_active(pool));
                ConnectionPool_stop(pool);
                ConnectionPool_free(&pool);
                Vector_free(&v);
                assert(pool==NULL);
                URL_free(&url);
        }
        printf("=> Test7: OK\n\n");

        printf("=> Test8: Exceptions handling\n");
        {
                Connection_T con;
                ResultSet_T result;
                url = URL_new(testURL);
                pool = ConnectionPool_new(url);
                assert(pool);
                ConnectionPool_setReaper(pool, 0); // disable reaper
                ConnectionPool_setAbortHandler(pool, TabortHandler);
                ConnectionPool_start(pool);
                con = ConnectionPool_getConnection(pool);
                assert(con);
                /*
                 * The following should work without throwing exceptions
                 */
                TRY
                {
                        Connection_execute(con, "%s", schema);
                }
                ELSE
                {
                        printf("\tResult: Creating table zild_t failed -- %s\n", Exception_frame.message);
                        assert(false); // Should not fail
                }
                END_TRY;
                TRY
                {
                        Connection_beginTransaction(con);
                        for (int i = 0; data[i]; i++)
                                Connection_execute(con, "insert into zild_t (name, percent) values('%s', %d.%d);", data[i], i+1, i);
                        Connection_commit(con);
                        printf("\tResult: table zild_t successfully created\n");
                }
                ELSE
                {
                        printf("\tResult: Test failed -- %s\n", Exception_frame.message);
                        assert(false); // Should not fail
                }
                FINALLY
                {
                        Connection_close(con);
                }
                END_TRY;
                assert((con = ConnectionPool_getConnection(pool)));
                TRY
                {
                        const char *bg[]= {"Starbuck", "Sharon Valerii",
                                "Number Six", "Gaius Baltar", "William Adama",
                                "Lee \"Apollo\" Adama", "Laura Roslin", 0};
                        PreparedStatement_T p = Connection_prepareStatement
                        (con, "insert into zild_t (name) values(?);");
                        /* If we did not get a statement, an SQLException is thrown
                           and we will not get here. So we can safely use the
                           statement now. Likewise, below, we do not have to
                           check return values from the statement since any error
                           will throw an SQLException and transfer the control
                           to the exception handler
                        */
                        int i, j;
                        for (i = 0, j = 42; bg[i]; i++, j++) {
                                PreparedStatement_setString(p, 1, bg[i]);
                                PreparedStatement_execute(p);
                        }
                }
                CATCH(SQLException)
                {
                        printf("\tResult: prepare statement failed -- %s\n", Exception_frame.message);
                        assert(false); // Should not fail
                }
                END_TRY;
                TRY
                {
                        printf("\t\tBattlestar Galactica: \n");
                        result = Connection_executeQuery(con, "select name from zild_t where id > 12;");
                        while (ResultSet_next(result))
                                printf("\t\t%s\n", ResultSet_getString(result, 1));
                }
                CATCH(SQLException)
                {
                        printf("\tResult: resultset failed -- %s\n", Exception_frame.message);
                       assert(false);
                }
                FINALLY
                {
                        Connection_close(con);
                }
                END_TRY;
                /*
                 * The following should fail and throw exceptions. The exception error
                 * message can be obtained with Exception_frame.message, or from
                 * Connection_getLastError(con). Exception_frame.message contains both
                 * SQL errors or api errors such as prepared statement parameter index
                 * out of range, while Connection_getLastError(con) only has SQL errors
                 */
                TRY
                {
                        assert((con = ConnectionPool_getConnection(pool)));
                        Connection_execute(con, "%s", schema);
                        /* Creating the table again should fail and we
                        should not come here */
                        printf("\tResult: Test failed -- exception not thrown\n");
                        exit(1);
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode != 0);
                        if (ConnectionPool_getType(pool) == CONNECTIONPOOL_POSTGRESQL) {
                                char* sqlstate = SQLState_toString(Exception_frame.errorCode, (char[6]){});
                                assert(STR_DEF(sqlstate));
                                DEBUG("\t(SQLSTATE code = %s)", sqlstate);
                        } else {
                                DEBUG("\t(errorCode = %d)", Exception_frame.errorCode);
                        }
                        printf("\n");
                        Connection_close(con);
                }
                END_TRY;
                TRY
                {
                        assert((con = ConnectionPool_getConnection(pool)));
                        printf("\tTesting: Query with errors.. ");
                        Connection_executeQuery(con, "blablabala;");
                        printf("\tResult: Test failed -- exception not thrown\n");
                        exit(1);
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode != 0);
                        printf("ok");
                        if (ConnectionPool_getType(pool) == CONNECTIONPOOL_POSTGRESQL) {
                                char* sqlstate = SQLState_toString(Exception_frame.errorCode, (char[6]){});
                                assert(STR_DEF(sqlstate));
                                DEBUG("\t(SQLSTATE code = %s)", sqlstate);
                        } else {
                                DEBUG("\t(errorCode = %d)", Exception_frame.errorCode);
                        }
                        printf("\n");
                        Connection_close(con);
                }
                END_TRY;
                TRY
                {
                        printf("\tTesting: Prepared statement query with errors.. ");
                        assert((con = ConnectionPool_getConnection(pool)));
                        PreparedStatement_T p = Connection_prepareStatement(con, "blablabala;");
                        ResultSet_T r = PreparedStatement_executeQuery(p);
                        while(ResultSet_next(r));
                        printf("\tResult: Test failed -- exception not thrown\n");
                        exit(1);
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode != 0);
                        printf("ok");
                        if (ConnectionPool_getType(pool) == CONNECTIONPOOL_POSTGRESQL) {
                                char* sqlstate = SQLState_toString(Exception_frame.errorCode, (char[6]){});
                                assert(STR_DEF(sqlstate));
                                DEBUG("\t(SQLSTATE code = %s)", sqlstate);
                        } else {
                                DEBUG("\t(errorCode = %d)", Exception_frame.errorCode);
                        }
                        printf("\n");
                        Connection_close(con);
                }
                END_TRY;
                TRY
                {
                        assert((con = ConnectionPool_getConnection(pool)));
                        printf("\tTesting: Column index out of range.. ");
                        result = Connection_executeQuery(con, "select id, name from zild_t;");
                        while (ResultSet_next(result)) {
                                int id = ResultSet_getInt(result, 1);
                                const char *name = ResultSet_getString(result, 2);
                                /* So far so good, now, try access an invalid
                                   column, which should throw an SQLException */
                                int bogus = ResultSet_getInt(result, 3);
                                printf("\tResult: Test failed -- exception not thrown\n");
                                printf("%d, %s, %d", id, name, bogus);
                                exit(1);
                        }
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode == 0); // API error, not a database error
                        printf("ok\n");
                        Connection_close(con);
                }
                END_TRY;
                TRY
                {
                        assert((con = ConnectionPool_getConnection(pool)));
                        printf("\tTesting: Invalid column name.. ");
                        result = Connection_executeQuery(con, "select name from zild_t;");
                        while (ResultSet_next(result)) {
                                const char *name = ResultSet_getStringByName(result, "nonexistingcolumnname");
                                printf("%s", name);
                                printf("\tResult: Test failed -- exception not thrown\n");
                                exit(1);
                        }
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode == 0); // API error, not a database error
                        printf("ok\n");
                        Connection_close(con);
                }
                END_TRY;
                TRY
                {
                        assert((con = ConnectionPool_getConnection(pool)));
                        PreparedStatement_T p = Connection_prepareStatement(con, "update zild_t set name = ? where id = ?;");
                        printf("\tTesting: Parameter index out of range.. ");
                        PreparedStatement_setInt(p, 3, 123);
                        printf("\tResult: Test failed -- exception not thrown\n");
                        exit(1);
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode == 0); // API error, not a database error
                        printf("ok\n");
                }
                FINALLY
                {
                        Connection_close(con);
                }
                END_TRY;
                TRY
                {
                        assert((con = ConnectionPool_getConnection(pool)));
                        printf("\tTesting: select from non-existing table.. ");
                        result = Connection_executeQuery(con, "select name from X;");
                        while (ResultSet_next(result)) {
                                const char *name = ResultSet_getStringByName(result, "nonexistingcolumnname");
                                printf("%s", name);
                                printf("\tResult: Test failed -- exception not thrown\n");
                                exit(1);
                        }
                }
                CATCH(SQLException)
                {
                        assert(Exception_frame.errorCode != 0);
                        printf("ok");
                        if (ConnectionPool_getType(pool) == CONNECTIONPOOL_POSTGRESQL) {
                                char* sqlstate = SQLState_toString(Exception_frame.errorCode, (char[6]){});
                                assert(STR_DEF(sqlstate));
                                DEBUG("\t(SQLSTATE code = %s)", sqlstate);
                        } else {
                                DEBUG("\t(errorCode = %d)", Exception_frame.errorCode);
                        }
                        printf("\n");
                        Connection_close(con);
                }
                END_TRY;
                assert((con = ConnectionPool_getConnection(pool)));
                Connection_execute(con, "drop table zild_t;");
                Connection_close(con);
                ConnectionPool_stop(pool);
                ConnectionPool_free(&pool);
                assert(pool==NULL);
                URL_free(&url);
        }
        printf("=> Test8: OK\n\n");
        
        printf("=> Test9: Ensure Capacity\n");
        {
                /* Check that MySQL ensureCapacity works for columns that exceed the preallocated buffer and that no truncation is done */
                if ( Str_startsWith(testURL, "mysql")) {
                        int myimagesize;
                        url = URL_new(testURL);
                        pool = ConnectionPool_new(url);
                        assert(pool);
                        ConnectionPool_setReaper(pool, 0); // disable reaper
                        ConnectionPool_start(pool);
                        Connection_T con = ConnectionPool_getConnection(pool);
                        assert(con);
                        Connection_execute(con, "CREATE TABLE zild_t(id INTEGER AUTO_INCREMENT PRIMARY KEY, image BLOB, string TEXT);");
                        PreparedStatement_T p = Connection_prepareStatement(con, "insert into zild_t (image, string) values(?, ?);");
                        char t[4096];
                        memset(t, 'x', 4096);
                        t[4095] = 0;
                        for (int i = 0; i < 4; i++) {
                                // store successive larger string-blobs to trigger realloc on ResultSet_getBlobByName;
                                // default buffer size is STRLEN and we only realloc more when needed
                                PreparedStatement_setBlob(p, 1, t, (i+1)*512);
                                PreparedStatement_setString(p, 2, t);
                                PreparedStatement_execute(p);
                        }
                        ResultSet_T r = Connection_executeQuery(con, "select image, string from zild_t;");
                        for (int i = 0; ResultSet_next(r); i++) {
                                ResultSet_getBlobByName(r, "image", &myimagesize);
                                const char *image = ResultSet_getStringByName(r, "image"); // Blob as String is NUL terminated
                                const char *string = ResultSet_getStringByName(r, "string");
                                assert(myimagesize == (i+1)*512);
                                assert(strlen(image) == (size_t)((i+1)*512));
                                assert(strlen(string) == 4095);
                        }
                        p = Connection_prepareStatement(con, "select image, string from zild_t;");
                        r = PreparedStatement_executeQuery(p);
                        for (int i = 0; ResultSet_next(r); i++) {
                                ResultSet_getBlobByName(r, "image", &myimagesize);
                                const char *image = ResultSet_getStringByName(r, "image");
                                const char *string = (char*)ResultSet_getStringByName(r, "string");
                                assert(myimagesize == (i+1)*512);
                                assert(strlen(image) == (size_t)((i+1)*512));
                                assert(strlen(string) == 4095);
                        }
                        Connection_execute(con, "drop table zild_t;");
                        Connection_close(con);
                        ConnectionPool_stop(pool);
                        ConnectionPool_free(&pool);
                        URL_free(&url);
                }
        }
        printf("=> Test9: OK\n\n");
        
        printf("=> Test10: Date, Time, DateTime and Timestamp\n");
        {
                url = URL_new(testURL);
                pool = ConnectionPool_new(url);
                assert(pool);
                ConnectionPool_setReaper(pool, 0); // disable reaper
                ConnectionPool_start(pool);
                Connection_T con = ConnectionPool_getConnection(pool);
                if (Str_startsWith(testURL, "postgres"))
                        Connection_execute(con, "create table zild_t(d date, t time, dt timestamp, ts timestamp)");
                else if (Str_startsWith(testURL, "oracle"))
                        Connection_execute(con, "create table zild_t(d date, t date, dt date, ts timestamp)");
                else
                        Connection_execute(con, "create table zild_t(d date, t time, dt datetime, ts timestamp);");
                PreparedStatement_T p = Connection_prepareStatement(con, "insert into zild_t values(?, ?, ?, ?);");
                if (Str_startsWith(testURL, "oracle")) { // Oracle does not have a pure time data type
                        Connection_execute(con, "alter session set nls_date_format='YYYY-MM-DD HH24:MI:SS';");
                        Connection_execute(con, "alter session set nls_timestamp_format='YYYY-MM-DD HH24:MI:SS';");
                        PreparedStatement_setString(p, 1, "2013-12-28 00:00:00");
                        PreparedStatement_setString(p, 2, "2013-12-28 10:12:42");
                } else {
                        PreparedStatement_setString(p, 1, "2013-12-28");
                        PreparedStatement_setString(p, 2, "10:12:42");
                }
                PreparedStatement_setString(p, 3, "2013-12-28 10:12:42");
                PreparedStatement_setTimestamp(p, 4, 1388225562);
                PreparedStatement_execute(p);
                ResultSet_T r = Connection_executeQuery(con, "select * from zild_t");
                if (ResultSet_next(r)) {
                        struct tm date = ResultSet_getDateTime(r, 1);
                        struct tm time = ResultSet_getDateTime(r, 2);
                        struct tm datetime = ResultSet_getDateTime(r, 3);
                        time_t timestamp = ResultSet_getTimestamp(r, 4);
                        struct tm timestampAsTm = ResultSet_getDateTime(r, 4);
                        // Check Date
                        assert(date.tm_hour == 0);
                        assert(date.tm_year == 2013);
                        assert(date.tm_mon == 11); // Remember month - 1
                        assert(date.tm_mday == 28);
                        assert(date.TM_GMTOFF == 0);
                        // Check Time
                        if (! Str_startsWith(testURL, "oracle"))
                                assert(time.tm_year == 0);
                        assert(time.tm_hour == 10);
                        assert(time.tm_min == 12);
                        assert(time.tm_sec == 42);
                        assert(time.TM_GMTOFF == 0);
                        // Check datetime
                        assert(datetime.tm_year == 2013);
                        assert(datetime.tm_mon == 11); // Remember month - 1
                        assert(datetime.tm_mday == 28);
                        assert(datetime.tm_hour == 10);
                        assert(datetime.tm_min == 12);
                        assert(datetime.tm_sec == 42);
                        assert(datetime.TM_GMTOFF == 0);
                        // Check timestamp
                        assert(timestamp == 1388225562);
                        // Check timestamp as datetime
                        assert(timestampAsTm.tm_year == 2013);
                        assert(timestampAsTm.tm_mon == 11); // Remember month - 1
                        assert(timestampAsTm.tm_mday == 28);
                        assert(timestampAsTm.tm_hour == 10);
                        assert(timestampAsTm.tm_min == 12);
                        assert(timestampAsTm.tm_sec == 42);
                        assert(timestampAsTm.TM_GMTOFF == 0);
                        // Result
                        printf("\tDate: %s, Time: %s, DateTime: %s\n\tTimestamp as numeric: %lld, Timestamp as string: %s\n",
                               ResultSet_getString(r, 1),
                               ResultSet_getString(r, 2),
                               ResultSet_getString(r, 3),
                               (long long)ResultSet_getTimestamp(r, 4),
                               ResultSet_getString(r, 4)); // SQLite will show both as numeric
                }
                // Regression: a calendar date near a year boundary must report the
                // correct calendar year. 2024-12-30 falls in ISO week 1 of 2025, so
                // the Oracle date-to-string format must use YYYY (calendar year), not
                // IYYY (ISO week-numbering year), which would report 2025.
                Connection_execute(con, "delete from zild_t;");
                PreparedStatement_T pb = Connection_prepareStatement(con, "insert into zild_t (d) values (?);");
                if (Str_startsWith(testURL, "oracle"))
                        PreparedStatement_setString(pb, 1, "2024-12-30 00:00:00");
                else
                        PreparedStatement_setString(pb, 1, "2024-12-30");
                PreparedStatement_execute(pb);
                ResultSet_T rb = Connection_executeQuery(con, "select d from zild_t");
                if (ResultSet_next(rb)) {
                        struct tm bd = ResultSet_getDateTime(rb, 1);
                        assert(bd.tm_year == 2024); // IYYY (ISO year) would report 2025
                        assert(bd.tm_mon == 11);    // December (month - 1)
                        assert(bd.tm_mday == 30);
                        // Oracle: getString on a date column must use ':' as the time
                        // separator (canonical "YYYY-MM-DD HH:MM:SS"), not '.'.
                        if (Str_startsWith(testURL, "oracle")) {
                                const char *ds = ResultSet_getString(rb, 1);
                                assert(ds && strchr(ds, ':') && !strchr(ds, '.'));
                        }
                }
                Connection_execute(con, "drop table zild_t;");
                Connection_close(con);
                ConnectionPool_stop(pool);
                ConnectionPool_free(&pool);
                assert(pool==NULL);
                URL_free(&url);
        }
        printf("=> Test10: OK\n\n");


        printf("============> Connection Pool Tests: OK\n\n");
}

int main(void) {
        URL_T url;
        char buf[BSIZE];
        char *help = "Please enter a valid database connection URL and press ENTER\n"
                    "E.g. sqlite:///tmp/sqlite.db?synchronous=normal\n"
                    "E.g. mysql://localhost:3306/test?user=root&password=root\n"
                    "E.g. postgresql://localhost:5432/test?user=root&password=root\n"
                    "E.g. oracle://scott:tiger@localhost:1521/servicename\n"
                    "To exit, enter '.' on a single line\n\nConnection URL> ";
        ZBDEBUG = true;
        Exception_init();
        printf("============> Start Connection Pool Tests\n\n");
        printf("This test will create and drop a table called zild_t in the database\n");
        printf("%s", help);
        while (fgets(buf, BSIZE, stdin)) {
                if (*buf == '.' || *buf == 'q')
                        break;
                if (*buf == '\r' || *buf == '\n' || *buf == 0)
                        goto next;
                url = URL_new(buf);
                if (! url) {
                        printf("Please enter a valid database URL or stop by entering '.'\n");
                        goto next;
                }
                testPool(URL_toString(url));
                URL_free(&url);
                printf("%s", help);
                continue;
next:
                printf("Connection URL> ");
        }
        return 0;
}
