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
#include <ctype.h>

#include "SQLState.h"


/**
 * Implementation of the SQLState interface.
 *
 * SQLSTATE codes are encoded using base-36 arithmetic where:
 * - Digits '0'-'9' map to values 0-9
 * - Letters 'A'-'Z' map to values 10-35
 *
 * A 5-character SQLSTATE encodes to:
 *   value = c[0]*36^4 + c[1]*36^3 + c[2]*36^2 + c[3]*36 + c[4]
 *
 * Maximum value for "ZZZZZ" is 60,466,175 which fits in a 32-bit integer.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


static const char _kBase36Digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


/* -------------------------------------------------------- Public methods */


int SQLState_toInt(const char *sqlstate) {
        if (STR_UNDEF(sqlstate))
                return 0;
        int result = 0;
        for (int i = 0; i < 5; i++) {
                if (sqlstate[i] == '\0')
                        return 0;  // String too short
                char c = toupper((unsigned char)sqlstate[i]);
                int val;
                if (c >= '0' && c <= '9')
                        val = c - '0';
                else if (c >= 'A' && c <= 'Z')
                        val = c - 'A' + 10;
                else
                        return 0;  // Invalid character
                result = result * 36 + val;
        }
        return result;
}


char *SQLState_toString(int code, char buf[static 6]) {
        if (code <= 0) {
                buf[0] = '\0';
                return NULL;
        }
        for (int i = 4; i >= 0; i--) {
                buf[i] = _kBase36Digits[code % 36];
                code /= 36;
        }
        buf[5] = '\0';
        return buf;
}
