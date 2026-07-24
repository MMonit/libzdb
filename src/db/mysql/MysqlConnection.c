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
#include <string.h>
#include <errmsg.h>
#include <ctype.h>

#include "Str.h"
#include "MysqlAdapter.h"
#include "StringBuffer.h"
#include "ConnectionDelegate.h"


/**
 * Implementation of the Connection/Delegate interface for mysql.
 *
 * @file
 */


/* ------------------------------------------------------------- Definitions */


#define T ConnectionDelegate_T
struct T {
        MYSQL *db;
        int lastError;
        StringBuffer_T sb;
        Connection_T delegator;
};
#define MYSQL_OK 0
extern const struct Rop_T mysqlrops;
extern const struct Pop_T mysqlpops;


/* --------------------------------------------------------- Private methods */


static bool _isConnectedToProxy(MYSQL *db, char **error) {
        const char *server_info = mysql_get_server_info(db);
        if (!server_info) return false;
        
        const char *proxy_type = NULL;
        if (Str_sub(server_info, "router")) {
                proxy_type = "MySQL Router";
        } else if (Str_sub(server_info, "maxscale")) {
                proxy_type = "MariaDB MaxScale";
        } else if (Str_sub(server_info, "proxy")) {
                proxy_type = "MySQL Proxy";
        } else if (Str_sub(server_info, "vitess")) {
                proxy_type = "Vitess";
        }
        
        if (proxy_type) {
                *error = Str_cat("%s detected (%s).\n"
                                 "MySQL proxies are not supported due to connection stability and prepared\n"
                                 "statement compatibility issues. Please connect directly to your MySQL/MariaDB\n"
                                 "server instead.",
                                 proxy_type, server_info);
                return true;
        }
        return false;
}


static MYSQL *_doConnect(Connection_T delegator, char **error) {
#define ERROR(e) do {*error = Str_dup(e); goto error;} while (0)
        URL_T url = Connection_getURL(delegator);
        int connectTimeout = SQL_DEFAULT_TIMEOUT / MSEC_PER_SEC;
        unsigned long clientFlags = CLIENT_MULTI_STATEMENTS;
        MYSQL *db = mysql_init(NULL);
        if (! db) {
                *error = Str_dup("unable to allocate mysql handler");
                return NULL;
        }
        const char *user = URL_getUser(url);
        if (! user)
                if (! (user = URL_getParameter(url, "user")))
                        ERROR("no username specified in URL");
        const char *password = URL_getPassword(url);
        if (! password)
                if (! (password = URL_getParameter(url, "password")))
                        ERROR("no password specified in URL");
        const char *host = URL_getHost(url);
        const char *unix_socket = URL_getParameter(url, "unix-socket");
        if (unix_socket) {
                host = "localhost"; // Make sure host is localhost if unix socket is to be used
        } else if (! host)
                ERROR("no host specified in URL");
        int port = URL_getPort(url);
        if (port <= 0)
                ERROR("no port specified in URL");
        const char *database = URL_getPath(url);
        if (! database)
                ERROR("no database specified in URL");
        else
                database++;
        // Options
        if (Str_parseBool(URL_getParameter(url, "compress")))
                clientFlags |= CLIENT_COMPRESS;
        if (Str_parseBool(URL_getParameter(url, "use-ssl"))) {
#ifndef LIBMARIADB
                enum mysql_ssl_mode ssl_mode = SSL_MODE_REQUIRED;
                mysql_options(db, MYSQL_OPT_SSL_MODE, &ssl_mode);
#else
                // MariaDB hasn't got the memo that mysql_ssl_set is deprecated
                mysql_ssl_set(db, 0,0,0,0,0);
#endif
        }
#if MYSQL_VERSION_ID < 80000
        if (Str_parseBool(URL_getParameter(url, "secure-auth")))
                mysql_options(db, MYSQL_SECURE_AUTH, (const char*)&yes);
        else {
                bool no = 0;
                mysql_options(db, MYSQL_SECURE_AUTH, (const char*)&no);
        }
#else
        if (URL_getParameter(url, "auth-plugin")) {
                mysql_options(db, MYSQL_DEFAULT_AUTH, URL_getParameter(url, "auth-plugin"));
        }
#endif
        const char *timeout = URL_getParameter(url, "connect-timeout");
        if (timeout)
                connectTimeout = Str_parseInt(timeout);
        mysql_options(db, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&connectTimeout);
        const char *charset = URL_getParameter(url, "charset");
        if (charset)
                mysql_options(db, MYSQL_SET_CHARSET_NAME, charset);
#if MYSQL_VERSION_ID < 80034
        // Explicitly disable auto-reconnect for safety (transaction/session state)
        // In MySQL 8.0.34+, reconnect is removed and disabled by default
        bool reconnect = false;
        mysql_options(db, MYSQL_OPT_RECONNECT, &reconnect);
#endif
        // Set Connection ResultSet fetch size if found in URL
        const char *fetchSize = URL_getParameter(url, "fetch-size");
        if (fetchSize) {
                int rows = Str_parseInt(fetchSize);
                if (rows < 1)
                        ERROR("invalid fetch-size");
                Connection_setFetchSize(delegator, rows);
        }

        if (URL_getParameter(url, "ssl-ca")) {
                mysql_options(db, MYSQL_OPT_SSL_CA, URL_getParameter(url, "ssl-ca"));
        }
        if (URL_getParameter(url, "ssl-cert")) {
                mysql_options(db, MYSQL_OPT_SSL_CERT, URL_getParameter(url, "ssl-cert"));
        }
        if (URL_getParameter(url, "ssl-key")) {
                mysql_options(db, MYSQL_OPT_SSL_KEY, URL_getParameter(url, "ssl-key"));
        }

        // Connect
        if (mysql_real_connect(db, host, user, password, database, port, unix_socket, clientFlags)) {
                // Check for MySQL Proxies after successful connection (unless explicitly allowed)
                if (!Str_parseBool(URL_getParameter(url, "allow-proxy"))) {
                        if (_isConnectedToProxy(db, error))
                                goto error;
                }
                return db;
        }
        *error = Str_dup(mysql_error(db));
error:
        mysql_close(db);
        return NULL;
}


