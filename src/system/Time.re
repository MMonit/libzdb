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
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <limits.h>

#include "Str.h"
#include "system/System.h"
#include "system/Time.h"


/**
 * Implementation of the Time interface
 *
 * ISO 8601: http://en.wikipedia.org/wiki/ISO_8601
 * @file
 */


/* ----------------------------------------------------------- Definitions */

#ifndef HAVE_TIMEGM
/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/* Counter the number of leap year in the range [0, y). The |y| is the
 year, including century (e.g., 2012) */
static int count_leap_year(int y)
{
        y -= 1;
        return y/4-y/100+y/400;
}


/* Returns nonzero if the |y| is the leap year. The |y| is the year,
 including century (e.g., 2012) */
static int is_leap_year(int y)
{
        return y%4 == 0 && (y%100 != 0 || y%400 == 0);
}


/* The number of days before ith month begins */
static int daysum[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};


/* Based on the algorithm of Python 2.7 calendar.timegm. */
time_t timegm(struct tm *tm)
{
        int days;
        int num_leap_year;
        int64_t t;
        if(tm->tm_mon > 11) {
                return -1;
        }
        num_leap_year = count_leap_year(tm->tm_year + 1900) - count_leap_year(1970);
        days = (tm->tm_year - 70) * 365 +
        num_leap_year + daysum[tm->tm_mon] + tm->tm_mday-1;
        if(tm->tm_mon >= 2 && is_leap_year(tm->tm_year + 1900)) {
                ++days;
        }
        t = ((int64_t)days * 24 + tm->tm_hour) * 3600 + tm->tm_min * 60 + tm->tm_sec;
        if(sizeof(time_t) == 4) {
                if(t < INT_MIN || t > INT_MAX) {
                        return -1;
                }
        }
        return t;
}
#endif /* !HAVE_TIMEGM */

#if HAVE_STRUCT_TM_TM_GMTOFF
#define TM_GMTOFF tm_gmtoff
#else
#define TM_GMTOFF tm_wday
#endif

#define _i2a(i, x) ((x)[0] = ((i) / 10) + '0', (x)[1] = ((i) % 10) + '0')

#define _isValidDate(tm) (((tm).tm_mday < 32 && (tm).tm_mday >= 1) && ((tm).tm_mon < 12 && (tm).tm_mon >= 0))
#define _isValidTime(tm) (((tm).tm_hour < 24 && (tm).tm_hour >= 0) && ((tm).tm_min < 60 && (tm).tm_min >= 0) && ((tm).tm_sec < 61 && (tm).tm_sec >= 0))


/* --------------------------------------------------------------- Private */


static inline int _a2i(const char *a, int l) {
        int n = 0;
        for (; *a && l--; a++)
                n = n * 10 + (*a - '0');
        return n;
}

static inline int _m2i(const char m[static 3]) {
        char month[3] = {[0] = tolower(m[0]), [1] = tolower(m[1]), [2] = tolower(m[2])};
        static char *months = "janfebmaraprmayjunjulaugsepoctnovdec";
        for (int i = 0; i < 34; i += 3) {
                if (memcmp(months + i, month, 3) == 0)
                        return i / 3;
        }
        return -1;
}


/* ----------------------------------------------------- Protected methods */


#ifdef PACKAGE_PROTECTED
#pragma GCC visibility push(hidden)
#endif


time_t Time_toTimestamp(const char *s) {
        if (STR_DEF(s)) {
                struct tm t = {};
                if (Time_toDateTime(s, &t)) {
                        t.tm_year -= 1900;
                        time_t offset = t.TM_GMTOFF;
                        return timegm(&t) - offset;
                }
        }
	return 0;
}


