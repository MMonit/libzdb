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
#include <ctype.h>
#include <sys/types.h>

#include "PostgresqlAdapter.h"


/**
 * Implementation of the ResultSet/Delegate interface for postgresql.
 * Accessing columns with index outside range throws SQLException
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#define T ResultSetDelegate_T
struct T {
        int maxRows;
        int rowCount;
        int currentRow;
        int columnCount;
        PGresult *res;
        Connection_T delegator;
        uchar_t **blob;   // per-column decoded blob for the current row, NULL until read
        int *blobSize;    // decoded size for each cached blob
};
#define ISFIRSTOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '3')
#define ISOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '7')
#define OCTVAL(CH) ((CH) - '0')


/* ------------------------------------------------------- Private methods */


// Decode the escaped bytea 'src' (len bytes, from PQgetvalue) into 'dest' using
// the (un)escape mechanism described at
// https://www.postgresql.org/docs/current/datatype-binary.html
// 'dest' must have room for at least 'len' bytes (the decoded form is never
// longer than the escaped form). Returns the number of decoded bytes.
// The 'src' (the PGresult buffer) is not modified, so getString() and repeated
// getBlob() on the same cell remain correct.
static inline int _unescape_bytea(const uchar_t *src, int len, uchar_t *dest) {
        register int i, j;
        if (len >= 2 && src[0] == '\\' && src[1] == 'x') { // bytea hex format
                static const uchar_t hex[128] = {
                        0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
                        0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                };
                for (i = 0, j = 2; j < len; j++) {
                        // Whitespace between hex pairs is allowed 🤔
                        if (isxdigit(src[j])) {
                                dest[i] = hex[src[j]] << 4;
                                dest[i] |= hex[src[j + 1]];
                                i++;
                                j++;
                        }
                }
        } else { // bytea escaped format
                uchar_t byte;
                for (i = j = 0; j < len; i++, j++) {
                        if ((dest[i] = src[j]) == '\\') {
                                if (src[j + 1] == '\\')
                                        j++;
                                else if ((ISFIRSTOCTDIGIT(src[j + 1]))
                                         && (ISOCTDIGIT(src[j + 2]))
                                         && (ISOCTDIGIT(src[j + 3]))) {
                                        byte = OCTVAL(src[j + 1]);
                                        byte = (byte << 3) + OCTVAL(src[j + 2]);
                                        byte = (byte << 3) + OCTVAL(src[j + 3]);
                                        dest[i] = byte;
                                        j += 3;
                                }
                        }
                }
        }
        return i;
}


// Free the decoded blob buffers cached for the current row.
static void _clearBlobs(T R) {
        if (R->blob)
                for (int i = 0; i < R->columnCount; i++)
                        if (R->blob[i])
                                FREE(R->blob[i]);
}

/* ------------------------------------------------------------- Constructor */


T PostgresqlResultSet_new(Connection_T delegator, PGresult *res) {
        T R;
        assert(delegator);
        NEW(R);
        R->delegator = delegator;
        R->res = res;
        R->maxRows = Connection_getMaxRows(delegator);
        R->currentRow = -1;
        R->columnCount = PQnfields(R->res);
        R->rowCount = PQntuples(R->res);
        if (R->columnCount > 0) {
                R->blob = CALLOC(R->columnCount, sizeof *R->blob);
                R->blobSize = CALLOC(R->columnCount, sizeof *R->blobSize);
        }
        return R;
}


/* -------------------------------------------------------- Delegate methods */


static void _free(T *R) {
        assert(R && *R);
        _clearBlobs(*R);
        FREE((*R)->blob);
        FREE((*R)->blobSize);
        FREE(*R);
}


static int _getColumnCount(T R) {
        assert(R);
        return R->columnCount;
}


static const char *_getColumnName(T R, int columnIndex) {
        assert(R);
        columnIndex--;
        if (R->columnCount <= 0 || columnIndex < 0 || columnIndex >= R->columnCount)
                return NULL;
        return PQfname(R->res, columnIndex);
}


static long _getColumnSize(T R, int columnIndex) {
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        if (PQgetisnull(R->res, R->currentRow, i))
                return 0;
        return PQgetlength(R->res, R->currentRow, i);
}


static bool _next(T R) {
        assert(R);
        _clearBlobs(R); // decoded blobs belong to the row we are leaving
        R->currentRow += 1;
        return (! ((R->currentRow >= R->rowCount) || (R->maxRows && (R->currentRow >= R->maxRows))));
}


static bool _isnull(T R, int columnIndex) {
        assert(R);
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        return PQgetisnull(R->res, R->currentRow, i);
}


static const char *_getString(T R, int columnIndex) {
        assert(R);
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        if (PQgetisnull(R->res, R->currentRow, i))
                return NULL;
        return PQgetvalue(R->res, R->currentRow, i);
}


// Decode the escaped bytea value into an owned per-column buffer (cached for the
// current row) rather than mutating the shared PGresult in place. This keeps
// getString() correct and makes repeated getBlob() on the same cell idempotent.
static const void *_getBlob(T R, int columnIndex, int *size) {
        assert(R);
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        *size = 0;
        if (PQgetisnull(R->res, R->currentRow, i))
                return NULL;
        if (! R->blob[i]) {
                const uchar_t *src = (const uchar_t*)PQgetvalue(R->res, R->currentRow, i);
                int len = PQgetlength(R->res, R->currentRow, i);
                uchar_t *dest = ALLOC(len + 1); // decoded form is never longer than the escaped form
                R->blobSize[i] = _unescape_bytea(src, len, dest);
                dest[R->blobSize[i]] = 0;
                R->blob[i] = dest;
        }
        *size = R->blobSize[i];
        return R->blob[i];
}


/* ------------------------------------------------------------------------- */


const struct Rop_T postgresqlrops = {
        .name           = "postgresql",
        .free           = _free,
        .getColumnCount = _getColumnCount,
        .getColumnName  = _getColumnName,
        .getColumnSize  = _getColumnSize,
        .next           = _next,
        .isnull         = _isnull,
        .getString      = _getString,
        .getBlob        = _getBlob
        // get/setFetchSize is not applicable for Postgres or rather libpq
        // getTimestamp and getDateTime is handled in ResultSet
};