static bool _prepare(T C, const char *sql, int len, MYSQL_STMT **stmt) {
        if (! (*stmt = mysql_stmt_init(C->db))) {
                DEBUG("mysql_stmt_init -- Out of memory\n");
                C->lastError = CR_OUT_OF_MEMORY;
                return false;
        }
        if ((C->lastError = mysql_stmt_prepare(*stmt, sql, len))) {
                StringBuffer_set(C->sb, "%s", mysql_stmt_error(*stmt));
                mysql_stmt_close(*stmt);
                *stmt = NULL;
                return false;
        }
        return true;
}


/* -------------------------------------------------------- Delegate Methods */


static T _new(Connection_T delegator, char **error) {
        T C;
        assert(delegator);
        assert(error);
        MYSQL *db;
        if (! (db = _doConnect(delegator, error)))
                return NULL;
        NEW(C);
        C->db = db;
        C->delegator = delegator;
        C->sb = StringBuffer_create(STRLEN);
        return C;
}


static void _free(T *C) {
        assert(C && *C);
        mysql_close((*C)->db);
        StringBuffer_free(&((*C)->sb));
        FREE(*C);
}


static bool _ping(T C) {
        assert(C);
        return (mysql_ping(C->db) == 0);
}


static void _setQueryTimeout(T C, int ms) {
        assert(C);
#if MYSQL_VERSION_ID >= 50704
        StringBuffer_set(C->sb, "SET SESSION MAX_EXECUTION_TIME=%d;", ms);
        C->lastError = mysql_query(C->db, StringBuffer_toString(C->sb));
#endif
}


// Since CLIENT_MULTI_STATEMENTS is enabled, we need to drain results to
// prevent CR_COMMANDS_OUT_OF_SYNC in case multiple statements were used. This
// function is called on the _execute path, where no result set is expected.
// The purpose here is to drain protocol result packets off the wire, not to
// read result sets: non-query statements carry none (store_result -> NULL); we
// still store+free defensively in case a statement (e.g. a CALL) returns rows.
static void _drainResults(T C) {
        while (mysql_more_results(C->db)) {
                if (mysql_next_result(C->db) != 0)
                        break; // -1: no more results, or >0: a later statement failed
                MYSQL_RES *result = mysql_store_result(C->db);
                if (result)
                        mysql_free_result(result);
        }
}


