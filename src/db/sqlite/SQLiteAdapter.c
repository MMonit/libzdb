/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 */

#include "Config.h"

#include "Thread.h"
#include "system/Time.h"
#include "SQLiteAdapter.h"


#if defined SQLITEUNLOCK && SQLITE_VERSION_NUMBER >= 3006012

/*
 * SQLite unlock notify API
 * @see https://www.sqlite.org/unlock_notify.html
 */

typedef struct UnlockNotification {
        int fired;
        Sem_T cond;
        Mutex_T mutex;
} UnlockNotification_T;


static inline void unlock_notify_cb(void **apArg, int nArg) {
        for (int i = 0; i < nArg; i++) {
                UnlockNotification_T *p = (UnlockNotification_T *)apArg[i];
                Mutex_lock(p->mutex);
                p->fired = 1;
                Sem_signal(p->cond);
                Mutex_unlock(p->mutex);
        }
}


static inline int wait_for_unlock_notify(sqlite3 *db, long long deadline){
        UnlockNotification_T un;
        un.fired = 0;
        Mutex_init(un.mutex);
        Sem_init(un.cond);
        int rc = sqlite3_unlock_notify(db, unlock_notify_cb, (void *)&un);
        assert(rc == SQLITE_LOCKED || rc == SQLITE_OK);
        if (rc == SQLITE_OK) {
                struct timespec wait = {.tv_sec = deadline / 1000, .tv_nsec = (deadline % 1000) * 1000000};
                Mutex_lock(un.mutex);
                if (! un.fired)
                        Sem_timeWait(un.cond, un.mutex, wait);
                Mutex_unlock(un.mutex);
                sqlite3_unlock_notify(db, NULL, NULL);
        }
        Sem_destroy(un.cond);
        Mutex_destroy(un.mutex);
        return rc;
}


// MARK: - Blocking API

int zdb_sqlite3_step(sqlite3_stmt *pStmt, int timeout) {
        int rc;
        long long deadline = 0;
        while (SQLITE_LOCKED == (rc = sqlite3_step(pStmt))) {
                if (! deadline)
                        deadline = Time_milli() + timeout;
                else if (Time_milli() >= deadline)
                        break;
                rc = wait_for_unlock_notify(sqlite3_db_handle(pStmt), deadline);
                if (rc != SQLITE_OK)
                        break;
#if SQLITE_VERSION_NUMBER < 3070000 || defined SQLITE_OMIT_AUTORESET
                sqlite3_reset(pStmt);
#endif
        }
        return rc;
}


int zdb_sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nSql, sqlite3_stmt **ppStmt, const char **pz, int timeout) {
        int rc;
        long long deadline = 0;
        while (SQLITE_LOCKED == (rc = sqlite3_prepare_v2(db, zSql, nSql, ppStmt, pz))) {
                if (! deadline)
                        deadline = Time_milli() + timeout;
                else if (Time_milli() >= deadline)
                        break;
                rc = wait_for_unlock_notify(db, deadline);
                if (rc != SQLITE_OK)
                        break;
        }
        return rc;
}


#endif


int zdb_sqlite3_exec(sqlite3 *db, const char *sql, int timeout) {
        int rc = SQLITE_OK;
        const char *tail = sql;
        while (STR_DEF(tail) && rc == SQLITE_OK) {
                sqlite3_stmt *stmt = NULL;
                rc = zdb_sqlite3_prepare_v2(db, tail, -1, &stmt, &tail, timeout);
                if (rc != SQLITE_OK)
                        break;
                if (! stmt) // trailing whitespace or a comment-only fragment
                        continue;
                int step;
                do {
                        step = zdb_sqlite3_step(stmt, timeout);
                } while (step == SQLITE_ROW);
                sqlite3_finalize(stmt);
                if (step != SQLITE_DONE)
                        rc = step; // propagate the error (e.g. BUSY/LOCKED after retries)
        }
        return rc;
}
