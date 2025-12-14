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
 * Get Oracle error code (ORA-XXXXX number) and message from an OCI error handle.
 * @param status The OCI return status code
 * @param err The OCI error handle
 * @param code Output parameter for the Oracle error code (e.g., 1 for ORA-00001), or NULL if not needed
 * @param buf Buffer to store error message, or NULL if message not needed
 * @param bufsize Size of buffer
 * @return Pointer to error message (buf if provided, or a static string)
 */
static inline const char *Oracle_getError(sword status, OCIError *err, int *code, char *buf, size_t bufsize) {
        sb4 errcode = 0;
        const char *msg;
        switch (status) {
                case OCI_SUCCESS:
                        msg = "";
                        break;
                case OCI_SUCCESS_WITH_INFO:
                        msg = "OCI_SUCCESS_WITH_INFO";
                        break;
                case OCI_NEED_DATA:
                        msg = "OCI_NEED_DATA";
                        break;
                case OCI_NO_DATA:
                        msg = "OCI_NO_DATA";
                        break;
                case OCI_INVALID_HANDLE:
                        msg = "OCI_INVALID_HANDLE";
                        break;
                case OCI_STILL_EXECUTING:
                        msg = "OCI_STILL_EXECUTING";
                        break;
                case OCI_CONTINUE:
                        msg = "OCI_CONTINUE";
                        break;
                case OCI_ERROR:
                        if (err) {
                                OCIErrorGet(err, 1, NULL, &errcode,
                                            buf ? (OraText *)buf : NULL,
                                            buf ? (ub4)bufsize : 0,
                                            OCI_HTYPE_ERROR);
                                msg = buf ? buf : "OCI_ERROR";
                        } else {
                                msg = "OCI_ERROR";
                        }
                        break;
                default:
                        msg = "Unknown OCI error";
        }
        if (code)
                *code = (int)errcode;
        return msg;
}

ResultSetDelegate_T OracleResultSet_new(Connection_T delegator, OCIStmt *stmt, OCIEnv *env, OCISession* usr, OCIError *err, OCISvcCtx *svc, int need_free) __attribute__ ((visibility("hidden")));
PreparedStatementDelegate_T OraclePreparedStatement_new(Connection_T delegator, OCIStmt *stmt, OCIEnv *env, OCISession* usr, OCIError *err, OCISvcCtx *svc) __attribute__ ((visibility("hidden")));

#endif // !ORACLEADAPTER_INCLUDED
