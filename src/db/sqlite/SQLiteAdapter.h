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


#ifndef SQLITEDEFS_INCLUDED
#define SQLITEDEFS_INCLUDED

#include <sqlite3.h>

#include "zdb.h"


/// Busy state of a connection that waits with zdb_sqlite3_busy_handler():
/// the busy timeout, and the wait in progress
typedef struct ZdbBusy_T {
        int timeout;              // ms, the busy timeout
        long long started;        // µs, CLOCK_MONOTONIC, when the current wait started
        unsigned long long seen;  // the wake-up generation at the last retry
} ZdbBusy_T;

void zdb_sqlite3_busy_handler(sqlite3 *db, ZdbBusy_T *busy) __attribute__ ((visibility("hidden")));
int zdb_sqlite3_step(sqlite3_stmt *pStmt, int timeout) __attribute__ ((visibility("hidden")));
#if defined SQLITEUNLOCK && SQLITE_VERSION_NUMBER >= 3006012
int zdb_sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nSql, sqlite3_stmt **ppStmt, const char **pz, int timeout) __attribute__ ((visibility("hidden")));
#else
#define zdb_sqlite3_prepare_v2(db, zSql, nSql, ppStmt, pz, timeout) sqlite3_prepare_v2(db, zSql, nSql, ppStmt, pz)
#endif
int zdb_sqlite3_exec(sqlite3 *db, const char *sql, int timeout) __attribute__ ((visibility("hidden")));

ResultSetDelegate_T SQLiteResultSet_new(Connection_T delegator, sqlite3_stmt *stmt, int keep, int busyTimeout) __attribute__ ((visibility("hidden")));
PreparedStatementDelegate_T SQLitePreparedStatement_new(Connection_T delegator, sqlite3_stmt *stmt, int busyTimeout) __attribute__ ((visibility("hidden")));

#endif