struct tm *Time_toDateTime(const char *s, struct tm *t) {
        assert(t);
        assert(s);
        struct tm tm = {.tm_isdst = -1}; 
        bool have_date = false, have_time = false;
        const char *yylimit = s + strlen(s), *yymarker, *yytoken, *yycursor = s;
	while (true) {
		if (yycursor >= yylimit) {
                        if (have_date || have_time) {
                                *(struct tm*)t = tm;
                                return t;
                        }
                        THROW(SQLException, "Invalid date or time");
                }
                yytoken = yycursor;
                /*!re2c
                 re2c:define:YYCTYPE         = "unsigned char";
                 re2c:define:YYCURSOR        = yycursor;
                 re2c:define:YYLIMIT         = yylimit;
                 re2c:define:YYMARKER        = yymarker;
                 re2c:yyfill:enable          = 0;
                 re2c:eof                    = 0;
                 re2c:flags:case-insensitive = 1;
                 
                 any  = [\000-\377];
                 x    = [^0-9];
                 dd   = [0-9][0-9];
                 yyyy = [0-9]{4};
                 tz   = [-+]dd(.? dd)?;
                 frac = [.,][0-9]+;
                 mmm  = ("jan"|"feb"|"mar"|"apr"|"may"|"jun"|"jul"|"aug"|"sep"|"oct"|"nov"|"dec");

                 $
                 { // EOF
                        THROW(SQLException, "Invalid date or time");
                 }

                 yyyy x dd x dd
                 { // Date: YYYY-MM-DD
                        tm.tm_year = _a2i(yytoken, 4);
                        tm.tm_mon  = _a2i(yytoken + 5, 2) - 1;
                        tm.tm_mday = _a2i(yytoken + 8, 2);
                        have_date  = _isValidDate(tm);
                        continue;
                 }
                 yyyy dd dd
                 { // Compressed Date: YYYYMMDD
                        tm.tm_year = _a2i(yytoken, 4);
                        tm.tm_mon  = _a2i(yytoken + 4, 2) - 1;
                        tm.tm_mday = _a2i(yytoken + 6, 2);
                        have_date  = _isValidDate(tm);
                        continue;
                 }
                 dd x dd x yyyy
                 { // Date: dd/mm/yyyy
                        tm.tm_mday = _a2i(yytoken, 2);
                        tm.tm_mon  = _a2i(yytoken + 3, 2) - 1;
                        tm.tm_year = _a2i(yytoken + 6, 4);
                        have_date  = _isValidDate(tm);
                        continue;
                 }
                 dd x mmm x yyyy
                 { // Date: Parse date part of RFC 7231 IMF-fixdate (HTTP date), e.g. Sun, 06 Nov 1994 08:49:37 GMT
                        tm.tm_mday = _a2i(yytoken, 2);
                        tm.tm_mon  = _m2i(yytoken + 3);
                        tm.tm_year = _a2i(yytoken + 7, 4);
                        have_date  = _isValidDate(tm);
                        continue;
                 }
                 dd x dd x dd frac?
                 { // Time: HH:MM:SS
                        tm.tm_hour = _a2i(yytoken, 2);
                        tm.tm_min  = _a2i(yytoken + 3, 2);
                        tm.tm_sec  = _a2i(yytoken + 6, 2);
                        have_time  = _isValidTime(tm);
                        continue;
                 }
                 dd dd dd frac?
                 { // Compressed Time: HHMMSS
                        tm.tm_hour = _a2i(yytoken, 2);
                        tm.tm_min  = _a2i(yytoken + 2, 2);
                        tm.tm_sec  = _a2i(yytoken + 4, 2);
                        have_time  = _isValidTime(tm);
                        continue;
                 }
                 dd ':' dd
                 { // Time: HH:MM
                        tm.tm_hour = _a2i(yytoken, 2);
                        tm.tm_min  = _a2i(yytoken + 3, 2);
                        tm.tm_sec  = 0;
                        have_time  = _isValidTime(tm);
                        continue;
                 }
                 tz
                 { // Timezone: +-HH:MM, +-HH or +-HHMM is offset from UTC in seconds
                        if (have_time) { // Only set timezone if we have parsed time
                                tm.TM_GMTOFF = _a2i(yytoken + 1, 2) * 3600;
                                if (isdigit(yytoken[3]))
                                        tm.TM_GMTOFF += _a2i(yytoken + 3, 2) * 60;
                                else if (isdigit(yytoken[4]))
                                        tm.TM_GMTOFF += _a2i(yytoken + 4, 2) * 60;
                                if (yytoken[0] == '-')
                                        tm.TM_GMTOFF *= -1;
                        }
                        continue;
                 }
                 any
                 {
                        continue;
                 }
                 */
        }
	return NULL;
}


char *Time_toString(time_t time, char result[static 20]) {
        assert(result);
        struct tm ts = {.tm_isdst = -1};
        gmtime_r(&time, &ts);
        memcpy(result, "YYYY-MM-DD HH:MM:SS\0", 20);
        /*              0    5  8  11 14 17 */
        _i2a((ts.tm_year+1900)/100, &result[0]);
        _i2a((ts.tm_year+1900)%100, &result[2]);
        _i2a(ts.tm_mon + 1, &result[5]); // Months in 01-12
        _i2a(ts.tm_mday, &result[8]);
        _i2a(ts.tm_hour, &result[11]);
        _i2a(ts.tm_min, &result[14]);
        _i2a(ts.tm_sec, &result[17]);
	return result;
}


time_t Time_now(void) {
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0)
                THROW(AssertException, "%s", System_getLastError());
	return t.tv_sec;
}


long long Time_milli(void) {
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0)
                THROW(AssertException, "%s", System_getLastError());
	return (long long)t.tv_sec * 1000  +  (long long)t.tv_usec / 1000;
}


long long Time_usleep(long long microseconds) {
        struct timespec req, rem;
        req.tv_sec = microseconds / USEC_PER_SEC;
        req.tv_nsec = (microseconds % USEC_PER_SEC) * 1000LL;
        while (nanosleep(&req, &rem) == -1) {
                if (errno == EINTR)
                        return rem.tv_sec * 1000000LL + rem.tv_nsec / 1000LL;
                if (errno == EINVAL)
                        return -1;
                req = rem;
        }
        return 0;
}


#ifdef PACKAGE_PROTECTED
#pragma GCC visibility pop
#endif
