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


// Replace all occurences of ? in this string buffer with prefix[1..99]. The scan is context-aware: a "?" character inside a string literal, a quoted identifier, a "-- comment"
// or a /*...*/ block comment is not a placeholder parameter, but part of SQL content, and need to be left untouched.
static int _prepare(T S, char prefix) {
        int placeholderCount = 0;
        int positions[99]; // Maximum 99 parameters allowed

        // 1st loop: Count placeholder parameters and record their offset
        for (int i = 0; S->buffer[i]; i++) {
                uchar_t c = S->buffer[i];
                if (c == '\'' || c == '"') {
                        // Start of quoted-string or quoted-identifier, ignore "?" in this context
                        i++;
                        while (S->buffer[i]) {
                                if (S->buffer[i] == c) {
                                        // Found quote (' or "), perform lookahead to check if it's escaped quote ('' or "")
                                        if (S->buffer[i + 1] == c) {
                                                // Escaped quote, still inside the quoted-string or quoted-identifier
                                                i += 2;
                                        } else {
                                                // Closing quote (' or ")
                                                break;
                                        }
                                } else {
                                        // Another character inside the quoted-string or quoted-identifier
                                        i++;
                                }
                        }
                        if (! S->buffer[i]) {
                                // End of string => unterminated literal
                                break;
                        }
                } else if (c == '-' && S->buffer[i + 1] == '-') {
                        // Line comment: the rest of this line past "--" should be ignored
                        i += 2;
                        while (S->buffer[i] && S->buffer[i] != '\n') {
                                // Another line character
                                i++;
                        }
                        if (! S->buffer[i]) {
                                // End of string
                                break;
                        }
                } else if (c == '/' && S->buffer[i + 1] == '*') {
                        // Start of block comment
                        int depth = 1; // Nested block comments are allowed in PostgreSQL => depth counter
                        i += 2;
                        while (S->buffer[i] && depth > 0) {
                                if (S->buffer[i] == '/' && S->buffer[i + 1] == '*') {
                                        // Nested block comment start
                                        depth++;
                                        i += 2;
                                } else if (S->buffer[i] == '*' && S->buffer[i + 1] == '/') {
                                        // Block comment end
                                        depth--;
                                        i += 2;
                                } else {
                                        // Character inside comment
                                        i++;
                                }
                        }
                        i--; // compensate the outer i++
                } else if (c == '?') {
                        // A placeholder parameter (not part of literal nor comment)
                        if (placeholderCount < 99)
                                positions[placeholderCount] = i;
                        placeholderCount++;
                }
        }

        if (placeholderCount > 99) {
                // Sanity check
                THROW(SQLException, "Max 99 parameters are allowed in a prepared statement. Found %d parameters in statement", placeholderCount);
        } else if (placeholderCount) {
                // At least one placeholder parameter is present

                // How many extra bytes we need for placeholder escaping. E.g. postgresql uses "$<number>" pattern => need extra 1 byte for "?" -> "$[1-9]" and extra 2 bytes
                // for "$[10-99]" (ditto Oracle pattern ":[1-99]")
                int extra = (placeholderCount <= 9) ? placeholderCount : (2 * placeholderCount - 9);

                // Sanity check: StringBuffer length is limited to 'int' maximum => make sure the new statement size won't wrap the INT_MAX, as (used + extra + 1) bytes are needed
                if (extra >= INT_MAX - S->used)
                        THROW(AssertException, "StringBuffer: content exceeds the maximum size of %d bytes", INT_MAX);

                int new_used = S->used + extra;
                if (new_used >= S->length) {
                        // RESIZE may throw error on out-of-memory, let it bubble up
                        RESIZE(S->buffer, new_used + 1);
                        S->length = new_used + 1;
                }

                // 2nd loop: right-to-left escaping: Move the characters to the right and replace "?" placeholders with db specific pattern (e.g. "$<number>" for postgresql)
                int r = S->used - 1;
                int w = new_used - 1;
                int currentPlaceholder = placeholderCount;
                while (r >= 0) {
                        if (currentPlaceholder > 0 && r == positions[currentPlaceholder - 1]) {
                                // The character is on the "?" placeholder position, identified during the first loop => escape
                                if (currentPlaceholder >= 10) {
                                        // Two digits
                                        S->buffer[w--] = '0' + (currentPlaceholder % 10);
                                        S->buffer[w--] = '0' + (currentPlaceholder / 10);
                                } else {
                                        // One digit
                                        S->buffer[w--] = '0' + currentPlaceholder;
                                }
                                // Push db-specific prefix ("$" for PostgreSQL, ":" for Oracle) in front of the parameter <number> we just printed to the statement
                                S->buffer[w--] = prefix;
                                currentPlaceholder--;
                        } else {
                                S->buffer[w--] = S->buffer[r];
                        }
                        r--;
                }
                S->used = new_used;
                S->buffer[S->used] = 0;
        }
        return placeholderCount;
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

