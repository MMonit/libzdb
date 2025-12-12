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

#include <stdio.h>

#include "StringBuffer.h"
#include "SQLiteAdapter.h"
#include "ConnectionDelegate.h"


/**
 * Implementation of the Connection/Delegate interface for SQLite
 *
 * @file
 */


/* ------------------------------------------------------------- Definitions */


#define T ConnectionDelegate_T
struct T {
        sqlite3 *db;
        int maxRows;
        int lastError;
        StringBuffer_T sb;
        Connection_T delegator;
};
static int kQueryTimeoutDelta = 5;
extern const struct Rop_T sqlite3rops;
extern const struct Pop_T sqlite3pops;


/* --------------------------------------------------------- Private methods */


// Return options for the database connection
static int _options(URL_T url) {
        int options = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        // Specify the connection's threading mode. Multi-thread
        // is the default, setting URL parameter 'serialized=true'
        // enable  the "serialized" threading mode instead
        // @see https://www.sqlite.org/threadsafe.html
        if (Str_parseBool(URL_getParameter(url, "serialized"))) {
                options |= SQLITE_OPEN_FULLMUTEX;
        } else {
                options |= SQLITE_OPEN_NOMUTEX;
        }
        /*
         * If ENABLE_SQLITE_SHARED_CACHE is defined during compilation,
         * shared-cache mode is always enabled regardless of the URL parameter.
         * This is specifically used for builds where shared-cache must always
         * be on to ensure consistent behavior and prevent database lock issues.
         * When not defined, the 'shared-cache' URL parameter controls this setting.
         */
#if ENABLE_SQLITE_SHARED_CACHE
        options |= SQLITE_OPEN_SHAREDCACHE;
#else
        if (Str_parseBool(URL_getParameter(url, "shared-cache"))) {
                options |= SQLITE_OPEN_SHAREDCACHE;
        }
#endif
        return options;
}


static sqlite3 *_doConnect(Connection_T delegator, char **error) {
        int status;
        sqlite3 *db;
        URL_T url = Connection_getURL(delegator);
        const char *path = URL_getPath(url);
        if (! path) {
                *error = Str_dup("no database specified in URL");
                return NULL;
        }
#if SQLITE_VERSION_NUMBER >= 3005000
        status = sqlite3_open_v2(path, &db, _options(url) , NULL);
#else
        status = sqlite3_open(path, &db);
#endif
        if (SQLITE_OK != status) {
                *error = Str_cat("cannot open database '%s' -- %s", path, sqlite3_errmsg(db));
                sqlite3_close(db);
                return NULL;
        }
        return db;
}


static bool _setProperties(T C, char **error) {
        URL_T url = Connection_getURL(C->delegator);
        const char **properties = URL_getParameterNames(url);
        const char *handled_properties[] = {"serialized", "shared-cache", NULL};
        if (properties) {
                StringBuffer_clear(C->sb);
                for (int i = 0; properties[i]; i++) {
                        if (Str_member(properties[i], handled_properties)) {
                                continue; // Handled in _doConnect, ignore
                        } else {
                                StringBuffer_append(C->sb, "PRAGMA %s = %s; ", properties[i], URL_getParameter(url, properties[i]));
                        }
                }
                C->lastError = zdb_sqlite3_exec(C->db, StringBuffer_toString(C->sb));
                if (C->lastError != SQLITE_OK) {
                        *error = Str_cat("unable to set database pragmas -- %s", sqlite3_errmsg(C->db));
                        return false;
                }
        }
        return true;
}


/* -------------------------------------------------------- Delegate Methods */


static void _free(T *C) {
        assert(C && *C);
        while (sqlite3_close((*C)->db) == SQLITE_BUSY)
                Time_usleep(10);
        StringBuffer_free(&((*C)->sb));
        FREE(*C);
}


static T _new(Connection_T delegator, char **error) {
        T C;
        assert(delegator);
        assert(error);
        sqlite3 *db;
        if (! (db = _doConnect(delegator, error)))
                return NULL;
        NEW(C);
        C->db = db;
        C->delegator = delegator;
        // Set a minimal timeout to install a busy_timeout handler. Actual concurrency timeout is handled by
        // SQLiteAdapter.h methods using either unlock notify or a backoff retry strategy
        sqlite3_busy_timeout(C->db, kQueryTimeoutDelta);
        C->sb = StringBuffer_create(STRLEN);
        if (! _setProperties(C, error))
                _free(&C);
        return C;
}


