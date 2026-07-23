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
 * All parameter values are sent as text except for blobs and large strings bound
 * to text-class columns (text, varchar, char), which are sent in binary format.
 * libpq ignores paramLengths for text-format parameters and reads the value up to
 * the NUL terminator, but honors it for binary-format parameters — and for the
 * text-class types the binary wire format is simply the raw string bytes. So for
 * parameters the server inferred as a text-class type we bind the caller's buffer
 * by reference with the caller-supplied length: no copy and no NUL termination
 * required. The inferred parameter types are fetched once per statement with
 * PQdescribePrepared() (one server round trip), triggered only when a string is
 * large enough that copying it would cost more than the round trip. All other
 * string values keep text format - so the server still parses text representations
 * of typed columns (dates, numbers, json, ...) — and are copied into a
 * NUL-terminated buffer (inline for short values, else an owned heap buffer) so
 * the caller-supplied length is honored and non-NUL-terminated buffers are safe.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


// Strings larger than this trigger the one-time PQdescribePrepared() round trip
// that enables the zero-copy binary path in _setString(); smaller strings are
// simply copied, which at these sizes is cheaper than a network round trip.
#define ZEROCOPY_THRESHOLD 1024

// Parameter type OIDs whose binary wire format is the raw string bytes. These
// OIDs are pinned in PostgreSQL's catalog (pg_type.dat) and have been stable
// since forever; libpq's client headers do not expose them.
#ifndef TEXTOID
#define TEXTOID    25
#endif
#ifndef BPCHAROID
#define BPCHAROID  1042
#endif
#ifndef VARCHAROID
#define VARCHAROID 1043
#endif

typedef struct param_t {
        char s[65];     // scratch for number/timestamp text, reused for short (SSO) strings
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
        char **sbuf;        // owned heap copies for strings too long for the inline params[].s
        int *sbufcap;       // allocated capacity of each sbuf[i] (grow-only, never shrinks)
        Oid *paramOids;     // parameter types from PQdescribePrepared(), fetched lazily; NULL until then
        bool describeFailed; // describe failed; zero-copy is an optimization, so just use the copy path
        Connection_T delegator;
};
extern const struct Rop_T postgresqlrops;


/* --------------------------------------------------------- Private methods */


// Fetch and cache the parameter types the server inferred for this statement.
// One round trip, performed at most once per statement. On failure, fall back
// permanently to the copy path in _setString()
static void _describe(T P) {
        if (P->paramOids || P->describeFailed)
                return;
        PGresult *r = PQdescribePrepared(P->db, P->stmt);
        if (r && PQresultStatus(r) == PGRES_COMMAND_OK && PQnparams(r) == P->parameterCount) {
                P->paramOids = CALLOC(P->parameterCount, sizeof(Oid));
                for (int i = 0; i < P->parameterCount; i++)
                        P->paramOids[i] = PQparamtype(r, i);
        } else {
                P->describeFailed = true;
        }
        PQclear(r);
}


// True for the types whose binary wire format is the raw string bytes (still
// subject to the usual client -> server encoding conversion, same as text
// format). Deliberately strict: other string-ish types are not byte-identical
// in binary form (e.g. jsonb prepends a version byte) and must stay on the
// text-format path.
static inline bool _isTextType(Oid t) {
        return t == TEXTOID || t == VARCHAROID || t == BPCHAROID;
}


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
                P->sbufcap = CALLOC(P->parameterCount, sizeof(int));
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
                FREE((*P)->sbufcap);
                FREE((*P)->paramOids);
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
        if (! x) {
                P->paramValues[i] = NULL; // SQL NULL
                P->paramLengths[i] = 0;
                P->paramFormats[i] = 0;
                return;
        }
        // Zero-copy path
        if (P->paramOids || size > ZEROCOPY_THRESHOLD) {
                _describe(P);
                if (P->paramOids && _isTextType(P->paramOids[i])) {
                        P->paramValues[i] = (char *)x;
                        P->paramLengths[i] = size;
                        P->paramFormats[i] = 1;
                        return;
                }
        }
        // Copy path
        char *buf;
        if (size < (int)sizeof(P->params[i].s)) { // fits inline with room for NUL
                buf = P->params[i].s;
        } else {
                if (P->sbufcap[i] <= size) { // need size + 1 bytes; test avoids overflow
                        if (P->sbuf[i])
                                RESIZE(P->sbuf[i], size + 1);
                        else
                                P->sbuf[i] = ALLOC(size + 1);
                        P->sbufcap[i] = size + 1;
                }
                buf = P->sbuf[i];
        }
        memcpy(buf, x, size);
        buf[size] = 0;
        P->paramValues[i] = buf;
        P->paramLengths[i] = size;
        P->paramFormats[i] = 0;
}


static void _setInt(T P, int parameterIndex, int x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->paramLengths[i] = snprintf(P->params[i].s, sizeof(P->params[i].s), "%d", x);
        P->paramValues[i] =  P->params[i].s;
        P->paramFormats[i] = 0;
}


static void _setLLong(T P, int parameterIndex, long long x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->paramLengths[i] = snprintf(P->params[i].s, sizeof(P->params[i].s), "%lld", x);
        P->paramValues[i] =  P->params[i].s;
        P->paramFormats[i] = 0;
}


static void _setDouble(T P, int parameterIndex, double x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        // %.17g preserves full IEEE-754 double precision on round-trip and uses
        // the shortest of fixed/scientific notation;
        P->paramLengths[i] = snprintf(P->params[i].s, sizeof(P->params[i].s), "%.17g", x);
        P->paramValues[i] =  P->params[i].s;
        P->paramFormats[i] = 0;
}


static void _setTimestamp(T P, int parameterIndex, time_t x) {
        assert(P);
        int i = checkAndSetParameterIndex(parameterIndex, P->parameterCount);
        P->paramValues[i] = Time_toString(x, P->params[i].s);
        P->paramLengths[i] = (int)strlen(P->paramValues[i]);
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

