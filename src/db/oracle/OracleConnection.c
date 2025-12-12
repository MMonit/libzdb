/*
 * Copyright (C) 2010-2013 Volodymyr Tarasenko <tvntsr@yahoo.com>
 *               2010      Sergey Pavlov <sergey.pavlov@gmail.com>
 *               2010      PortaOne Inc.
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OracleAdapter.h"
#include "StringBuffer.h"
#include "ConnectionDelegate.h"


/**
 * Implementation of the Connection/Delegate interface for Oracle.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#define T ConnectionDelegate_T
struct T {
        Connection_T   delegator;
        OCIEnv*        env;
        OCIError*      err;
        OCISvcCtx*     svc;
        OCISession*    usr;
        OCIServer*     srv;
        OCITrans*      txnhp;
        char           erb[ORACLE_ERR_SIZE];
        int            maxRows;
        int            timeout;
        sword          lastError;
        ub4            rowsChanged;
        StringBuffer_T sb;
};
extern const struct Rop_T oraclerops;
extern const struct Pop_T oraclepops;


/* ------------------------------------------------------- Private methods */


/* Convenience macro for error messages */
#define ERR(C) Oracle_getError((C)->lastError, (C)->err, (C)->erb, sizeof((C)->erb))


static bool _doConnect(T C, char **error) {
#define ERROR(e) do {*error = Str_dup(e); return false;} while (0)
#define ORAERROR(e) do {*error = Str_dup(ERR(e)); return false;} while (0)
        URL_T url = Connection_getURL(C->delegator);
        const char *servicename, *username, *password;
        const char *host = URL_getHost(url);
        int port = URL_getPort(url);
        if (! (username = URL_getUser(url)))
                if (! (username = URL_getParameter(url, "user")))
                        ERROR("no username specified in URL");
        if (! (password = URL_getPassword(url)))
                if (! (password = URL_getParameter(url, "password")))
                        ERROR("no password specified in URL");
        if (! (servicename = URL_getPath(url)))
                ERROR("no Service Name specified in URL");
        ++servicename;
        /* Create a thread-safe OCI environment with N' substitution turned on. */
        if (OCIEnvCreate(&C->env, OCI_THREADED | OCI_OBJECT | OCI_NCHAR_LITERAL_REPLACE_ON, 0, 0, 0, 0, 0, 0))
                ERROR("Create a OCI environment failed");
        /* allocate an error handle */
        if (OCI_SUCCESS != OCIHandleAlloc(C->env, (dvoid**)&C->err, OCI_HTYPE_ERROR, 0, 0))
                ERROR("Allocating error handler failed");
        /* server contexts */
        if (OCI_SUCCESS != OCIHandleAlloc(C->env, (dvoid**)&C->srv, OCI_HTYPE_SERVER, 0, 0))
                ERROR("Allocating server context failed");
        /* allocate a service handle */
        if (OCI_SUCCESS != OCIHandleAlloc(C->env, (dvoid**)&C->svc, OCI_HTYPE_SVCCTX, 0, 0))
                ERROR("Allocating service handle failed");
        StringBuffer_clear(C->sb);
        /* Oracle connect string is on the form: //host[:port]/service name */
        if (host) {
                StringBuffer_append(C->sb, "//%s", host);
                if (port > 0)
                        StringBuffer_append(C->sb, ":%d", port);
                StringBuffer_append(C->sb, "/%s", servicename);
        } else /* Or just service name */
                StringBuffer_append(C->sb, "%s", servicename);
        // Set Connection ResultSet fetch size if found in URL
        const char *fetchSize = URL_getParameter(url, "fetch-size");
        if (fetchSize) {
                int rows = Str_parseInt(fetchSize);
                if (rows < 1)
                        ERROR("invalid fetch-size");
                Connection_setFetchSize(C->delegator, rows);
        }
        /* Create a server context */
        C->lastError = OCIServerAttach(C->srv, C->err, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), OCI_DEFAULT);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                ORAERROR(C);
        /* Set attribute server context in the service context */
        C->lastError = OCIAttrSet(C->svc, OCI_HTYPE_SVCCTX, C->srv, 0, OCI_ATTR_SERVER, C->err);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                ORAERROR(C);
        C->lastError = OCIHandleAlloc(C->env, (void**)&C->usr, OCI_HTYPE_SESSION, 0, NULL);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                ORAERROR(C);
        C->lastError = OCIAttrSet(C->usr, OCI_HTYPE_SESSION, (dvoid *)username, (int)strlen(username), OCI_ATTR_USERNAME, C->err);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                ORAERROR(C);
        C->lastError = OCIAttrSet(C->usr, OCI_HTYPE_SESSION, (dvoid *)password, (int)strlen(password), OCI_ATTR_PASSWORD, C->err);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                ORAERROR(C);
        ub4 sessionFlags = OCI_DEFAULT;
        if (Str_parseBool(URL_getParameter(url, "sysdba"))) {
                sessionFlags |= OCI_SYSDBA;
        }
        C->lastError = OCISessionBegin(C->svc, C->err, C->usr, OCI_CRED_RDBMS, sessionFlags);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                ORAERROR(C);
        OCIAttrSet(C->svc, OCI_HTYPE_SVCCTX, C->usr, 0, OCI_ATTR_SESSION, C->err);
        return true;
}


