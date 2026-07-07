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


/**
 * Implementation of the PreparedStatement/Delegate interface for Oracle.
 *
 * LOB handling:
 * PreparedStatement_setBlob() binds parameters as SQLT_BLOB using temporary
 * LOBs (OCILobCreateTemporary). This works for BLOB columns; binding to CLOB
 * columns may require SQLT_CLOB and appropriate character set handling.
 *
 * The OCI API supports both BLOB and CLOB via OCILobWrite2(), so a future
 * enhancement could add a setClob() method or auto-detect the target column
 * type if needed.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


typedef struct param_t {
        union {
                double real;
                long integer;
                const void *blob;
                const char *string;
                OCINumber number;
                OCIDateTime *date;
        } type;
        OCIInd is_null;
        int length;
        OCIBind *bind;
        OCILobLocator *lob_loc;
} *param_t;

#define T PreparedStatementDelegate_T
struct T {
        ub4         parameterCount;
        OCISession *usr;
        OCIStmt    *stmt;
        OCIEnv     *env;
        OCIError   *err;
        OCISvcCtx  *svc;
        param_t     params;
        sword       lastError;
        ub4         rowsChanged;
        Connection_T delegator;
        char        erb[ORACLE_ERR_SIZE];
};
extern const struct Rop_T oraclerops;


/* --------------------------------------------------------- Private methods */


/* Convenience macro for error messages */
#define ERR(P) Oracle_getError((P)->lastError, (P)->err, NULL, (P)->erb, sizeof((P)->erb))

/* Macro to throw SQLException with Oracle error code and message in one OCIErrorGet call */
#define THROW_ORACLE_ERROR(P) do { \
        int _code; \
        const char *_msg = Oracle_getError((P)->lastError, (P)->err, &_code, (P)->erb, sizeof((P)->erb)); \
        THROW_SQL(_code, "%s", _msg); \
} while(0)


/* ------------------------------------------------------------- Constructor */


T OraclePreparedStatement_new(Connection_T delegator, OCIStmt *stmt, OCIEnv *env, OCISession *usr, OCIError *err, OCISvcCtx *svc) {
        T P;
        assert(stmt);
        assert(env);
        assert(err);
        assert(svc);
        NEW(P);
        P->delegator = delegator;
        P->stmt = stmt;
        P->env = env;
        P->err = err;
        P->svc = svc;
        P->usr = usr;
        P->lastError = OCI_SUCCESS;
        P->rowsChanged = 0;
        /* parameterCount */
        P->lastError = OCIAttrGet(P->stmt, OCI_HTYPE_STMT, &P->parameterCount, NULL, OCI_ATTR_BIND_COUNT, P->err);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                P->parameterCount = 0;
        if (P->parameterCount)
                P->params = CALLOC(P->parameterCount, sizeof(struct param_t));
        return P;
}


/* -------------------------------------------------------- Delegate Methods */


static void _free(T *P) {
        assert(P && *P);
        OCIHandleFree((*P)->stmt, OCI_HTYPE_STMT);
        if ((*P)->params) {
                for (ub4 i = 0; i < (*P)->parameterCount; i++) {
                        if ((*P)->params[i].lob_loc) {
                                boolean is_temp = FALSE;
                                if ((*P)->svc && OCILobIsTemporary((*P)->env, (*P)->err, (*P)->params[i].lob_loc, &is_temp) == OCI_SUCCESS && is_temp) {
                                        OCILobFreeTemporary((*P)->svc, (*P)->err, (*P)->params[i].lob_loc);
                                }
                                OCIDescriptorFree((*P)->params[i].lob_loc, OCI_DTYPE_LOB);
                        }
                }
                FREE((*P)->params);
        }
        FREE(*P);
}


static void _setString(T P, int parameterIndex, const char *x, int size) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->params[i].type.string = x;
        if (x) {
                P->params[i].length = size;
                P->params[i].is_null = OCI_IND_NOTNULL;
        } else {
                P->params[i].length = 0;
                P->params[i].is_null = OCI_IND_NULL;
        }
        P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex, (char *)P->params[i].type.string,
                                    (int)P->params[i].length, SQLT_CHR, &P->params[i].is_null, 0, 0, 0, 0, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
}


static void _setTimestamp(T P, int parameterIndex, time_t time) {
        assert(P);
        struct tm ts = {.tm_isdst = -1};
        ub4 valid;
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->lastError = OCIDescriptorAlloc((dvoid *)P->env, (dvoid **)&(P->params[i].type.date),
                                          (ub4)OCI_DTYPE_TIMESTAMP, (size_t)0, (dvoid **)0);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
        gmtime_r(&time, &ts);
        OCIDateTimeConstruct(P->usr, P->err, P->params[i].type.date,
                             ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday,
                             ts.tm_hour, ts.tm_min, ts.tm_sec, 0, (OraText *)0, 0);
        if (OCI_SUCCESS != OCIDateTimeCheck(P->usr, P->err, P->params[i].type.date, &valid) || valid != 0)
                THROW(SQLException, "Invalid date/time value");
        P->params[i].length = sizeof(OCIDateTime *);
        P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex, &P->params[i].type.date,
                                    P->params[i].length, SQLT_TIMESTAMP, 0, 0, 0, 0, 0, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
}


static void _setInt(T P, int parameterIndex, int x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->params[i].type.integer = x;
        P->params[i].length = sizeof(x);
        P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex, &P->params[i].type.integer,
                                    (int)P->params[i].length, SQLT_INT, 0, 0, 0, 0, 0, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
}


