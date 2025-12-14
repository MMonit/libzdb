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


#ifndef CONNECTION_INCLUDED
#define CONNECTION_INCLUDED


/**
 * @brief A **Connection** represents a connection to a SQL database system.
 *
 * Use a Connection to execute SQL statements. There are three ways to
 * execute statements: Connection_execute() is used to execute SQL
 * statements that do not return a result set. Such statements are INSERT,
 * UPDATE or DELETE. Connection_executeQuery() is used to execute a SQL
 * SELECT statement and return a result set. These methods can only handle
 * values which can be expressed as C-strings. If you need to handle binary
 * data, such as inserting a blob value into the database, use a
 * PreparedStatement object to execute the SQL statement. The factory method
 * Connection_prepareStatement() is used to obtain a PreparedStatement object.
 *
 * The method Connection_executeQuery() will return an empty ResultSet (not null)
 * if the SQL statement did not return any values. A ResultSet is valid until the
 * next call to Connection execute or until the Connection is returned
 * to the Connection Pool. If an error occurs during execution, an SQLException
 * is thrown.
 *
 * Any SQL statement that changes the database (basically, any SQL
 * command other than SELECT) will automatically start a transaction
 * if one is not already in effect. Automatically started transactions
 * are committed at the conclusion of the command.
 *
 * Transactions can also be started manually using
 * Connection_beginTransaction(). Such transactions usually persist
 * until the next call to Connection_commit() or Connection_rollback().
 * A transaction will also rollback if the database is closed or if an
 * error occurs. Nested transactions are not allowed.
 *
 * ## Examples
 *
 * ### Basic Query Execution
 * @code
 * Connection_T con = ConnectionPool_getConnection(pool);
 * if (con) {
 *     ResultSet_T result = Connection_executeQuery(con, "SELECT name, age FROM users WHERE id = %d", 1);
 *     if (ResultSet_next(result)) {
 *         const char* name = ResultSet_getString(result, 1);
 *         int age = ResultSet_getInt(result, 2);
 *         printf("Name: %s, Age: %d\n", valueOr(name, "N/A"), age);
 *     }
 *     Connection_close(con);
 * }
 * @endcode
 *
 * ### Transaction Example
 * @code
 * Connection_T con = NULL;
 * TRY
 * {
 *     con = ConnectionPool_getConnectionOrException(pool);
 *     Connection_beginTransaction(con);
 *     Connection_execute(con, "UPDATE accounts SET balance = balance - %f WHERE id = %d", 100.0, 1);
 *     Connection_execute(con, "UPDATE accounts SET balance = balance + %f WHERE id = %d", 100.0, 2);
 *     Connection_commit(con);
 *     printf("Transfer successful\n");
 * }
 * ELSE
 * {
 *     // The error message in Exception_frame.message specify the error that occured
 *     printf("Transfer failed: %s\n", Exception_frame.message);
 *
 *     // Connection_close() will automatically call Connection_rollback() if
 *     // the connection is in an uncommitted transaction
 * }
 * FINALLY
 * {
 *    if (con) Connection_close(con);
 * }
 * END_TRY;
 * @endcode
 *
 * ### Using PreparedStatement
 * @code
 * Connection_T con = ConnectionPool_getConnection(pool);
 * if (con) {
 *     const char *sql = "INSERT INTO logs (message, timestamp) VALUES (?, ?)";
 *     PreparedStatement_T stmt = Connection_prepareStatement(con, sql);
 *     PreparedStatement_setString(stmt, 1, "User logged in");
 *     PreparedStatement_setTimestamp(stmt, 2, time(NULL));
 *     PreparedStatement_execute(stmt);
 *     printf("Rows affected: %lld\n", PreparedStatement_rowsChanged(stmt));
 *     Connection_close(con);
 * }
 * @endcode
 *
 * *A Connection is reentrant, but not thread-safe and should only be used
 * by one thread (at a time).*
 *
 * @note When Connection_close() is called on a Connection object, it is
 * automatically returned to the pool. If the connection is still in a
 * transaction at this point, the transaction will be automatically rolled
 * back. This ensures data integrity even when exceptions occur. It's
 * recommended to always call Connection_close() in a FINALLY block to
 * guarantee proper resource management and transaction handling. See the
 * Transaction Example above for a practical demonstration of this behavior.
 *
 * @see ResultSet.h PreparedStatement.h SQLException.h
 * @file
 */


#define T Connection_T
typedef struct Connection_S *T;


/**
 * Enum representing different transaction isolation levels and behaviors.
 * Support for specific types varies depending on the database system being used.
 *
 * Note: All transactions must be explicitly ended with either a commit or a rollback 
 * operation, regardless of the isolation level or database system.
 */
