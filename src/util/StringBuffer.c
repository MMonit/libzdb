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
#include <limits.h>

#include "StringBuffer.h"


/**
 * Implementation of the StringBuffer interface.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#define T StringBuffer_T
struct T {
        int used;
        int length;
	uchar_t *buffer;
};


/* ------------------------------------------------------- Private methods */


static inline void _append(T S, const char *s, va_list ap) {
        va_list ap_copy;
        while (true) {
                va_copy(ap_copy, ap);
                int n = vsnprintf((char*)(S->buffer + S->used), S->length - S->used, s, ap_copy);
                va_end(ap_copy);
                if (n < 0)
                        THROW(AssertException, "StringBuffer: vsnprintf failed");
                // The buffer size is an int; reject content that would not fit
                // (used + n + 1 bytes are needed) rather than overflowing to a
                // negative length and passing garbage sizes to RESIZE.
                if (n >= INT_MAX - S->used)
                        THROW(AssertException, "StringBuffer: content exceeds the maximum size of %d bytes", INT_MAX);
                if ((S->used + n) < S->length) {
                        S->used += n;
                        break;
                }
                int need = S->used + n + 1; // cannot overflow: n < INT_MAX - used
                int length = (need > INT_MAX - STRLEN) ? INT_MAX : need + STRLEN;
                // Commit the new length only after RESIZE succeeds: RESIZE throws
                // MemoryException on OOM leaving the old (smaller) buffer in place,
                // and length must keep describing the buffer we actually own
                RESIZE(S->buffer, length);
                S->length = length;
        }
}


// Replace all occurences of ? in this string buffer with prefix[1..99]
static int _prepare(T S, char prefix) {
        int n, i;
        for (n = i = 0; S->buffer[i]; i++) if (S->buffer[i] == '?') n++;
        if (n > 99)
                THROW(SQLException, "Max 99 parameters are allowed in a prepared statement. Found %d parameters in statement", n);
        else if (n) {
                int extra = (n <= 9) ? n : (2 * n - 9);
                if (extra >= INT_MAX - S->used)
                        THROW(AssertException, "StringBuffer: content exceeds the maximum size of %d bytes", INT_MAX);
                int new_used = S->used + extra;
                if (new_used >= S->length) {
                        // RESIZE before committing length, see _append()
                        RESIZE(S->buffer, new_used + 1);
                        S->length = new_used + 1;
                }
                int r = S->used - 1;
                int w = new_used - 1;
                int j = n;
                while (r >= 0) {
                        if (S->buffer[r] == '?') {
                                if (j >= 10) {
                                        S->buffer[w--] = '0' + (j % 10);
                                        S->buffer[w--] = '0' + (j / 10);
                                } else {
                                        S->buffer[w--] = '0' + j;
                                }
                                S->buffer[w--] = prefix;
                                j--;
                        } else {
                                S->buffer[w--] = S->buffer[r];
                        }
                        r--;
                }
                S->used = new_used;
                S->buffer[S->used] = 0;
        }
        return n;
}


static inline T _ctor(int hint) {
        T S;
        NEW(S);
        S->length = hint;
        S->buffer = ALLOC(hint);
        *S->buffer = 0;
        return S;
}


/* ----------------------------------------------------- Protected methods */


#ifdef PACKAGE_PROTECTED
#pragma GCC visibility push(hidden)
#endif

T StringBuffer_new(const char *s) {
        return StringBuffer_append(_ctor(STRLEN), "%s", s);
}


T StringBuffer_create(int hint) {
        if (hint <= 0)
                THROW(AssertException, "Illegal hint value");
        return _ctor(hint);
}


void StringBuffer_free(T *S) {
        assert(S && *S);
	FREE((*S)->buffer);
        FREE(*S);
}


T StringBuffer_append(T S, const char *s, ...) {
        assert(S);
        if (STR_DEF(s)) {
                va_list ap;
                va_start(ap, s);
                _append(S, s, ap);
                va_end(ap);
        }
        return S;
}


T StringBuffer_vappend(T S, const char *s, va_list ap) {
        assert(S);
        if (STR_DEF(s))
                _append(S, s, ap);
        return S;
}


T StringBuffer_set(T S, const char *s, ...) {
	assert(S);
        StringBuffer_clear(S);
        if (STR_DEF(s)) {
                va_list ap;
                va_start(ap, s);
                _append(S, s, ap);
                va_end(ap);
        }
        return S;
}


T StringBuffer_vset(T S, const char *s, va_list ap) {
	assert(S);
        StringBuffer_clear(S);
        if (STR_DEF(s))
                _append(S, s, ap);
        return S;
}


int StringBuffer_length(T S) {
        assert(S);
        return S->used;
}


T StringBuffer_clear(T S) {
        assert(S);
        S->used = 0;
        *S->buffer = 0;
        return S;
}


const char *StringBuffer_toString(T S) {
        assert(S);
        return (const char*)S->buffer;
}


int StringBuffer_prepare4postgres(T S) {
        assert(S);
        return _prepare(S, '$');
}


int StringBuffer_prepare4oracle(T S) {
        assert(S);
        return _prepare(S, ':');
}


T StringBuffer_trim(T S) {
        assert(S);
        if (S->used == 0)
                return S;
        // Right trim
        uchar_t *end = S->buffer + S->used - 1;
        if (isspace(*end)) {
                while (end >= S->buffer && isspace(*end))
                        end--;
                S->used = (int)(end - S->buffer) + 1;
                S->buffer[S->used] = 0;
        }
        // Left trim
        if (S->used > 0 && isspace(*S->buffer)) {
                uchar_t *start = S->buffer + 1;
                while (isspace(*start)) start++;
                int shift = (int)(start - S->buffer);
                S->used -= shift;
                memmove(S->buffer, start, S->used + 1);
        }
        return S;
}


#ifdef PACKAGE_PROTECTED
#pragma GCC visibility pop
#endif

