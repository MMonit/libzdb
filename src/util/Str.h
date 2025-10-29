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


#ifndef STR_INCLUDED
#define STR_INCLUDED
#include <stdarg.h>


/**
 * General purpose <b>String</b> <b>Class methods</b>.
 *
 * @file
 */


/**
 * Test if the given string is defined. That is; not NULL nor the empty ("") string
 * @param s The string to test
 * @return true if s is defined, otherwise false
 * @hideinitializer
 */
#define STR_DEF(s) ((s) && *(s))


/**
 * Test if the given string is NULL or the empty ("") string
 * @param s The string to test
 * @return true if s is NULL or the empty string, otherwise false
 * @hideinitializer
 */
#define STR_UNDEF(s) (! STR_DEF(s))


/**
 * Returns true if the string <i>a</i> equals the string <i>b</i>. The
 * test is <i>case-insensitive</i> but depends on that all characters
 * in the two strings can be translated in the current locale.
 * @param a The string to test for equality with <code>b</code>
 * @param b The string to test for equality with <code>a</code>
 * @return true if a equals b, otherwise false
 */
bool Str_isEqual(const char *a, const char *b);


/**
 * Returns true if the string <i>a</i> equals the string <i>b</i>. The
 * test is <i>case-sensitive</i> and compares byte by byte 
 * @param a The string to test for equality with <code>b</code>
 * @param b The string to test for equality with <code>a</code>
 * @return true if a equals b, otherwise false
 */
bool Str_isByteEqual(const char *a, const char *b);


/**
 * Returns true if the string <i>a</i> starts with the sub-string
 * <i>b</i>. The test is <i>case-sensitive</i>.
 * @param a The string to search for b in
 * @param b The <i>sub-string</i> to test a against
 * @return true if a starts with b, otherwise false
 */
bool Str_startsWith(const char *a, const char *b);


/**
 * Returns the first substring of <i>a</i> that match the string <code>b</code>.
 * If any of the parameters are NULL or <code>b</code> is an empty string,
 * NULL is returned. The test is <i>case-insensitive</i>.
 * Example:
 * <pre>
 * Str_sub("Haystack with Needle", "needle") -> "Needle"
 * Str_sub("foo bar baz", "bar") -> "bar baz"
 * Str_sub("foo", "foo bar") -> NULL
 * Str_sub("foo", "") -> NULL
 * </pre>
 * @param a The string to search for b in
 * @param b The sub-string to search for in a
 * @return A pointer to the start of the substring in a that contains b,
 * otherwise NULL
 */
char *Str_sub(const char *a, const char *b);


/**
 * Returns true if <i>s</i> is equal to a string in the <code>set</code> 
 * of strings. It is an unchecked runtime error for the last element of
 * the set not to be NULL
 * <pre>
 * char *ext = "jpg";
 * const char *set[] = {"gif", "png", "jpg", "jpeg", "tiff", NULL};
 * Str_member(ext, set) -> true
 * Str_member("gif", (const char**){"foo", "bar", NULL}) -> false
 * Str_member(ext, (const char*[]){"jpgn", NULL}) -> false
 * Str_member(ext, NULL) -> false
 * Str_member(NULL, NULL) -> false
 * </pre>
 * @param s The string to test
 * @param set An array of C-strings. The last element in the array must be NULL
 * @return true if s is a member of the set, otherwise false
 */
bool Str_member(const char *s, const char **set);


/**
 * Strcpy that copy only <code>n</code> char from the given
 * string. The destination string, <code>dest</code>, is NUL
 * terminated at length <code>n</code> or if <code>src</code> is
 * shorter than <code>n</code> at the length of <code>src</code>
 * @param dest The destination buffer
 * @param src The string to copy to dest
 * @param n The number of bytes to copy
 * @return A pointer to dest
 */
char *Str_copy(char *dest, const char *src, int n);


/**
 * Returns a copy of <code>s</code>. The caller must free the returned String.
 * @param s A String to duplicate
 * @return A pointer to the duplicated string, NULL if s is NULL
 * @exception MemoryException if allocation failed
 */
char *Str_dup(const char *s);


/**
 * Strdup that duplicates only n char from the given string The caller 
 * must free the returned String. If s is less than n characters long, all 
 * characters of s are copied. I.e. the same as calling Str_dup(s).
 * @param s A string to duplicate
 * @param n The number of bytes to copy from s
 * @return A pointer to the duplicated string, NULL if s is NULL
 * @exception MemoryException if allocation failed
 * @exception AssertException if n is less than 0
 */
char *Str_ndup(const char *s, int n);


/**
 * Creates a new String by merging a formated string and a variable
 * argument list. The caller must free the returned String.
 * @param s A format string
 * @return The new String or NULL if the string could not be created
 * @exception MemoryException if memory allocation fails
 */
char *Str_cat(const char *s, ...) __attribute__((format (printf, 1, 2)));


/**
 * Creates a new String by merging a formated string and a variable
 * argument list. The caller must free the returned String.
 * @param s A format string
 * @param ap A variable argument lists
 * @return a new String concating s and va_list or NULL on error
 * @exception MemoryException if memory allocation fails
 */
char *Str_vcat(const char *s, va_list ap);


/**
 * Parses the string argument as a signed integer in base 10.
 * @param s A string
 * @return The integer represented by the string argument.
 * @exception SQLException If a parse error occurred
 */
int Str_parseInt(const char *s);


/**
 * Parses the string argument as a signed long long in base 10.
 * @param s A string
 * @return The long long represented by the string argument.
 * @exception SQLException If a parse error occurred
 */
long long Str_parseLLong(const char *s);


/**
 * Parses the string argument as a double.
 * @param s A string
 * @return The double represented by the string argument.
 * @exception SQLException If a parse error occurred
 */
double Str_parseDouble(const char *s);


/**
 * Parses the string argument as a boolean value. It checks if the string,
 * after ignoring leading whitespace, starts with any of the following
 * values: "true", "yes", "1", "on", "enable", or "enabled", followed by
 * either whitespace or the end of the string. If such a pattern is found,
 * the function returns true. For any other value or pattern, it returns false.
 * Example:
 * <pre>
 * Str_parseBool("true")        -> true
 * Str_parseBool("TRUE")        -> true
 * Str_parseBool(" yes ")       -> true
 * Str_parseBool("yesterday")   -> false
 * </pre>
 * @param s A string representing a boolean value.
 * @return true if 's' starts with a boolean value ("true", "yes", "1", "on",
 * "enable", "enabled"), followed by a space or the end of the string,
 * false otherwise. The comparison is case-insensitive. If 's' is NULL or
 * the empty string, the function returns false.
 */
bool Str_parseBool(const char *s);


#endif