typedef enum {
    /**
     * Use the default transaction behavior of the underlying database system.
     * - MySQL: REPEATABLE READ
     * - PostgreSQL: READ COMMITTED
     * - Oracle: READ COMMITTED
     * - SQLite: SERIALIZABLE
     */
    TRANSACTION_DEFAULT = 0,

    /**
     * Lowest isolation level. Transactions can read uncommitted data.
     * Supported by: MySQL.
     * Not supported by: PostgreSQL, Oracle, SQLite
     */
    TRANSACTION_READ_UNCOMMITTED,

    /**
     * Prevents dirty reads. A transaction only sees data committed before the transaction began.
     * Supported by: MySQL, PostgreSQL, Oracle.
     * Not applicable to SQLite (always SERIALIZABLE)
     */
    TRANSACTION_READ_COMMITTED,

    /**
     * Prevents non-repeatable reads.
     * Supported by: MySQL, PostgreSQL.
     * Not supported by: Oracle.
     * Not applicable to SQLite (always SERIALIZABLE)
     */
    TRANSACTION_REPEATABLE_READ,

    /**
     * Highest isolation level. Prevents dirty reads, non-repeatable reads, and phantom reads.
     * Supported by: MySQL, PostgreSQL, Oracle.
     * Default and only level for SQLite
     */
    TRANSACTION_SERIALIZABLE,

    /**
     * SQLite-specific. Starts a transaction immediately, acquiring a RESERVED lock.
     * Not applicable to other database systems.
     */
    TRANSACTION_IMMEDIATE,

    /**
     * SQLite-specific. Starts a transaction and acquires an EXCLUSIVE lock immediately.
     * Not applicable to other database systems.
     */
    TRANSACTION_EXCLUSIVE
} TRANSACTION_TYPE;


//<< Protected methods

/**
 * @brief Create a new Connection.
 * @param pool The parent connection pool
 * @param error Connection error or NULL if no error was found
 * @return A new Connection object or NULL on error
 */
T Connection_new(void *pool, char **error) __attribute__ ((visibility("hidden")));


/**
 * @brief Destroy a Connection and release allocated resources.
 * @param C A Connection object reference
 */
void Connection_free(T *C) __attribute__ ((visibility("hidden")));


/**
 * @brief Set if Connection is available
 * @param C A Connection object
 * @param isAvailable true if Connection is available, false otherwise
 */
void Connection_setAvailable(T C, bool isAvailable) __attribute__ ((visibility("hidden")));


/**
 * @brief Gets the availability of this Connection.
 * @param C A Connection object
 * @return true if this Connection is available otherwise false
 */
bool Connection_isAvailable(T C) __attribute__ ((visibility("hidden")));


/**
 * @brief Gets the last accessed time.
 *
 * The time is returned as the number of seconds since midnight, January 1,
 * 1970 GMT. Actions that your application takes, such as calling the public
 * methods of this class do not affect this time.
 * 
 * @param C A Connection object
 * @return The last time (seconds) this Connection was accessed
 */
time_t Connection_getLastAccessedTime(T C) __attribute__ ((visibility("hidden")));


//>> End Protected methods

/// @name Properties
/// @{

/**
 * @brief Sets the query timeout for this Connection.
 *
 * If the limit is exceeded, the statement will return immediately with an 
 * error. The timeout is set per connection/session. The default is no query timeout.
 *
 * @param C A Connection object
 * @param ms The query timeout in milliseconds; zero (the default) means there
 * is no timeout limit.
 */
void Connection_setQueryTimeout(T C, int ms);


/**
 * @brief Gets the query timeout for this Connection.
 * @param C A Connection object
 * @return The query timeout limit in milliseconds; zero means there
 * is no timeout limit
 */
int Connection_getQueryTimeout(T C);


/**
 * @brief Sets the maximum number of rows for ResultSet objects.
 *
 * If the limit is exceeded, the excess rows are silently dropped.
 *
 * @param C A Connection object
 * @param max The new max rows limit; 0 (the default) means there is no limit
 */
void Connection_setMaxRows(T C, int max);


/**
 * @brief Gets the maximum number of rows for ResultSet objects.
 * @param C A Connection object
 * @return The max rows limit; 0 means there is no limit
 */
int Connection_getMaxRows(T C);


/**
 * @brief Sets the number of rows to fetch for ResultSet objects.
 *
 * The default value is 100, meaning that a ResultSet will prefetch rows in
 * batches of 100 rows to reduce the network roundtrip to the database. This
 * value can also be set via the URL parameter `fetch-size` to apply to all
 * connections. This method and the concept of pre-fetching rows are only
 * applicable to MySQL and Oracle.
 *
 * @param C A Connection object
 * @param rows The number of rows to fetch (1..INT_MAX)
 * @exception AssertException If `rows` is less than 1
 */
