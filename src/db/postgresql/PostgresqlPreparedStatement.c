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

#include "system/Time.h"
#include "PostgresqlAdapter.h"


/**
 * Implementation of the PreparedStatement/Delegate interface for postgresql.
 * All parameter values are sent as text except for blobs, which are sent in
 * binary format. libpq ignores paramLengths for text parameters (so it is set to
 * 0 for those); string values are copied into an owned, NUL-terminated buffer so
 * the caller-supplied length is honored and non-NUL-terminated buffers are safe.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


typedef struct param_t {
        char s[65];
} *param_t;
#define T PreparedStatementDelegate_T
struct T {
        int lastError;
        char *stmt;
        PGconn *db;
        PGresult *res;
        param_t params;
        int parameterCount;
        char **paramValues;
        int *paramLengths;
        int *paramFormats;
        char **sbuf;        // owned, length-exact copies of string parameters
        Connection_T delegator;
};
extern const struct Rop_T postgresqlrops;


/* ------------------------------------------------------------- Constructor */


T PostgresqlPreparedStatement_new(Connection_T delegator, PGconn *db, char *stmt, int parameterCount) {
        T P;
        assert(db);
        assert(stmt);
        NEW(P);
        P->delegator = delegator;
        P->db = db;
        P->stmt = stmt;
        P->parameterCount = parameterCount;
        P->lastError = PGRES_COMMAND_OK;
        if (P->parameterCount) {
                P->paramValues = CALLOC(P->parameterCount, sizeof(char *));
                P->paramLengths = CALLOC(P->parameterCount, sizeof(int));
                P->paramFormats = CALLOC(P->parameterCount, sizeof(int));
                P->params = CALLOC(P->parameterCount, sizeof(struct param_t));
                P->sbuf = CALLOC(P->parameterCount, sizeof(char *));
        }
        return P;
}


/* -------------------------------------------------------- Delegate Methods */


static void _free(T *P) {
	assert(P && *P);
        /* There is no C API function for explicit statement
         deallocation as of postgres v. 11 - the DEALLOCATE statement
         has to be used. The postgres documentation mentiones such a
         function as a possible future extension */
        char stmt[STRLEN];
        snprintf(stmt, STRLEN, "DEALLOCATE \"%s\";", (*P)->stmt);
        PQclear(PQexec((*P)->db, stmt));
        PQclear((*P)->res);
	FREE((*P)->stmt);
        if ((*P)->parameterCount) {
                for (int i = 0; i < (*P)->parameterCount; i++)
                        FREE((*P)->sbuf[i]);
                FREE((*P)->sbuf);
	        FREE((*P)->paramValues);
	        FREE((*P)->paramLengths);
	        FREE((*P)->paramFormats);
	        FREE((*P)->params);
        }
	FREE(*P);
}


static void _setString(T P, int parameterIndex, const char *x, int size) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        FREE(P->sbuf[i]); // release any copy from a previous bind on this index
        if (! x) {
                P->paramValues[i] = NULL; // SQL NULL
                P->paramLengths[i] = 0;
                P->paramFormats[i] = 0;
                return;
        }
        // libpq ignores paramLengths for text-format parameters and reads the value
        // up to the NUL terminator. To honor the caller-supplied length (setSString)
        // and avoid over-reading a non-NUL-terminated buffer, copy exactly 'size'
        // bytes into an owned, NUL-terminated buffer. Text format (not binary) is
        // kept so the server still parses text representations of typed columns
        // (dates, numbers, ...) bound via setString().
        P->sbuf[i] = ALLOC(size + 1);
        memcpy(P->sbuf[i], x, size);
        P->sbuf[i][size] = 0;
        P->paramValues[i] = P->sbuf[i];
        P->paramLengths[i] = 0;
        P->paramFormats[i] = 0;
}


static void _setInt(T P, int parameterIndex, int x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        snprintf(P->params[i].s, 64, "%d", x);
        P->paramValues[i] =  P->params[i].s;
        P->paramLengths[i] = 0;
        P->paramFormats[i] = 0;
}


static void _setLLong(T P, int parameterIndex, long long x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        snprintf(P->params[i].s, 64, "%lld", x);
        P->paramValues[i] =  P->params[i].s;
        P->paramLengths[i] = 0; 
        P->paramFormats[i] = 0;
}


static void _setDouble(T P, int parameterIndex, double x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        // %.17g preserves full IEEE-754 double precision on round-trip and uses
        // the shortest of fixed/scientific notation; %lf (fixed 6-decimal) loses
        // precision and truncates large magnitudes (e.g. 1e300 needs ~308 digits)
        snprintf(P->params[i].s, 64, "%.17g", x);
        P->paramValues[i] =  P->params[i].s;
        P->paramLengths[i] = 0;
        P->paramFormats[i] = 0;
}


static void _setTimestamp(T P, int parameterIndex, time_t x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->paramValues[i] = Time_toString(x, P->params[i].s);
        P->paramLengths[i] = 0;
        P->paramFormats[i] = 0;
}


static void _setBlob(T P, int parameterIndex, const void *x, int size) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->paramValues[i] = (char *)x;
        P->paramLengths[i] = size;
        P->paramFormats[i] = 1;
}


static void _execute(T P) {
        assert(P);
        PQclear(P->res);
        P->res = PQexecPrepared(P->db, P->stmt, P->parameterCount, (const char **)P->paramValues, P->paramLengths, P->paramFormats, 0);
        P->lastError = P->res ? PQresultStatus(P->res) : PGRES_FATAL_ERROR;
        if (P->lastError != PGRES_COMMAND_OK) {
                THROW_SQL(_getSQLStateErrorCode(P->res), "%s", _getSQLErrorMessage(P->res));
        }
}


static ResultSet_T _executeQuery(T P) {
        assert(P);
        PQclear(P->res);
        P->res = PQexecPrepared(P->db, P->stmt, P->parameterCount, (const char **)P->paramValues, P->paramLengths, P->paramFormats, 0);
        P->lastError = P->res ? PQresultStatus(P->res) : PGRES_FATAL_ERROR;
        if (P->lastError == PGRES_TUPLES_OK)
                return ResultSet_new(PostgresqlResultSet_new(P->delegator, P->res), (Rop_T)&postgresqlrops);
        THROW_SQL(_getSQLStateErrorCode(P->res), "%s", _getSQLErrorMessage(P->res));
        return NULL;
}


static long long _rowsChanged(T P) {
        assert(P);
        // PQcmdTuples() returns the empty string, never NULL, for commands
        // without a tuple count (e.g. CREATE TABLE, SET), and Str_parseLLong
        // would throw on it: return 0 like the other database drivers
        char *changes = PQcmdTuples(P->res);
        return STR_DEF(changes) ? Str_parseLLong(changes) : 0;
}


static int _parameterCount(T P) {
        assert(P);
        return P->parameterCount;
}


/* ------------------------------------------------------------------------- */


const struct Pop_T postgresqlpops = {
        .name           = "postgresql",
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

