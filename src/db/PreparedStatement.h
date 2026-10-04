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


#ifndef PREPAREDSTATEMENT_INCLUDED
#define PREPAREDSTATEMENT_INCLUDED
#include <time.h>
//<< Protected methods
#include "PreparedStatementDelegate.h"
//>> End Protected methods


/**
 * @brief A **PreparedStatement** represents a single SQL statement pre-compiled
 * into byte code for later execution.
 *
 * The SQL statement may contain *in* parameters of the form `?`. Such parameters
 * represent unspecified literal values (or "wildcards") to be filled in later by the
 * various setter methods defined in this interface. Each *in* parameter has an
 * associated index number which is its sequence in the statement. The first
 * *in* '?' parameter has index 1, the next has index 2 and so on. A
 * PreparedStatement is created by calling Connection_prepareStatement().
 *
 * Consider this statement:
 * ```sql
 * INSERT INTO employee(name, photo) VALUES(?, ?)
 * ```
 * There are two *in* parameters in this statement, the parameter for setting
 * the name has index 1 and the one for the photo has index 2. To set the
 * values for the *in* parameters we use a setter method. Assuming name has
 * a string value we use PreparedStatement_setString(). To set the value
 * of the photo we submit a binary value using the
 * method PreparedStatement_setBlob().
 *
 * ## Example
 *
 * To summarize, here is the code in context.
 *
 * ```c
 * PreparedStatement_T p = Connection_prepareStatement(con, "INSERT INTO employee(name, photo) VALUES(?, ?)");
 * PreparedStatement_setString(p, 1, "Kamiya Kaoru");
 * PreparedStatement_setBlob(p, 2, jpeg, jpeg_size);
 * PreparedStatement_execute(p);
 * ```
 *
 * ## Reuse
 *
 * A PreparedStatement can be reused. That is, the method
 * PreparedStatement_execute() can be called one or more times to execute
 * the same statement. Clients can also set new *in* parameter values and
 * re-execute the statement as shown in this example:
 *
 * ```c
 * PreparedStatement_T p = Connection_prepareStatement(con, "INSERT INTO employee(name, photo) VALUES(?, ?)");
 * for (int i = 0; employees[i]; i++)
 * {
 *        PreparedStatement_setString(p, 1, employees[i].name);
 *        PreparedStatement_setBlob(p, 2, employees[i].photo.data, employees[i].photo.size);
 *        PreparedStatement_execute(p);
 * }
 * ```
 *
 * ## Result Sets
 *
 * Here is another example where we use a Prepared Statement to execute a query
 * which returns a Result Set:
 *
 * ```c
 * PreparedStatement_T p = Connection_prepareStatement(con, "SELECT id FROM employee WHERE name LIKE ?");
 * PreparedStatement_setString(p, 1, "%oru%");
 * ResultSet_T r = PreparedStatement_executeQuery(p);
 * while (ResultSet_next(r))
 *        printf("employee.id = %d\n", ResultSet_getInt(r, 1));
 * ```
 *
 * A ResultSet returned from PreparedStatement_executeQuery() is valid until
 * the Prepared Statement is executed again or until the Connection is
 * returned to the Connection Pool.
 *
 * ## Date and Time
 *
 * PreparedStatement_setTimestamp() can be used to set a Unix timestamp value as
 * a `time_t` type. To set Date, Time or DateTime values, simply use
 * PreparedStatement_setString() to set a time string in a format understood by
 * your database. For instance to set a SQL Date value,,
 * ```c
 * PreparedStatement_setString(p, parameterIndex, "2019-12-28");
 * ```
 *
 * ## SQL Injection Prevention
 *
 * Prepared Statement is particularly useful when dealing with user-submitted data, 
 * as properly used Prepared Statements provide strong protection against SQL
 * injection attacks. By separating SQL logic from data, PreparedStatements ensure
 * that user input is treated as data only, not as part of the SQL command.
 *
 * *A PreparedStatement is reentrant, but not thread-safe and should only be used
 * by one thread (at a time).*
 *
 * @note
 * - Remember that parameter indices in PreparedStatement are 1-based, not 0-based.
 * - To minimizes memory allocation and avoid unnecessary data copying, string
 *   and blob values are set by reference and MUST remain valid until either
 *   PreparedStatement_execute() or PreparedStatement_executeQuery() is called.
 *
 * @warning
 * The '?' character should only be used as a parameter placeholder in your SQL
 * statement. Do not use '?' in string literals, comments, or other SQL contexts.
 *
 * @see Connection.h ResultSet.h SQLException.h
 * @file
 */


#define T PreparedStatement_T
typedef struct PreparedStatement_S *T;

//<< Protected methods

/**
 * @brief Create a new PreparedStatement.
 * @param D the delegate used by this PreparedStatement
 * @param op delegate operations
 * @return A new PreparedStatement object
 */
T PreparedStatement_new(PreparedStatementDelegate_T D, Pop_T op) __attribute__ ((visibility("hidden")));


/**
 * @brief Destroy a PreparedStatement and release allocated resources.
 * @param P A PreparedStatement object reference
 */
void PreparedStatement_free(T *P) __attribute__ ((visibility("hidden")));