void Connection_setFetchSize(T C, int rows);


/**
 * @brief Gets the number of rows to fetch for ResultSet objects.
 * @param C A Connection object
 * @return The number of rows to fetch
 */
int Connection_getFetchSize(T C);


/**
 * @brief Gets this Connections URL
 * @param C A Connection object
 * @return This Connections URL
 * @see URL.h
 */
URL_T Connection_getURL(T C);

/// @}
/// @name Functions
/// @{

/**
 * @brief Pings the database server to check if the connection is alive.
 * @param C A Connection object
 * @return true if the connection is alive, false otherwise.
 */
bool Connection_ping(T C);


/**
 * @brief Clears any ResultSet and PreparedStatements in the Connection.
 *
 * Normally it is not necessary to call this method, but for some
 * implementations (SQLite) it *may, in some situations,* be
 * necessary to call this method if an execution sequence error occurs.
 *
 * @param C A Connection object
 */
void Connection_clear(T C);


/**
 * @brief Returns the connection to the connection pool.
 *
 * The same as calling ConnectionPool_returnConnection() on a connection.
 * If the connection is in an uncommitted transaction, rollback is called.
 * It is an unchecked error to attempt to use the Connection after this
 * method is called
 *
 * @param C A Connection object
 */
void Connection_close(T C);


/**
 * @brief Begins a new (default) transaction.
 * @param C A Connection object
 * @exception SQLException If a database error occurs
 * @see SQLException.h
 * @note All transactions must be ended with either Connection_commit()
 *       or Connection_rollback(). Nested transactions are not supported.
 */
void Connection_beginTransaction(T C);


/**
 * @brief Begins a new specific transaction.
 *
 * This method is similar to Connection_beginTransaction() except 
 * it allows you to specify the new transaction's isolation level
 * explicitly. Connection_beginTransaction() uses the default isolation
 * level for the database.
 *
 * @param C A Connection object
 * @param type The transaction type to start
 *             @see TRANSACTION_TYPE enum for available options.
 * @exception SQLException If a database error occurs
 * @see SQLException.h
 * @note All transactions must be ended with either Connection_commit()
 *       or Connection_rollback(). Nested transactions are not supported.
 */
void Connection_beginTransactionType(T C, TRANSACTION_TYPE type);


/**
 * @brief Checks if this Connection is in an uncommitted transaction.
 * @param C A Connection object
 * @return true if in a transaction, false otherwise.
 */
bool Connection_inTransaction(T C);


/**
 * @brief Commits the current transaction.
 *
 * Makes all changes made since the previous commit/rollback permanent
 * and releases any database locks currently held by this Connection
 * object.
 *
 * @param C A Connection object
 * @exception SQLException If a database error occurs
 * @see SQLException.h
 */
void Connection_commit(T C);


/**
 * @brief Rolls back the current transaction.
 *
 * Undoes all changes made in the current transaction and releases any
 * database locks currently held by this Connection object. This method
 * will first call Connection_clear() before performing the rollback to
 * clear any statements in progress such as selects.
 *
 * @param C A Connection object
 * @exception SQLException If a database error occurs
 * @see SQLException.h
 */
void Connection_rollback(T C);


/**
 * @brief Gets the last inserted row ID for auto-increment columns.
 * @param C A Connection object
 * @return The value of the rowid from the last insert statement
 *
 * @note This function works as expected for MySQL and SQLite.
 * For PostgreSQL and Oracle, this function may not return the expected value.
 * For these databases, it's recommended to use the RETURNING clause in your 
 * INSERT statement instead.
 *
 * Example usage for different database systems:
 *
 * @code
 * // For MySQL and SQLite:
 * Connection_execute(con, "INSERT INTO users(name) VALUES(%s)", "Min-seo");
 * long long id = Connection_lastRowId(con);
 *
 * // For PostgreSQL (assuming id is the auto increment column name):
 * ResultSet_T r = Connection_executeQuery(con, "INSERT INTO users(name) VALUES(%s) RETURNING id", "Min-seo");
 * long long id = ResultSet_next(r) ? ResultSet_getLLong(r, 1) : -1;
 *
 * // For Oracle:
 * ResultSet_T r = Connection_executeQuery(con, "INSERT INTO users(name) VALUES(%s) RETURNING ROWID", "Min-seo");
 * long long id = ResultSet_next(r) ? ResultSet_getLLong(r, 1) : -1;
 * @endcode
 *
 * Using PreparedStatement:
 *
 * @code
 * // For MySQL and SQLite:
 * PreparedStatement_T p = Connection_prepareStatement(con, "INSERT INTO users(name) VALUES(?)");
 * PreparedStatement_setString(p, 1, "Ji-eun");
 * PreparedStatement_execute(p);
 * long long id = Connection_lastRowId(con);
 *
 * // For PostgreSQL:
 * PreparedStatement_T p = Connection_prepareStatement(con, "INSERT INTO users(name) VALUES(?) RETURNING id");
 *
 * // For Oracle:
 * PreparedStatement_T p = Connection_prepareStatement(con, "INSERT INTO users(name) VALUES(?) RETURNING ROWID");
 *
 * // for both
 * PreparedStatement_setString(p, 1, "Ji-eun");
 * ResultSet_T r = PreparedStatement_executeQuery(p);
 * long long id = ResultSet_next(r) ? ResultSet_getLLong(r, 1) : -1;
 * @endcode
 *
 * @see ResultSet.h
 */