static void _setLLong(T P, int parameterIndex, long long x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->params[i].length = sizeof(P->params[i].type.number);
        P->lastError = OCINumberFromInt(P->err, &x, sizeof(x), OCI_NUMBER_SIGNED, &P->params[i].type.number);
        if (P->lastError != OCI_SUCCESS)
                THROW(SQLException, "%s", ERR(P));
        P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex, &P->params[i].type.number,
                                    (int)P->params[i].length, SQLT_VNU, 0, 0, 0, 0, 0, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
}


static void _setDouble(T P, int parameterIndex, double x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->params[i].type.real = x;
        P->params[i].length = sizeof(x);
        P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex, &P->params[i].type.real,
                                    (int)P->params[i].length, SQLT_FLT, 0, 0, 0, 0, 0, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
}


static void _setBlob(T P, int parameterIndex, const void *x, int size) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        // Only a NULL pointer binds SQL NULL: a non-NULL, zero-length value is
        // stored as an empty blob, consistent with PreparedStatement_setString()
        // and the other database drivers
        if (x == NULL) {
                P->params[i].is_null = OCI_IND_NULL;
                P->params[i].length = 0;
                P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex,
                                            NULL, 0, SQLT_BLOB, &P->params[i].is_null, 0, 0, 0, 0, OCI_DEFAULT);
                if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                        THROW(SQLException, "%s", ERR(P));
                return;
        }
        // Allocate LOB locator if not already done
        if (P->params[i].lob_loc == NULL) {
                P->lastError = OCIDescriptorAlloc(P->env, (void **)&P->params[i].lob_loc, OCI_DTYPE_LOB, 0, NULL);
                if (P->lastError != OCI_SUCCESS)
                        THROW(SQLException, "Failed to allocate LOB descriptor");
        } else {
                // Free existing temp LOB before reuse
                boolean is_temp = FALSE;
                if (OCILobIsTemporary(P->env, P->err, P->params[i].lob_loc, &is_temp) == OCI_SUCCESS && is_temp)
                        OCILobFreeTemporary(P->svc, P->err, P->params[i].lob_loc);
        }
        // Create temporary BLOB
        P->lastError = OCILobCreateTemporary(P->svc, P->err, P->params[i].lob_loc,
                                             OCI_DEFAULT, OCI_DEFAULT, OCI_TEMP_BLOB,
                                             FALSE, OCI_DURATION_SESSION);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
        // Write data to the temporary LOB. A zero-length value skips the write
        // and binds the freshly created, still empty temporary BLOB, as
        // OCILobWrite2() does not accept a zero amount
        if (size > 0) {
                oraub8 amt = size;
                P->lastError = OCILobWrite2(P->svc, P->err, P->params[i].lob_loc, &amt, NULL, 1,
                                            (void *)x, (oraub8)size, OCI_ONE_PIECE, NULL, NULL, 0, SQLCS_IMPLICIT);
                if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                        THROW(SQLException, "%s", ERR(P));
        }
        // Bind the LOB locator
        P->params[i].is_null = OCI_IND_NOTNULL;
        P->params[i].length = sizeof(OCILobLocator *);
        P->lastError = OCIBindByPos(P->stmt, &P->params[i].bind, P->err, parameterIndex,
                                    &P->params[i].lob_loc, 0, SQLT_BLOB,
                                    &P->params[i].is_null, 0, 0, 0, 0, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW(SQLException, "%s", ERR(P));
}


static void _execute(T P) {
        assert(P);
        P->rowsChanged = 0;
        P->lastError = OCIStmtExecute(P->svc, P->stmt, P->err, 1, 0, NULL, NULL, OCI_DEFAULT);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW_ORACLE_ERROR(P);
        P->lastError = OCIAttrGet(P->stmt, OCI_HTYPE_STMT, &P->rowsChanged, 0, OCI_ATTR_ROW_COUNT, P->err);
        if (P->lastError != OCI_SUCCESS && P->lastError != OCI_SUCCESS_WITH_INFO)
                THROW_ORACLE_ERROR(P);
}


static ResultSet_T _executeQuery(T P) {
        assert(P);
        P->rowsChanged = 0;
        P->lastError = OCIStmtExecute(P->svc, P->stmt, P->err, 0, 0, NULL, NULL, OCI_DEFAULT);
        if (P->lastError == OCI_SUCCESS || P->lastError == OCI_SUCCESS_WITH_INFO)
                return ResultSet_new(OracleResultSet_new(P->delegator, P->stmt, P->env, P->usr, P->err, P->svc, false), (Rop_T)&oraclerops);
        THROW_ORACLE_ERROR(P);
        return NULL;
}


static long long _rowsChanged(T P) {
        assert(P);
        return P->rowsChanged;
}


static int _parameterCount(T P) {
        assert(P);
        return P->parameterCount;
}


/* ------------------------------------------------------------------------- */


const struct Pop_T oraclepops = {
        .name           = "oracle",
        .free           = _free,
        .setString      = _setString,
        .setInt         = _setInt,
        .setLLong       = _setLLong,
        .setDouble      = _setDouble,
        .setTimestamp   = _setTimestamp,
        .setBlob        = _setBlob,
        .execute        = _execute,
        .executeQuery   = _executeQuery,
        .rowsChanged    = _rowsChanged,
        .parameterCount = _parameterCount
};