static bool _beginTransactionType(T C, TRANSACTION_TYPE type) {
        assert(C);
        const char *sql;
        switch (type) {
                case TRANSACTION_READ_UNCOMMITTED:
                        sql = "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED; START TRANSACTION;";
                        break;
                case TRANSACTION_READ_COMMITTED:
                        sql = "SET TRANSACTION ISOLATION LEVEL READ COMMITTED; START TRANSACTION;";
                        break;
                case TRANSACTION_REPEATABLE_READ:
                        sql = "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ; START TRANSACTION;";
                        break;
                case TRANSACTION_SERIALIZABLE:
                        sql = "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE; START TRANSACTION;";
                        break;
                default:
                        sql = "START TRANSACTION;";
        }
        C->lastError = mysql_query(C->db, sql);
        _drainResults(C); // "SET TRANSACTION ...; START TRANSACTION;" yields two results
        return (C->lastError == MYSQL_OK);
}


static bool _commit(T C) {
        assert(C);
        C->lastError = mysql_query(C->db, "COMMIT;");
        return (C->lastError == MYSQL_OK);
}


static bool _rollback(T C) {
        assert(C);
        C->lastError = mysql_query(C->db, "ROLLBACK;");
        return (C->lastError == MYSQL_OK);
}


static long long _lastRowId(T C) {
        assert(C);
        return (long long)mysql_insert_id(C->db);
}


static long long _rowsChanged(T C) {
        assert(C);
        return (long long)mysql_affected_rows(C->db);
}


static bool _execute(T C, const char *sql, va_list ap) {
        assert(C);
        va_list ap_copy;
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        C->lastError = mysql_real_query(C->db, StringBuffer_toString(C->sb), StringBuffer_length(C->sb));
        _drainResults(C); // user SQL may contain several ';'-separated statements
        return (C->lastError == MYSQL_OK);
}


static ResultSet_T _executeQuery(T C, const char *sql, va_list ap) {
        assert(C);
        va_list ap_copy;
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        MYSQL_STMT *stmt = NULL;
        if (_prepare(C, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), &stmt)) {
#if MYSQL_VERSION_ID >= 50002
                unsigned long cursor = CURSOR_TYPE_READ_ONLY;
                mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, &cursor);
#endif
                if ((C->lastError = mysql_stmt_execute(stmt))) {
                        StringBuffer_set(C->sb, "%s", mysql_stmt_error(stmt));
                        mysql_stmt_close(stmt);
                } else
                        return ResultSet_new(MysqlResultSet_new(C->delegator, stmt, false), (Rop_T)&mysqlrops);
        }
        return NULL;
}


static PreparedStatement_T _prepareStatement(T C, const char *sql, va_list ap) {
        assert(C);
        va_list ap_copy;
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        MYSQL_STMT *stmt = NULL;
        if (_prepare(C, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), &stmt)) {
                return PreparedStatement_new(MysqlPreparedStatement_new(C->delegator, stmt), (Pop_T)&mysqlpops);
        }
        return NULL;
}


static const char *_getLastError(T C) {
        assert(C);
        if (mysql_errno(C->db))
                return mysql_error(C->db);
        return StringBuffer_toString(C->sb); // Either the statement itself or a statement error
}


static int _getLastErrorCode(T C) {
        assert(C);
        int error = mysql_errno(C->db);
        if (error == 0)
                return C->lastError;
        return error;
}


/* ------------------------------------------------------------------------- */


const struct Cop_T mysqlcops = {
        .name 		        = "mysql",
        .new 		        = _new,
        .free 		        = _free,
        .ping		        = _ping,
        .setQueryTimeout        = _setQueryTimeout,
        .beginTransactionType   = _beginTransactionType,
        .commit		        = _commit,
        .rollback	        = _rollback,
        .lastRowId	        = _lastRowId,
        .rowsChanged	        = _rowsChanged,
        .execute	        = _execute,
        .executeQuery           = _executeQuery,
        .prepareStatement       = _prepareStatement,
        .getLastError           = _getLastError,
        .getLastErrorCode       = _getLastErrorCode
};