long long Connection_lastRowId(T C);


/**
 * @brief Gets the number of rows affected by the last execute() statement.
 *
 * If used with a transaction, this method should be called *before* commit is
 * executed, otherwise 0 is returned.
 *
 * @param C A Connection object
 * @return The number of rows changed by the last (DIM) SQL statement
 */
long long Connection_rowsChanged(T C);


/**
 * @brief Executes a SQL statement, with or without parameters.
 *
 * Executes the given SQL statement, which may be an INSERT, UPDATE,
 * or DELETE statement or an SQL statement that returns nothing, such
 * as an SQL DDL statement. Several SQL statements can be used in the
 * sql parameter string, each separated with the `;` SQL
 * statement separator character. **Note**, calling this method
 * clears any previous ResultSets associated with the Connection.
 *
 * @param C A Connection object
 * @param sql A SQL statement
 * @exception SQLException If a database error occurs.
 * @see SQLException.h
 */
void Connection_execute(T C, const char *sql, ...) __attribute__((format (printf, 2, 3)));


/**
 * @brief Executes a SQL query and returns a ResultSet.
 *
 * You may **only** use one SQL statement with this method.
 * This is different from the behavior of Connection_execute() which
 * executes all SQL statements in its input string. If the sql
 * parameter string contains more than one SQL statement, only the
 * first statement is executed, the others are silently ignored.
 * A ResultSet a valid until the next call to Connection_executeQuery(),
 * Connection_execute() or until the Connection is returned to the Connection
 * Pool. *This means that Result Sets cannot be saved between queries*.
 *
 * @param C A Connection object
 * @param sql A SQL statement
 * @return A ResultSet object that contains the data produced by the
 * given query.
 * @exception SQLException If a database error occurs.
 * @see ResultSet.h
 * @see SQLException.h
 */
ResultSet_T Connection_executeQuery(T C, const char *sql, ...) __attribute__((format (printf, 2, 3)));


/**
 * @brief Prepares a SQL statement for execution.
 *
 * The `sql` parameter may contain IN parameter placeholders. An IN
 * placeholder is specified with a '?' character in the sql string.
 * The placeholders are then replaced with actual values by using the
 * PreparedStatement's setXXX methods. Only *one* SQL statement may be
 * used in the sql parameter, this in difference to Connection_execute()
 * which may take several statements. A PreparedStatement is valid until the
 * Connection is returned to the Connection Pool.
 *
 * @param C A Connection object
 * @param sql A single SQL statement that may contain one or more '?'
 * IN parameter placeholders
 * @return A new PreparedStatement object containing the pre-compiled
 * SQL statement.
 * @exception SQLException If a database error occurs.
 * @see PreparedStatement.h
 * @see SQLException.h
 */
PreparedStatement_T Connection_prepareStatement(T C, const char *sql, ...) __attribute__((format (printf, 2, 3)));


/**
 * @brief Gets the last SQL error message.
 *
 * This method can be used to obtain a string describing the last
 * error that occurred. Inside a CATCH-block you can also find
 * the error message directly in the variable Exception_frame.message.
 * It is recommended to use this variable instead since it contains both
 * SQL errors and API errors such as parameter index out of range etc,
 * while Connection_getLastError() might only show SQL errors
 *
 * @param C A Connection object
 * @return A string explaining the last error
 */
const char *Connection_getLastError(T C);

/// @}
/// @name Class functions
/// @{

/**
 * @brief Checks if the specified database system is supported.
 *
 * Clients may pass a full Connection URL, for example using
 * URL_toString(), or for convenience only the protocol
 * part of the URL. E.g. "mysql" or "sqlite".
 *
 * @param url A database url string or database name
 * @return true if supported, false otherwise.
 */
bool Connection_isSupported(const char *url);

/// @}

#undef T
#endif