/**
 * @brief Close the ResultSet of this PreparedStatement, if any.
 * @param P A PreparedStatement object
 */
void PreparedStatement_clearResultSet(T P) __attribute__ ((visibility("hidden")));

//>> End Protected methods

/// @name Parameters
/// @{

/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the given string value.
 *
 * This method is less efficient than PreparedStatement_setSString() as it needs
 * to calculate the string length. Use PreparedStatement_setSString() if you know
 * the size of the string.
 *
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2, ...
 * @param x The string value to set. The string must be a '\0' terminated C-string.
 *          NULL is allowed to indicate a SQL NULL value.
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 * @see PreparedStatement_setSString
 */
void PreparedStatement_setString(T P, int parameterIndex, const char *x);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the given `sized`
 * string value.
 *
 * This method is more efficient than PreparedStatement_setString() as it doesn't
 * need to calculate the string length.
 *
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2, ...
 * @param x The string value to set. The string need not be '\0' terminated.
 *          NULL is allowed to indicate a SQL NULL value.
 * @param size The length of the byte string. For instance, the value returned
 *             by `strlen(3)`. If size is negative, it will be treated as 0.
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 * @see PreparedStatement_setString
 */
void PreparedStatement_setSString(T P, int parameterIndex, const char *x, int size);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the given int value.
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2,..
 * @param x The int value to set
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 */
void PreparedStatement_setInt(T P, int parameterIndex, int x);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the given long long value.
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2,..
 * @param x The long long value to set
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 */
void PreparedStatement_setLLong(T P, int parameterIndex, long long x);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the given double value.
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2,..
 * @param x The double value to set
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 */
void PreparedStatement_setDouble(T P, int parameterIndex, double x);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the given blob value.
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2,..
 * @param x The blob value to set. NULL is allowed to indicate a SQL NULL value
 * @param size The number of bytes in the blob. If size is negative, it will be treated as 0.
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 */
void PreparedStatement_setBlob(T P, int parameterIndex, const void *x, int size);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to the
 * given Unix timestamp value.
 *
 * The timestamp value given in `x` is expected to be a UTC timestamp,
 * representing the number of seconds since the Unix epoch, regardless of
 * the system's local timezone. For instance, a value returned by `time(3)`
 * is appropriate for this parameter.
 *
 * Note on database-specific behavior:
 * - SQLite: Stores the time_t value as a 64-bit integer. This preserves
 *   the exact UTC timestamp, which can be correctly interpreted in any 
 *   timezone when retrieved.
 * - MySQL, PostgreSQL and Oracle: Convert and store the timestamp in their
 *   respective datetime formats, preserving the UTC value.
 *
 * This approach ensures consistent timestamp handling across different timezones
 * and database systems. When retrieving the timestamp, use appropriate time
 * conversion functions to interpret the value in the desired timezone.
 *
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2, ...
 * @param x The UTC timestamp value to set. E.g., a value returned by time(3)
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 * @see ResultSet_getTimestamp
 */
void PreparedStatement_setTimestamp(T P, int parameterIndex, time_t x);


/**
 * @brief Sets the *in* parameter at index `parameterIndex` to SQL NULL.
 * @param P A PreparedStatement object
 * @param parameterIndex The first parameter is 1, the second is 2,..
 * @exception SQLException If a database access error occurs or if parameter
 * index is out of range
 * @see SQLException.h
 */
void PreparedStatement_setNull(T P, int parameterIndex);

/// @}
/// @name Functions
/// @{

/**
 * @brief Executes the prepared SQL statement.
 *
 * Executes the prepared SQL statement, which may be an INSERT, UPDATE,
 * or DELETE statement or an SQL statement that returns nothing, such
 * as an SQL DDL statement.
 *
 * @param P A PreparedStatement object
 * @exception SQLException If a database error occurs
 * @see SQLException.h
 */
void PreparedStatement_execute(T P);


/**
 * @brief Executes the prepared SQL query.
 *
 * Executes the prepared SQL statement, which returns a single ResultSet
 * object. A ResultSet is valid until the next call to a PreparedStatement
 * method or until the Connection is returned to the Connection Pool.
 * *This means that Result Sets cannot be saved between queries*.
 *
 * @param P A PreparedStatement object
 * @return A ResultSet object that contains the data produced by the prepared
 * statement.
 * @exception SQLException If a database error occurs
 * @see ResultSet.h
 * @see SQLException.h
 */
ResultSet_T PreparedStatement_executeQuery(T P);


/**
 * @brief Gets the number of rows affected by the most recent SQL statement.
 *
 * If used with a transaction, this method should be called *before* commit is
 * executed, otherwise 0 is returned.
 *
 * @param P A PreparedStatement object
 * @return The number of rows changed by the last (DIM) SQL statement
 */
long long PreparedStatement_rowsChanged(T P);

/// @}
/// @name Properties
/// @{

/**
 * @brief Gets the number of parameters in the prepared statement.
 * @param P A PreparedStatement object
 * @return The number of _in_ parameters in this prepared statement
 */
int PreparedStatement_getParameterCount(T P);

/// @}
#undef T
#endif