/* -------------------------------------------------------- Delegate Methods */


static void _free(T *C) {
        assert(C && *C);
        if ((*C)->svc)
                OCISessionEnd((*C)->svc, (*C)->err, (*C)->usr, OCI_DEFAULT);
        if ((*C)->srv)
                OCIServerDetach((*C)->srv, (*C)->err, OCI_DEFAULT);
        if ((*C)->env)
                OCIHandleFree((*C)->env, OCI_HTYPE_ENV);
        StringBuffer_free(&((*C)->sb));
        FREE(*C);
}


static T _new(Connection_T delegator, char **error) {
        T C;
        assert(delegator);
        assert(error);
        NEW(C);
        C->delegator = delegator;
        C->sb = StringBuffer_create(STRLEN);
        if (! _doConnect(C, error)) {
                _free(&C);
                return NULL;
        }
        C->txnhp = NULL;
        return C;
}


static bool _ping(T C) {
        assert(C);
        C->lastError = OCIPing(C->svc, C->err, OCI_DEFAULT);
        return (C->lastError == OCI_SUCCESS);
}


static void _setQueryTimeout(T C, int ms) {
        assert(C);
        assert(ms >= 0);
        C->timeout = ms;
        // OCI_ATTR_CALL_TIMEOUT (Oracle 18c+): Sets a timeout in milliseconds for OCI round-trips
        ub4 timeout_ms = (ub4)ms;
        C->lastError = OCIAttrSet(C->svc, OCI_HTYPE_SVCCTX, &timeout_ms, 0, OCI_ATTR_CALL_TIMEOUT, C->err);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                DEBUG("Warning: Failed to set OCI_ATTR_CALL_TIMEOUT: %s\n", ERR(C));
}


static bool _beginTransactionType(T C, TRANSACTION_TYPE type) {
        assert(C);
        // Allocate transaction handle if not already done
        if (C->txnhp == NULL) {
                C->lastError = OCIHandleAlloc(C->env, (void **)&C->txnhp, OCI_HTYPE_TRANS, 0, 0);
                if (C->lastError != OCI_SUCCESS)
                        return false;
                OCIAttrSet(C->svc, OCI_HTYPE_SVCCTX, (void *)C->txnhp, 0, OCI_ATTR_TRANS, C->err);
        }
        // Oracle supports two isolation levels: READ COMMITTED (default) and SERIALIZABLE
        ub4 flags = OCI_TRANS_NEW;
        if (type == TRANSACTION_SERIALIZABLE) {
                flags |= OCI_TRANS_SERIALIZABLE;
        }
        // All other types (DEFAULT, READ_COMMITTED, etc.) use Oracle's default READ COMMITTED
        C->lastError = OCITransStart(C->svc, C->err, 0, flags);
        return (C->lastError == OCI_SUCCESS);
}


static bool _commit(T C) {
        assert(C);
        C->lastError = OCITransCommit(C->svc, C->err, OCI_DEFAULT);
        return C->lastError == OCI_SUCCESS;
}


static bool _rollback(T C) {
        assert(C);
        C->lastError = OCITransRollback(C->svc, C->err, OCI_DEFAULT);
        return C->lastError == OCI_SUCCESS;
}


static long long _lastRowId(T C) {
        assert(C);
        // NA: See Connection_lastRowId documentation
        DEBUG("OracleConnection_lastRowId: Not implemented - use RETURNING clause\n");
        return -1;
}


static long long _rowsChanged(T C) {
        assert(C);
        return C->rowsChanged;
}


