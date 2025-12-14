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

#ifndef POSTGRESQLADAPTER_INCLUDED
#define POSTGRESQLADAPTER_INCLUDED

#include <libpq-fe.h>

#include "zdb.h"
#include "SQLState.h"


/**
 * Extract SQLSTATE error code from a PGresult.
 *
 * Returns the SQLSTATE code encoded as an integer for use in
 * Exception_frame.errorCode. Returns 0 if no SQLSTATE is available
 * or if the result is NULL.
 *
 * This function only returns SQLSTATE codes - it does not fall back
 * to PQresultStatus() to avoid mixing different error code domains.
 *
 * @param res The PGresult to extract error code from (may be NULL)
 * @return SQLSTATE as encoded integer, or 0 if unavailable
 */
static inline int _getSQLStateErrorCode(PGresult *res) {
        if (res) {
                const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
                if (sqlstate)
                        return SQLState_toInt(sqlstate);
        }
        return 0;
}


static inline const char* _getSQLErrorMessage(PGresult *res) {
        return res ? PQresultErrorMessage(res) : "unknown error";
}


ResultSetDelegate_T PostgresqlResultSet_new(Connection_T delegator, PGresult *res) __attribute__ ((visibility("hidden")));
PreparedStatementDelegate_T PostgresqlPreparedStatement_new(Connection_T delegator, PGconn *db, char *stmt, int parameterCount) __attribute__ ((visibility("hidden")));

#endif
