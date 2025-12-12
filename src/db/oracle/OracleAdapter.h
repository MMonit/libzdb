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

#ifndef ORACLEADAPTER_INCLUDED
#define ORACLEADAPTER_INCLUDED

#include <oci.h>

#include "zdb.h"

#define ORACLE_ERR_SIZE 512

/**
 * Get Oracle error message for the given status code.
 * @param status The OCI return status code
 * @param err The OCI error handle
 * @param buf Buffer to store error message
 * @param bufsize Size of buffer
 * @return Pointer to buf containing the error message
 */
static inline const char *Oracle_getError(sword status, OCIError *err, char *buf, size_t bufsize) {
        sb4 errcode;
        switch (status) {
                case OCI_SUCCESS:
                        return "";
                case OCI_SUCCESS_WITH_INFO:
                        return "OCI_SUCCESS_WITH_INFO";
                case OCI_NEED_DATA:
                        return "OCI_NEED_DATA";
                case OCI_NO_DATA:
                        return "OCI_NO_DATA";
                case OCI_INVALID_HANDLE:
                        return "OCI_INVALID_HANDLE";
                case OCI_STILL_EXECUTING:
                        return "OCI_STILL_EXECUTING";
                case OCI_CONTINUE:
                        return "OCI_CONTINUE";
                case OCI_ERROR:
                        if (err && buf && bufsize > 0) {
                                OCIErrorGet(err, 1, NULL, &errcode, (OraText *)buf, (ub4)bufsize, OCI_HTYPE_ERROR);
                                return buf;
                        }
                        return "OCI_ERROR";
                default:
                        return "Unknown OCI error";
        }
}

ResultSetDelegate_T OracleResultSet_new(Connection_T delegator, OCIStmt *stmt, OCIEnv *env, OCISession* usr, OCIError *err, OCISvcCtx *svc, int need_free) __attribute__ ((visibility("hidden")));
PreparedStatementDelegate_T OraclePreparedStatement_new(Connection_T delegator, OCIStmt *stmt, OCIEnv *env, OCISession* usr, OCIError *err, OCISvcCtx *svc) __attribute__ ((visibility("hidden")));

#endif // !ORACLEADAPTER_INCLUDED