static bool _execute(T C, const char *sql, va_list ap) {
        OCIStmt *stmtp;
        va_list ap_copy;
        assert(C);
        C->rowsChanged = 0;
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        StringBuffer_trim(C->sb);
        /* Build statement */
        C->lastError = OCIHandleAlloc(C->env, (void **)&stmtp, OCI_HTYPE_STMT, 0, NULL);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                return false;
        C->lastError = OCIStmtPrepare(stmtp, C->err, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO) {
                OCIHandleFree(stmtp, OCI_HTYPE_STMT);
                return false;
        }
        /* Execute */
        C->lastError = OCIStmtExecute(C->svc, stmtp, C->err, 1, 0, NULL, NULL, OCI_DEFAULT);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO) {
                ub4 parmcnt = 0;
                OCIAttrGet(stmtp, OCI_HTYPE_STMT, &parmcnt, NULL, OCI_ATTR_PARSE_ERROR_OFFSET, C->err);
                DEBUG("Error in StmtExecute %d (%s), offset is %d\n", C->lastError, ERR(C), parmcnt);
                OCIHandleFree(stmtp, OCI_HTYPE_STMT);
                return false;
        }
        C->lastError = OCIAttrGet(stmtp, OCI_HTYPE_STMT, &C->rowsChanged, 0, OCI_ATTR_ROW_COUNT, C->err);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                DEBUG("OracleConnection_execute: Error in OCIAttrGet %d (%s)\n", C->lastError, ERR(C));
        OCIHandleFree(stmtp, OCI_HTYPE_STMT);
        return C->lastError == OCI_SUCCESS;
}


static ResultSet_T _executeQuery(T C, const char *sql, va_list ap) {
        OCIStmt *stmtp;
        va_list ap_copy;
        assert(C);
        C->rowsChanged = 0;
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        StringBuffer_trim(C->sb);
        /* Build statement */
        C->lastError = OCIHandleAlloc(C->env, (void **)&stmtp, OCI_HTYPE_STMT, 0, NULL);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                return NULL;
        C->lastError = OCIStmtPrepare(stmtp, C->err, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO) {
                OCIHandleFree(stmtp, OCI_HTYPE_STMT);
                return NULL;
        }
        /* Execute and create Result Set */
        C->lastError = OCIStmtExecute(C->svc, stmtp, C->err, 0, 0, NULL, NULL, OCI_DEFAULT);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO) {
                ub4 parmcnt = 0;
                OCIAttrGet(stmtp, OCI_HTYPE_STMT, &parmcnt, NULL, OCI_ATTR_PARSE_ERROR_OFFSET, C->err);
                DEBUG("Error in StmtExecute %d (%s), offset is %d\n", C->lastError, ERR(C), parmcnt);
                OCIHandleFree(stmtp, OCI_HTYPE_STMT);
                return NULL;
        }
        C->lastError = OCIAttrGet(stmtp, OCI_HTYPE_STMT, &C->rowsChanged, 0, OCI_ATTR_ROW_COUNT, C->err);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                DEBUG("OracleConnection_execute: Error in OCIAttrGet %d (%s)\n", C->lastError, ERR(C));
        return ResultSet_new(OracleResultSet_new(C->delegator, stmtp, C->env, C->usr, C->err, C->svc, true), (Rop_T)&oraclerops);
}


static PreparedStatement_T _prepareStatement(T C, const char *sql, va_list ap) {
        OCIStmt *stmtp;
        va_list ap_copy;
        assert(C);
        va_copy(ap_copy, ap);
        StringBuffer_vset(C->sb, sql, ap_copy);
        va_end(ap_copy);
        StringBuffer_trim(C->sb);
        StringBuffer_prepare4oracle(C->sb);
        /* Build statement */
        C->lastError = OCIHandleAlloc(C->env, (void **)&stmtp, OCI_HTYPE_STMT, 0, 0);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO)
                return NULL;
        C->lastError = OCIStmtPrepare(stmtp, C->err, StringBuffer_toString(C->sb), StringBuffer_length(C->sb), OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (C->lastError != OCI_SUCCESS && C->lastError != OCI_SUCCESS_WITH_INFO) {
                OCIHandleFree(stmtp, OCI_HTYPE_STMT);
                return NULL;
        }
        return PreparedStatement_new(OraclePreparedStatement_new(C->delegator, stmtp, C->env, C->usr, C->err, C->svc), (Pop_T)&oraclepops);
}


static const char *_getLastError(T C) {
        assert(C);
        return ERR(C);
}


static int _getLastErrorCode(T C) {
        assert(C);
        return C->lastError;
}


/* ------------------------------------------------------------------------- */


const struct Cop_T oraclesqlcops = {
        .name                   = "oracle",
        .new                    = _new,
        .free                   = _free,
        .ping                   = _ping,
        .setQueryTimeout        = _setQueryTimeout,
        .beginTransactionType   = _beginTransactionType,
        .commit                 = _commit,
        .rollback               = _rollback,
        .lastRowId              = _lastRowId,
        .rowsChanged            = _rowsChanged,
        .execute                = _execute,
        .executeQuery           = _executeQuery,
        .prepareStatement       = _prepareStatement,
        .getLastError           = _getLastError,
        .getLastErrorCode       = _getLastErrorCode
};