static bool _ping(T C) {
        assert(C);
        C->lastError = zdb_sqlite3_exec(C->db, "select 1;");
        return (C->lastError == SQLITE_OK);
}


static void _setQueryTimeout(T C, int ms) {
        assert(C);
        if (ms <= 0)
                ms = kQueryTimeoutDelta; // Ensure a minimal timeout to install a busy_timeout handler
        sqlite3_busy_timeout(C->db, ms);
}


static bool _beginTransactionType(T C, TRANSACTION_TYPE type) {
    assert(C);
    const char *sql;
    switch (type) {
        case TRANSACTION_IMMEDIATE:
            sql = "BEGIN IMMEDIATE TRANSACTION;";
            break;
        case TRANSACTION_EXCLUSIVE:
            sql = "BEGIN EXCLUSIVE TRANSACTION;";
            break;
        default:
            sql = "BEGIN TRANSACTION;";
    }
    C->lastError = zdb_sqlite3_exec(C->db, sql);
    return (C->lastError == SQLITE_OK);
}


static bool _commit(T C) {
        assert(C);
        C->lastError = zdb_sqlite3_exec(C->db, "COMMIT TRANSACTION;");
        return (C->lastError == SQLITE_OK);
}


static bool _rollback(T C) {
        assert(C);
        C->lastError = zdb_sqlite3_exec(C->db, "ROLLBACK TRANSACTION;");
        return (C->lastError == SQLITE_OK);
}


static long long _lastRowId(T C) {
        assert(C);
        return sqlite3_last_insert_rowid(C->db);
}


static long long _rowsChanged(T C) {
        assert(C);
        return (long long)sqlite3_changes(C->db);
}


static bool _execute(T C, const char *sql, va_list ap) {
        assert(C);
        va_list ap_copy;
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        C->lastError = zdb_sqlite3_exec(C->db, StringBuffer_toString(C->sb));
        return (C->lastError == SQLITE_OK);
}


static ResultSet_T _executeQuery(T C, const char *sql, va_list ap) {
        va_list ap_copy;
        const char *tail;
        sqlite3_stmt *stmt;
        assert(C);
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        C->lastError = zdb_sqlite3_prepare_v2(C->db, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), &stmt, &tail);
        if (C->lastError == SQLITE_OK)
                return ResultSet_new(SQLiteResultSet_new(C->delegator, stmt, false), (Rop_T)&sqlite3rops);
        return NULL;
}


static PreparedStatement_T _prepareStatement(T C, const char *sql, va_list ap) {
        va_list ap_copy;
        const char *tail;
        sqlite3_stmt *stmt;
        assert(C);
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        C->lastError = zdb_sqlite3_prepare_v2(C->db, StringBuffer_toString(C->sb), -1, &stmt, &tail);
        if (C->lastError == SQLITE_OK) {
                return PreparedStatement_new(SQLitePreparedStatement_new(C->delegator, stmt), (Pop_T)&sqlite3pops);
        }
        return NULL;
}


static const char *_getLastError(T C) {
        assert(C);
        return sqlite3_errmsg(C->db);
}


static int _getLastErrorCode(T C) {
        assert(C);
        int error = sqlite3_extended_errcode(C->db);
        if (error == 0)
                return C->lastError;
        return error;
}


/* ------------------------------------------------------------------------- */


const struct Cop_T sqlite3cops = {
        .name 		        = "sqlite",
        .new 		        = _new,
        .free 		        = _free,
        .ping		        = _ping,
        .setQueryTimeout        = _setQueryTimeout,
        .beginTransactionType   = _beginTransactionType,
        .commit                 = _commit,
        .rollback	        = _rollback,
        .lastRowId	        = _lastRowId,
        .rowsChanged	        = _rowsChanged,
        .execute	        = _execute,
        .executeQuery	        = _executeQuery,
        .prepareStatement       = _prepareStatement,
        .getLastError           = _getLastError,
        .getLastErrorCode       = _getLastErrorCode
};

