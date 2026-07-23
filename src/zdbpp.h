/*
 * Copyright (C) 2016 dragon jiang<jianlinlong@gmail.com>
 * Copyright (C) 2019-2025 Tildeslash Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _ZDBPP_H_
#define _ZDBPP_H_

#include "zdb.h"
#include <string>
#include <utility>
#include <stdexcept>
#include <ctime>
#include <chrono>
#include <type_traits>
#include <limits>
#include <optional>
#include <span>
#include <concepts>
#include <ranges>
#include <functional>
#include <vector>

/**
 * @brief zdbpp.h - C++ Interface for libzdb
 *
 * <center><img src="database++.png" class="resp-img" style="width:700px"></center>
 *
 * A modern, type-safe way to interact with various SQL databases from C++ applications.
 *
 * ## Features
 *
 * - Thread-safe Database Connection Pool
 * - Connect to multiple database systems simultaneously
 * - Zero runtime configuration, connect using a URL scheme
 * - Supports MySQL, PostgreSQL, SQLite, and Oracle
 * - Modern C++ features (C++20 or later required)
 *
 * ## Core Concepts
 *
 * The central class in this library is `ConnectionPool`, which manages database
 * connections. All other main classes (`Connection`, `PreparedStatement`, and
 * `ResultSet`) are obtained through the `ConnectionPool` or its derivatives.
 *
 * ### ConnectionPool and URL
 *
 * The `ConnectionPool` is initialized with a `URL` object, which specifies the
 * database connection details:
 *
 * ```cpp
 * zdb::URL url("mysql://localhost:3306/mydb?user=root&password=secret");
 * zdb::ConnectionPool pool(url);
 * pool.start();
 * ```
 *
 * A ConnectionPool is designed to be a long-lived object that manages database
 * connections throughout the lifetime of your application. Typically, you would
 * instantiate one or more ConnectionPool objects as part of a resource management
 * class or in the global scope of your application.
 *
 * ### Best Practices for Using ConnectionPool
 *
 * 1. Create ConnectionPool instances at application startup.
 * 2. Maintain these instances for the entire duration of your application's runtime.
 * 3. Use a single ConnectionPool for each distinct database you need to connect to.
 * 4. Consider wrapping ConnectionPool instances in a singleton or dependency
 *    injection pattern for easy access across your application.
 * 5. Ensure proper shutdown of ConnectionPool instances when your application
 *    terminates to release all database resources cleanly.
 *
 * Example of a global ConnectionPool manager:
 *
 * ```cpp
 * class DatabaseManager {
 * public:
 *     static ConnectionPool& getMainPool() {
 *         static ConnectionPool mainPool("mysql://localhost/maindb?user=root&password=pass");
 *         return mainPool;
 *     }
 *
 *     static ConnectionPool& getAnalyticsPool() {
 *         static ConnectionPool analyticsPool("postgresql://analyst:pass@192.168.8.217/datawarehouse");
 *         return analyticsPool;
 *     }
 *
 *     static void initialize() {
 *         static std::once_flag initFlag;
 *         std::call_once(initFlag, []() {
 *             // Configure and start main pool
 *             ConnectionPool& main = getMainPool();
 *             main.setInitialConnections(5);  // Example value
 *             main.setMaxConnections(20);     // Example value
 *             main.setConnectionTimeout(30);  // 30 seconds timeout
 *             main.start();
 *
 *             // Configure and start analytics pool
 *             ConnectionPool& analytics = getAnalyticsPool();
 *             analytics.setInitialConnections(2);
 *             analytics.setMaxConnections(10);
 *             analytics.start();
 *         });
 *     }
 *
 *     static void shutdown() {
 *         getMainPool().stop();
 *         getAnalyticsPool().stop();
 *     }
 * };
 * ```
 *
 * ## Usage Examples
 *
 * ### Basic Query Execution
 *
 * ```cpp
 * auto& pool = DatabaseManager::getMainPool();
 * auto con = pool.getConnection();
 *
 * ResultSet result = con.executeQuery("SELECT name, age FROM users WHERE id = ?", 1);
 * if (result.next()) {
 *     std::cout << "Name: " << result.getString("name").value_or("N/A")
 *               << ", Age: " << result.getInt("age") << std::endl;
 * }
 * ```
 *
 * ### Using PreparedStatement
 *
 * ```cpp
 * auto& pool = DatabaseManager::getAnalyticsPool();
 * auto con = pool.getConnection();
 *
 * auto stmt = con.prepareStatement("INSERT INTO logs (message, timestamp) VALUES (?, ?)");
 * stmt.bindValues("User logged in", std::time(nullptr));
 * stmt.execute();
 * ```
 *
 * ### Transaction Example
 *
 * ```cpp
 * Connection con = pool.getConnection();
 *
 * // Use default isolation level
 * con.beginTransaction();
 * con.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?", 100.0, 1);
 * con.commit();
 *
 * // Alternatively, specify the transaction's isolation level
 * con.beginTransaction(TRANSACTION_SERIALIZABLE));
 * con.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?", 100.0, 2);
 * con.commit();
 * ```
 *
 * ## Exception Handling
 *
 * All database-related errors are thrown as `sql_exception`, which derives from
 * `std::runtime_error`.
 *
 * ### Example of Exception Handling
 *
 * ```cpp
 * try {
 *     Connection con = pool.getConnection();
 *     con.beginTransaction();
 *     con.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?", 100.0, 1);
 *     con.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?", 100.0, 2);
 *     con.commit();
 *     std::cout << "Transfer successful" << std::endl;
 *     // Connection is automatically returned to pool when it goes out of scope
 *     // If an exception occurred before commit, it will automatically rollback
 * } catch (const sql_exception& e) {
 *     std::cerr << "Transfer failed (" << e.error_code << "): << e.what() << std::endl;
 * }
 * ```
 * Key points about exception handling in this library:
 *
 * 1. All database operations that can fail will throw `sql_exception`.
 * 2. `sql_exception` provides informative error messages through its `what()` method.
 * 3. You should wrap database operations in try-catch blocks to handle potential errors gracefully.
 * 4. The library ensures that resources are properly managed even when exceptions are thrown, preventing resource leaks.
 *
 *
 * @note For detailed API documentation, refer to the comments for each class in this header file.
 * Visit [libzdb's homepage](https://www.tildeslash.com/libzdb/) for additional documentation and examples.
 * @file
 */
namespace zdb {
    namespace version {
        constexpr int major = LIBZDB_MAJOR;
        constexpr int minor = LIBZDB_MINOR;
        constexpr int revision = LIBZDB_REVISION;
        
        constexpr int number = LIBZDB_VERSION_NUMBER;
        constexpr std::string_view string = LIBZDB_VERSION;
        
        constexpr bool is_compatible_with(int required_major, int required_minor, int required_revision = 0) {
            return (major > required_major) ||
            (major == required_major && minor > required_minor) ||
            (major == required_major && minor == required_minor && revision >= required_revision);
        }
    } // version
    
    namespace { // private
        // @cond hide
        #define except_wrapper(f) TRY { f; } ELSE { throw sql_exception(Exception_frame.message, Exception_frame.errorCode); } END_TRY
        
        constexpr std::optional<std::string_view> _to_optional(const char* str) noexcept {
            return str ? std::optional<std::string_view>{str} : std::nullopt;
        }
        
        std::function<void(std::string_view)> g_abortHandler;
        
        void bridgeAbortHandler(const char* error) {
            if (g_abortHandler) {
                g_abortHandler(error);
            }
        }
        
        struct noncopyable {
            noncopyable() = default;
            noncopyable(const noncopyable&) = delete;
            noncopyable& operator=(const noncopyable&) = delete;
            noncopyable(noncopyable&&) = delete;
            noncopyable& operator=(noncopyable&&) = delete;
        };
        
        template<typename T>
        inline constexpr bool always_false = false;
        
        template<typename T>
        concept Stringable = std::convertible_to<T, std::string_view>;
        
        template<typename T>
        concept Numeric = std::integral<T> || std::floating_point<T>;

        template<typename T>
        concept Blobable = std::ranges::contiguous_range<T> && std::same_as<std::ranges::range_value_t<T>, std::byte>;

        template<typename T>
        concept TimePoint = std::same_as<T, std::chrono::system_clock::time_point>;
        // @endcond
    } // anonymous namespace
    
    /**
     * @brief Exception class for SQL related errors.
     *
     * Thrown for API and SQL errors. Inherits from `std::runtime_error`.
     *
     * The `error_code` member provides the native error code from the underlying
     * database, enabling programmatic error handling such as retry logic for
     * deadlocks or special handling for constraint violations. Error codes are
     * database-specific:
     *
     * - **PostgreSQL**: SQLSTATE codes encoded as integers (use constants from SQLState.h)
     * - **MySQL/MariaDB**: Native error codes from `mysql_errno()` (include `<mysqld_error.h>`)
     * - **SQLite**: Extended error codes (include `<sqlite3.h>`)
     * - **Oracle**: ORA- error numbers (numeric portion only)
     *
     * Example (PostgreSQL):
     * @code
     * try {
     *     con.execute(sql);
     * } catch (const zdb::sql_exception& e) {
     *     if (e.error_code == SQLSTATE_unique_violation) {
     *         // Handle duplicate key
     *     } else if (e.error_code == SQLSTATE_foreign_key_violation) {
     *         // Handle FK constraint failure
     *     } else if (e.error_code == SQLSTATE_deadlock_detected) {
     *         // Retry transaction
     *     } else {
     *         std::cerr << "Error (" << e.error_code << "): " << e.what() << std::endl;
     *     }
     * }
     * @endcode
     *
     * For MySQL, SQLite, and Oracle, include the respective database header
     * with error codes or use numeric codes directly.
     */
    class sql_exception : public std::runtime_error {
    public:
        /**
        * @brief Native error code from the underlying database.
        *
        * The value is `0` if no specific error code was provided. Consult your
        * database's documentation for code meanings.
        */
        const int error_code;
        
        /**
        * @brief Constructs a new sql_exception.
        * @param msg Error message (defaults to "SQLException")
        * @param code Database-specific error code (defaults to 0)
        */
        explicit sql_exception(const char* msg = "SQLException", int code = 0)
        : std::runtime_error(msg), error_code(code) {}
    };

    /**
     * @class URL
     * @brief Represents an immutable Uniform Resource Locator.
     *
     * A Uniform Resource Locator (URL), is used to uniquely identify a
     * resource on the Internet. The URL is a compact text string with a
     * restricted syntax that consists of four main components:
     *
     * ```
     * protocol://<authority><path><query>
     * ```
     *
     * The `protocol` part is mandatory, the other components may or may not
     * be present in an URL string. For instance the `file` protocol only use
     * the path component while a `http` protocol may use all components.
     *
     * The following URL components are automatically unescaped according to the escaping
     * mechanism defined in RFC 2396; `credentials`, `path` and parameter
     * `values`. If you use a password with non-URL safe characters, you must URL
     * escape the value.
     *
     * An <em>IPv6 address</em> can be used for host as defined in
     * <a href="https://www.ietf.org/rfc/rfc2732.txt">RFC2732</a> by enclosing the
     * address in [brackets]. For instance,
     * `mysql://[2010:836B:4179::836B:4179]:3306/test`
     *
     * For more information about the URL syntax and specification, see,
     * <a href="https://www.ietf.org/rfc/rfc2396.txt">RFC2396 -
     * Uniform Resource Identifiers (URI): Generic Syntax</a>
     *
     * ### Example:
     *
     * @code
     * URL url("postgresql://user:password@example.com:5432/database?use-ssl=true");
     *
     * // Retrieve and print various components of the URL
     * std::cout << "Protocol: " << url.protocol() << std::endl;
     * std::cout << "Host: " << url.host().value_or("Not specified") << std::endl;
     * std::cout << "Port: " << url.port() << std::endl;
     * std::cout << "User: " << url.user().value_or("Not specified") << std::endl;
     * std::cout << "Password: " << (url.password().has_value() ? "Specified" : "Not specified") << std::endl;
     * std::cout << "Path: " << url.path().value_or("Not specified") << std::endl;
     *
     * // Get a specific parameter value
     * auto use_ssl = url.parameter("use-ssl").value_or("false");
     * std::cout << "SSL Enabled: " << (use_ssl == "true") << std::endl;
     * @endcode
     */
    class URL {
    public:
        /**
         * @brief Creates a new URL object from the given URL string.
         * @param url A string specifying the URL.
         * @throws sql_exception if the URL cannot be parsed.
         */
        explicit URL(const std::string& url) : t_(URL_new(url.c_str())) {
            if (!t_) throw sql_exception("Invalid URL");
        }
        
        /**
         * @brief Copy constructor.
         * @param r URL object to copy.
         * @private
         */
        URL(const URL& r) : t_(URL_new(URL_toString(r.t_))) {
            if (!t_) throw sql_exception("Failed to copy URL");
        }
        
        /**
         * @brief Move constructor.
         * @param r URL object to move.
         * @private
         */
        URL(URL&& r) noexcept : t_(r.t_) {
            r.t_ = nullptr;
        }
        
        /**
         * @brief Copy assignment operator.
         * @param r URL object to copy assign.
         * @private
         */
        URL& operator=(const URL& r) {
            if (this != &r) {
                URL tmp(r);
                std::swap(t_, tmp.t_);
            }
            return *this;
        }
        
        /**
         * @brief Move assignment operator.
         * @param r URL object to move assign.
         * @private
         */
        URL& operator=(URL&& r) noexcept {
            if (this != &r) {
                if (t_) URL_free(&t_);
                t_ = r.t_;
                r.t_ = nullptr;
            }
            return *this;
        }
        
        /**
         * @brief Destroy the URL object.
         * @private
         */
        ~URL() { if (t_) URL_free(&t_); }
        
        /// @name Properties
        /// @{

        /**
         * @brief Gets the protocol of the URL.
         * @return The protocol name.
         */
        [[nodiscard]] constexpr std::string_view protocol() const noexcept { return URL_getProtocol(t_); }
        
        /**
         * @brief Gets the username from the URL's authority part.
         * @return An optional containing the username or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> user() const noexcept { return _to_optional(URL_getUser(t_)); }
        
        /**
         * @brief Gets the password from the URL's authority part.
         * @return An optional containing the password or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> password() const noexcept { return _to_optional(URL_getPassword(t_)); }
        
        /**
         * @brief Gets the hostname of the URL.
         * @return An optional containing the hostname or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> host() const noexcept { return _to_optional(URL_getHost(t_)); }
        
        /**
         * @brief Gets the port of the URL.
         * @return The port number of the URL or -1 if not specified.
         */
        [[nodiscard]] constexpr int port() const noexcept { return URL_getPort(t_); }
        
        /**
         * @brief Gets the path of the URL.
         * @return An optional containing the path or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> path() const noexcept { return _to_optional(URL_getPath(t_)); }
        
        /**
         * @brief Gets the query string of the URL.
         * @return An optional containing the query string or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> queryString() const noexcept { return _to_optional(URL_getQueryString(t_)); }
        
        /**
         * @brief Gets the names of parameters contained in this URL.
         * @return A vector of parameter names, or an empty vector if no parameters.
         */
        [[nodiscard]] std::vector<std::string_view> parameterNames() const noexcept {
            std::vector<std::string_view> names;
            const char **rawNames = URL_getParameterNames(t_);
            if (rawNames) {
                for (int i = 0; rawNames[i] != nullptr; i++) {
                    names.emplace_back(rawNames[i]);
                }
            }
            return names;
        }
        
        /**
         * @brief Gets the value of the specified URL parameter.
         * @param name The parameter name to lookup.
         * @return An optional containing the parameter value, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<std::string_view> parameter(const std::string& name) const noexcept {
            return _to_optional(URL_getParameter(t_, name.c_str()));
        }
        
        /// @}
        /// @name Functions
        /// @{

        /**
         * @brief Returns a string representation of this URL object.
         * @return The URL string.
         */
        [[nodiscard]] constexpr std::string_view toString() const noexcept { return URL_toString(t_); }
        
        /// @}

        /**
         * @brief Cast operator to URL_T.
         * @return The internal URL_T representation.
         * @private
         */
        operator URL_T() const noexcept { return t_; }
        
    private:
        URL_T t_;
    };

    
    /**
     * @class ResultSet
     * @brief Represents a database result set.
     *
     * A ResultSet is created by executing a SQL SELECT statement using
     * Connection::executeQuery().
     *
     * A ResultSet maintains a cursor pointing to its current row of data.
     * Initially, the cursor is positioned before the first row.
     * ResultSet::next() moves the cursor to the next row, and because
     * it returns false when there are no more rows, it can be used in a while
     * loop to iterate through the result set. A ResultSet is not updatable and
     * has a cursor that moves forward only. Thus, you can iterate through it
     * only once and only from the first row to the last row.
     *
     * The ResultSet class provides getter methods for retrieving
     * column values from the current row. Values can be retrieved using
     * either the index number of the column or the name of the column. In
     * general, using the column index will be more efficient. _Columns are
     * numbered from 1._
     *
     * Column names used as input to getter methods are case sensitive.
     * When a getter method is called with a column name and several
     * columns have the same name, the value of the first matching column
     * will be returned. The column name option is designed to be used
     * when column names are used in the SQL query that generated the
     * result set. For columns that are NOT explicitly named in the query,
     * it is best to use column indices.
     *
     * ## Examples
     *
     * The following examples demonstrate how to obtain a ResultSet and
     * how to retrieve values from it.
     *
     * ### Example: Using column names
     *
     * In this example, columns are named in the SELECT statement, and we retrieve
     * values using the column names (we could of course also use indices if we
     * want):
     *
     * @code
     * Connection con = pool.getConnection();
     * ResultSet result = con.executeQuery("SELECT ssn, name, photo FROM employees");
     * while (result.next()) {
     *     int ssn = result.getInt("ssn");
     *     auto name = result.getString("name");
     *     auto photo = result.getBlob("photo");
     *     if (photo) {
     *         // Process photo data
     *     }
     *     // Process other data...
     * }
     * @endcode
     *
     * ### Example: Using column indices
     *
     * This example demonstrates selecting a generated result and printing it.
     * When the SELECT statement doesn't name the column, we use the column
     * index to retrieve the value:
     *
     * @code
     * Connection con = pool.getConnection();
     * ResultSet r = con.executeQuery("SELECT COUNT(*) FROM employees");
     * if (r.next()) {
     *     std::cout << "Number of employees: "
     *               << r.getString(1).value_or("none")
     *               << std::endl;
     * } else {
     *     std::cout << "No results returned" << std::endl;
     * }
     * @endcode
     *
     * ## Automatic type conversions
     *
     * A ResultSet stores values internally as bytes and converts values
     * on-the-fly to numeric types when requested, such as when getInt()
     * or one of the other numeric get-methods are called. In the above example,
     * even if *count(\*)* returns a numeric value, we can use getString()
     * to get the number as a string or if we choose, we can use getInt()
     * to get the value as an integer. In the latter case, note that if the column
     * value cannot be converted to a number, an sql_exception is thrown.
     *
     * ## Date and Time
     *
     * ResultSet provides two principal methods for retrieving temporal column
     * values as C types. getTimestamp() converts a SQL timestamp value
     * to a `time_t` and getDateTime() returns a `tm structure` representing
     * a Date, Time, DateTime, or Timestamp column type. To get a temporal column
     * value as a string, simply use getString()
     *
     * *A ResultSet is reentrant, but not thread-safe and should only be used by
     * one thread (at a time).*
     *
     * @note Remember that column indices in ResultSet are 1-based, not 0-based.
     *
     * @warning ResultSet objects are internally managed by the Connection that
     * created them and are not copyable or movable. Always ensure that the originating
     * Connection object remains valid for the entire duration of the ResultSet's
     * use. Basically, keep the Connection and ResultSet objects in the same scope.
     * Do not attempt to use ResultSet objects (including through references or
     * pointers) after their Connection has been closed and returned to the pool.
     */
    class ResultSet : private noncopyable {
    public:
        /**
         * @brief Move constructor.
         * @param r ResultSet object to move.
         * @private
         */
        ResultSet(ResultSet&& r) noexcept : t_(r.t_) { r.t_ = nullptr; }
        
        /**
         * @brief Conversion operator to ResultSet_T.
         * @return The underlying ResultSet_T.
         * @private
         */
        operator ResultSet_T() noexcept { return t_; }
        
        /// @name Properties
        /// @{

        /**
         * @brief Gets the number of columns in this ResultSet.
         * @return The number of columns.
         */
        [[nodiscard]] int columnCount() const noexcept { return ResultSet_getColumnCount(t_); }
        
        /**
         * @brief Gets the designated column's name.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return An optional containing the Column name, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<std::string_view> columnName(int columnIndex) const noexcept {
            return _to_optional(ResultSet_getColumnName(t_, columnIndex));
        }
        
        /**
         * @brief Gets the size of a column in bytes.
         *
         * If the column is a blob then this method returns the number of bytes
         * in that blob. No type conversions occur. If the result is a string
         * (or a number since a number can be converted into a string) then return
         * the number of bytes in the resulting string.
         *
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return Column data size.
         * @throws sql_exception If columnIndex is outside the valid range.
         */
        [[nodiscard]] long columnSize(int columnIndex) {
            except_wrapper(RETURN ResultSet_getColumnSize(t_, columnIndex));
        }
        
        /**
         * @brief Sets the number of rows to fetch from the database.
         *
         * ResultSet will prefetch rows in batches of number of `rows` when next()
         * is called to reduce the network roundtrip to the database. This method
         * is only applicable to MySQL and Oracle.
         *
         * @param rows The number of rows to fetch (1..INT_MAX).
         */
        void setFetchSize(int rows) noexcept { ResultSet_setFetchSize(t_, rows); }
        
        /**
         * @brief Gets the number of rows to fetch from the database.
         *
         * Unless previously set with setFetchSize(), the returned value
         * is the same as returned by Connection::getFetchSize()
         
         * @return The number of rows to fetch or 0 if N/A.
         */
        [[nodiscard]] int getFetchSize() const noexcept { return ResultSet_getFetchSize(t_); }
        
        /// @}
        /// @name Functions
        /// @{

        /**
         * @brief Moves the cursor to the next row.
         *
         * A ResultSet cursor is initially positioned before the first row; the
         * first call to this method makes the first row the current row; the
         * second call makes the second row the current row, and so on. When
         * there are no more available rows false is returned. An empty
         * ResultSet will return false on the first call to ResultSet::next().
         *
         * @return true if the new current row is valid; false if there are no more rows.
         * @throws sql_exception If a database access error occurs.
         */
        bool next() { except_wrapper(RETURN ResultSet_next(t_)); }
        
        /// @}
        /// @name Columns
        /// @{

        /**
         * @brief Checks if the designated column's value is SQL NULL.
         *
         * A ResultSet returns an optional for reference types and 0 for value types.
         * Use this method if you need to differentiate between SQL NULL and std::nullopt/0.
         *
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return true if column value is SQL NULL, false otherwise.
         * @throws sql_exception If a database access error occurs or columnIndex is invalid.
         */
        [[nodiscard]] bool isNull(int columnIndex) {
            except_wrapper(RETURN ResultSet_isnull(t_, columnIndex));
        }
        
        /**
         * @brief Checks if the designated column's value is SQL NULL.
         *
         * A ResultSet returns an optional for reference types and 0 for value types.
         * Use this method if you need to differentiate between SQL NULL and std::nullopt/0.
         *
         * @param columnName The SQL name of the column. case-sensitive.
         * @return true if column value is SQL NULL, false otherwise.
         * @throws sql_exception If a database access error occurs or columnName does not exist.
         */
        [[nodiscard]] bool isNull(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_isnullByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Gets the designated column's value as a string.
         *
         * _The returned string may only be valid until the next call to next()
         * and if you plan to use the returned value longer, you must make a copy._
         *
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return An optional containing the column value, or std::nullopt if NULL.
         * @throws sql_exception If a database access error occurs or columnIndex is invalid.
         */
        [[nodiscard]] std::optional<std::string_view> getString(int columnIndex) {
            except_wrapper(RETURN _to_optional(ResultSet_getString(t_, columnIndex)));
        }
        
        /**
         * @brief Gets the designated column's value as a string.
         *
         * _The returned string may only be valid until the next call to next()
         * and if you plan to use the returned value longer, you must make a copy._
         *
         * @param columnName The SQL name of the column. case-sensitive.
         * @return An optional containing the column value, or std::nullopt if NULL.
         * @throws sql_exception If a database access error occurs or columnName does not exist.
         */
        [[nodiscard]] std::optional<std::string_view> getString(const std::string& columnName) {
            except_wrapper(RETURN _to_optional(ResultSet_getStringByName(t_, columnName.c_str())));
        }
        
        /**
         * @brief Gets the designated column's value as an int.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database error occurs, columnIndex is invalid or value is NaN.
         */
        [[nodiscard]] int getInt(int columnIndex) {
            except_wrapper(RETURN ResultSet_getInt(t_, columnIndex));
        }
        
        /**
         * @brief Gets the designated column's value as an int.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database error occurs, columnName is invalid or value is NaN.
         */
        [[nodiscard]] int getInt(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getIntByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Gets the designated column's value as a long long.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database error occurs, columnIndex is invalid or value is NaN.
         */
        [[nodiscard]] long long getLLong(int columnIndex) {
            except_wrapper(RETURN ResultSet_getLLong(t_, columnIndex));
        }
        
        /**
         * @brief Gets the designated column's value as a long long.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database error occurs, columnName is invalid or value is NaN.
         */
        [[nodiscard]] long long getLLong(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getLLongByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Gets the designated column's value as a double.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value; if the value is SQL NULL, the value returned is 0.0.
         * @throws sql_exception If a database error occurs, columnIndex is invalid or value is NaN.
         */
        [[nodiscard]] double getDouble(int columnIndex) {
            except_wrapper(RETURN ResultSet_getDouble(t_, columnIndex));
        }
        
        /**
         * @brief Gets the designated column's value as a double.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value; if the value is SQL NULL, the value returned is 0.0.
         * @throws sql_exception If a database error occurs, columnName is invalid or value is NaN.
         */
        [[nodiscard]] double getDouble(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getDoubleByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Gets the designated column's value as a byte span.
         *
         * _The returned blob may only be valid until the next call to next() and
         * if you plan to use the returned value longer, you must make a copy._
         *
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return An optional span of bytes containing the blob data, or std::nullopt if NULL.
         * @throws sql_exception If a database access error occurs or columnIndex is invalid.
         */
        [[nodiscard]] std::optional<std::span<const std::byte>> getBlob(int columnIndex) {
            int size = 0;
            const void *blob = nullptr;
            except_wrapper(blob = ResultSet_getBlob(t_, columnIndex, &size));
            if (blob == nullptr || size == 0) {
                return std::nullopt;
            }
            return std::span<const std::byte>(static_cast<const std::byte*>(blob), size);
        }
        
        /**
         * @brief Gets the designated column's value as a byte span.
         *
         * _The returned blob may only be valid until the next call to next() and
         * if you plan to use the returned value longer, you must make a copy._
         *
         * @param columnName The SQL name of the column. case-sensitive.
         * @return An optional span of bytes containing the blob data, or std::nullopt if NULL.
         * @throws sql_exception If a database access error occurs or columnName is invalid.
         */
        [[nodiscard]] std::optional<std::span<const std::byte>> getBlob(const std::string& columnName) {
            int size = 0;
            const void *blob = nullptr;
            except_wrapper(blob = ResultSet_getBlobByName(t_, columnName.c_str(), &size));
            if (blob == nullptr || size == 0) {
                return std::nullopt;
            }
            return std::span<const std::byte>(static_cast<const std::byte*>(blob), size);
        }

        /// @}
        /// @name Date and Time
        /// @{

        /**
         * @brief Gets the designated column's value as a Unix timestamp.
         *
         * The returned value is in Coordinated Universal Time (UTC) and represents 
         * seconds since the **epoch** (January 1, 1970, 00:00:00 GMT).
         *
         * Even though the underlying database might support timestamp ranges before
         * the epoch and after '2038-01-19 03:14:07 UTC' it is safest not to assume or
         * use values outside this range. Especially on a 32-bit system.
         *
         * *SQLite* does not have temporal SQL data types per se
         * and using this method with SQLite assumes the column value in the Result Set
         * to be either a numerical value representing a Unix Time in UTC which is
         * returned as-is or an [ISO 8601](http://en.wikipedia.org/wiki/ISO_8601)
         * time string which is converted to a `time_t` value.
         *
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value as seconds since the epoch in the GMT timezone.
         *         If the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnIndex is outside
         *         the valid range or if the column value cannot be converted to a valid timestamp.
         */
        [[nodiscard]] time_t getTimestamp(int columnIndex) {
            except_wrapper(RETURN ResultSet_getTimestamp(t_, columnIndex));
        }
        
        /**
         * @brief Gets the designated column's value as a Unix timestamp.
         *
         * The returned value is in Coordinated Universal Time (UTC) and represents
         * seconds since the **epoch** (January 1, 1970, 00:00:00 GMT).
         *
         * Even though the underlying database might support timestamp ranges before
         * the epoch and after '2038-01-19 03:14:07 UTC' it is safest not to assume or
         * use values outside this range. Especially on a 32-bit system.
         *
         * *SQLite* does not have temporal SQL data types per se
         * and using this method with SQLite assumes the column value in the Result Set
         * to be either a numerical value representing a Unix Time in UTC which is
         * returned as-is or an [ISO 8601](http://en.wikipedia.org/wiki/ISO_8601)
         * time string which is converted to a `time_t` value.
         *
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value as seconds since the epoch in the GMT timezone.
         *         If the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnName is not found
         *         or if the column value cannot be converted to a valid timestamp.
         */
        [[nodiscard]] time_t getTimestamp(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getTimestampByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Gets the designated column's value as a Date, Time or DateTime.
         *
         * This method can be used to retrieve the value of columns with the SQL data
         * type, Date, Time, DateTime or Timestamp. The returned `tm` structure follows
         * the convention for usage with mktime(3) where:
         *
         * - tm_hour = hours since midnight [0-23]
         * - tm_min = minutes after the hour [0-59]
         * - tm_sec = seconds after the minute [0-60]
         * - tm_mday = day of the month [1-31]
         * - tm_mon = months since January **[0-11]**
         *
         * If the column value contains timezone information, tm_gmtoff is set to the
         * offset from UTC in seconds, otherwise tm_gmtoff is set to 0. *On systems
         * without tm_gmtoff, (Solaris), the member, tm_wday is set to gmt offset
         * instead as this property is ignored by mktime on input.* The exception to
         * the above is **tm_year** which contains the year literal and *not years
         * since 1900* which is the convention. All other fields in the structure are
         * set to zero. If the column type is DateTime or Timestamp all the fields
         * mentioned above are set, if it is a Date or a Time, only the relevant
         * fields are set.
         *
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return A tm structure with fields for date and time.
         *         If the value is SQL NULL, a zeroed tm structure is returned. Use 
         *         isNull() if in doubt.
         * @throws sql_exception If a database access error occurs, columnIndex is 
         *         outside the valid range or if the column value cannot be converted
         *         to a valid SQL Date, Time or DateTime type.
         */
        [[nodiscard]] tm getDateTime(int columnIndex) {
            except_wrapper(RETURN ResultSet_getDateTime(t_, columnIndex));
        }
        
        /**
         * @brief Gets the designated column's value as a Date, Time or DateTime.
         *
         * This method can be used to retrieve the value of columns with the SQL data
         * type, Date, Time, DateTime or Timestamp. The returned `tm` structure follows
         * the convention for usage with mktime(3) where:
         *
         * - tm_hour = hours since midnight [0-23]
         * - tm_min = minutes after the hour [0-59]
         * - tm_sec = seconds after the minute [0-60]
         * - tm_mday = day of the month [1-31]
         * - tm_mon = months since January **[0-11]**
         *
         * If the column value contains timezone information, tm_gmtoff is set to the
         * offset from UTC in seconds, otherwise tm_gmtoff is set to 0. *On systems
         * without tm_gmtoff, (Solaris), the member, tm_wday is set to gmt offset
         * instead as this property is ignored by mktime on input.* The exception to
         * the above is **tm_year** which contains the year literal and *not years
         * since 1900* which is the convention. All other fields in the structure are
         * set to zero. If the column type is DateTime or Timestamp all the fields
         * mentioned above are set, if it is a Date or a Time, only the relevant
         * fields are set.
         *
         * @param columnName The SQL name of the column. case-sensitive.
         * @return A tm structure with fields for date and time.
         *         If the value is SQL NULL, a zeroed tm structure is returned. 
         *         Use isNull() if in doubt.
         * @throws sql_exception If a database access error occurs, columnName is 
         *         not found or if the column value cannot be converted to a valid
         *         Date, Time or DateTime type.
         */
        [[nodiscard]] tm getDateTime(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getDateTimeByName(t_, columnName.c_str()));
        }

        /// @}

    protected:
        friend class PreparedStatement;
        friend class Connection;
        
        /**
         * @brief Constructs a ResultSet from a ResultSet_T.
         * @param t The underlying ResultSet_T.
         * @private
         */
        explicit ResultSet(ResultSet_T t) : t_(t) {}
        
    private:
        ResultSet_T t_;
    };

    
    /**
     * @class PreparedStatement
     * @brief Represents a pre-compiled SQL statement for later execution.
     *
     * A PreparedStatement is created by calling Connection::prepareStatement(). The SQL
     * statement may contain *in* parameters of the form "?". Such parameters represent
     * unspecified literal values (or "wildcards") to be filled in later by the bind methods
     * defined in this class. Each *in* parameter has an associated index number which is
     * its sequence in the statement. The first *in* '?' parameter has index 1,
     * the next has index 2 and so on.
     *
     * Consider this statement:
     * ```sql
     * INSERT INTO employee(name, photo) VALUES(?, ?)
     * ```
     * There are two *in* parameters in this statement, the parameter for setting
     * the name has index 1 and the one for the photo has index 2. To set the values
     * for the *in* parameters we use bindValues() with two values, one for each *in*
     * parameter. Or we can use bind() to set the parameter values one by one.
     *
     * ## Examples
     *
     * The following examples demonstrate how to create and use a PreparedStatement.
     *
     * ### Example: Binding all values at once
     *
     * This example shows how to prepare a statement, bind multiple values at once, and execute it:
     *
     * ```cpp
     * Connection con = pool.getConnection();
     * PreparedStatement stmt = con.prepareStatement("INSERT INTO employee(name, photo) VALUES(?, ?)");
     * stmt.bindValues("Kamiya Kaoru", jpeg);
     * stmt.execute();
     * ```
     *
     * ### Example: Binding values individually
     *
     * Instead of binding all values at once, we can also bind values one by one by
     * specifying the parameter index we want to set a value for:
     *
     * ```cpp
     * PreparedStatement stmt = con.prepareStatement("INSERT INTO employee(name, photo) VALUES(?, ?)");
     * stmt.bind(1, "Kamiya Kaoru");
     * stmt.bind(2, jpeg);
     * stmt.execute();
     * ```
     *
     * ## Reuse
     *
     * A PreparedStatement can be reused. That is, the method execute() can be called one
     * or more times to execute the same statement. Clients can also set new *in* parameter
     * values and re-execute the statement as shown in this example, where we also show use
     * of a transaction and exception handling:
     *
     * ```cpp
     * PreparedStatement stmt = con.prepareStatement("INSERT INTO employee(name, photo) VALUES(?, ?)");
     * try {
     *     con.beginTransaction();
     *     for (const auto& emp : employees) {
     *         stmt.bind(1, emp.name);
     *         if (emp.photo) {
     *             stmt.bind(2, *emp.photo);
     *         } else {
     *             stmt.bind(2, nullptr); // Set to SQL NULL if no photo
     *         }
     *         stmt.execute();
     *     }
     *     con.commit();
     * } catch (const sql_exception& e) {
     *     con.rollback();
     *     std::cerr << "Database error (" << e.error_code << "): << e.what() << std::endl;
     * }
     * ```
     *
     * ## Date and Time
     *
     * bindValues() or bind() can be used to set a timestamp value by binding a
     * `std::chrono::system_clock::time_point`. (A `time_t` value binds as an ordinary
     * integer, since `time_t` is merely a typedef for a built-in integer type.)
     * To set Date, Time or DateTime values, simply use one of the bind methods to set a
     * time string in a format understood by your database. For instance to set a SQL Date value,
     * ```cpp
     * stmt.bind(parameterIndex, "2024-12-28");
     * // or using bindValues
     * stmt.bindValues("2019-12-28", ...);
     * ```
     *
     * ## Result Sets
     *
     * See Connection::executeQuery()
     *
     * ## SQL Injection Prevention
     *
     * Prepared Statement is particularly useful when dealing with user-submitted data, as
     * properly used Prepared Statements provide strong protection against SQL injection
     * attacks. By separating SQL logic from data, PreparedStatements ensure that user input
     * is treated as data only, not as part of the SQL command.
     *
     * *A PreparedStatement is reentrant, but not thread-safe and should only be used
     * by one thread (at a time).*
     *
     * @note
     * - Remember that parameter indices in PreparedStatement are 1-based, not 0-based.
     * - To minimizes memory allocation and avoid unnecessary data copying, string and blob
     *   values are set by reference and _MUST_ remain valid until PreparedStatement::execute()
     *   has been called.
     *
     * @warning
     * - PreparedStatement objects are internally managed by the Connection that
     *   created them and are not copyable or movable. Always ensure that the originating
     *   Connection object remains valid for the entire duration of the PreparedStatement's
     *   use. Basically, keep the Connection and PreparedStatement objects in the same scope.
     *   Do not attempt to use PreparedStatement objects (including through references or
     *   pointers) after their Connection has been closed and returned to the pool.
     * - The '?' character should only be used as a parameter placeholder in your SQL
     *   statement. Do not use '?' in string literals, comments, or other SQL contexts.
     */
    class PreparedStatement : private noncopyable {
    public:
        /**
         * @brief Move constructor.
         * @param r PreparedStatement object to move.
         * @private
         */
        PreparedStatement(PreparedStatement&& r) noexcept : t_(r.t_) { r.t_ = nullptr; }
        
        /**
         * @brief Conversion operator to PreparedStatement_T.
         * @return The underlying PreparedStatement_T.
         * @private
         */
        operator PreparedStatement_T() const noexcept { return t_; }
        
        /// @name Parameters
        /// @{

        /**
         * @brief Binds a value to the prepared statement.
         *
         * This method can bind different types of values:
         * - String-like types (convertible to std::string_view)
         * - Numeric types (integral or floating-point; time_t binds as an integer)
         * - Blob-like types (contiguous ranges of bytes)
         * - std::chrono::system_clock::time_point for timestamp values
         * - nullptr_t for SQL NULL values
         *
         * @tparam T The type of the value to bind
         * @param parameterIndex The index of the parameter to bind (1-based)
         * @param x The value to bind
         * @throws sql_exception If a database access error occurs or parameterIndex 
         *         is invalid
         * @note For string-like and blob-like types, the data must remain valid until
         *       execute() is called. This method does not copy the data but stores a
         *       reference to it.
         * @note This method will fail to compile for unsupported types, providing a 
         *       clear error message.
         */
        template<typename T>
        void bind(int parameterIndex, T&& x) {
            if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>) {
                setNull(parameterIndex);
            } else if constexpr (Stringable<std::remove_cvref_t<T>>) {
                std::string_view sv(x);
                except_wrapper(PreparedStatement_setSString(t_, parameterIndex, sv.data(), static_cast<int>(sv.size())));
            } else if constexpr (Numeric<std::remove_cvref_t<T>>) {
                using U = std::remove_cvref_t<T>;
                if constexpr (std::is_floating_point_v<U>) {
                    except_wrapper(PreparedStatement_setDouble(t_, parameterIndex, static_cast<double>(x)));
                } else if constexpr (std::is_signed_v<U>) {
                    if constexpr (sizeof(U) <= sizeof(int))
                        except_wrapper(PreparedStatement_setInt(t_, parameterIndex, static_cast<int>(x)));
                    else
                        except_wrapper(PreparedStatement_setLLong(t_, parameterIndex, static_cast<long long>(x)));
                } else {
                    if constexpr (sizeof(U) >= sizeof(long long)) {
                        if (x > static_cast<U>(std::numeric_limits<long long>::max()))
                            throw sql_exception("Unsigned integer value too large to bind (exceeds LLONG_MAX)");
                    }
                    except_wrapper(PreparedStatement_setLLong(t_, parameterIndex, static_cast<long long>(x)));
                }
            } else if constexpr (Blobable<std::remove_cvref_t<T>>) {
                if (std::empty(x)) {
                    setNull(parameterIndex);
                } else {
                    except_wrapper(PreparedStatement_setBlob(t_, parameterIndex, std::data(x), static_cast<int>(std::size(x))));
                }
            } else if constexpr (TimePoint<std::remove_cvref_t<T>>) {
                except_wrapper(PreparedStatement_setTimestamp(t_, parameterIndex, std::chrono::system_clock::to_time_t(x)));
            } else {
                static_assert(always_false<T>, "Unsupported type for bind");
            }
        }
        
        /**
         * @brief Binds multiple values to the Prepared Statement at once.
         *
         * @param args Values to bind to the Prepared Statement.
         * @throws sql_exception If a database error occurs or if argument count 
         *         is incorrect
         * @note Reference types must remain valid until execute() is called.
         *       This method does not copy any data.
         */
        template<typename... Args>
        void bindValues(Args&&... args) {
            if (sizeof...(Args) != getParameterCount()) {
                throw sql_exception("Number of values doesn't match placeholders in statement");
            }
            bindValuesHelper(1, std::forward<Args>(args)...);
        }
        
        /// @}
        /// @name Functions
        /// @{

        /**
         * @brief Executes the prepared SQL statement.
         * @throws sql_exception If a database access error occurs
         */
        void execute() {
            except_wrapper(PreparedStatement_execute(t_));
        }
        
        /**
         * @brief Gets the number of rows affected by the most recent SQL statement.
         *
         * If used with a transaction, this method should be called *before* commit is
         * executed, otherwise 0 is returned.
         *
         * @return The number of rows changed.
         */
        [[nodiscard]] long long rowsChanged() noexcept {
            return PreparedStatement_rowsChanged(t_);
        }
        
        /// @}
        /// @name Properties
        /// @{

        /**
         * @brief Gets the number of parameters in the prepared statement.
         * @return The number of _in_ parameters in this prepared statement
         */
        [[nodiscard]] int getParameterCount() noexcept {
            return PreparedStatement_getParameterCount(t_);
        }
        
        /// @}

    protected:
        friend class Connection;
        
        /**
         * @brief Constructs a PreparedStatement from a PreparedStatement_T.
         * @param t The underlying PreparedStatement_T.
         * @private
         */
        explicit PreparedStatement(PreparedStatement_T t) : t_(t) {}
        
        /**
         * @brief Executes the prepared SQL query.
         * @return A ResultSet containing the query results.
         * @throws sql_exception If a database access error occurs
         * @private
         */
        [[nodiscard]] ResultSet executeQuery() {
            except_wrapper(
                           ResultSet_T r = PreparedStatement_executeQuery(t_);
                           RETURN ResultSet(r);
                           );
        }
        
    private:
        PreparedStatement_T t_;

        // Sets the parameter at the given index to SQL NULL.
        void setNull(int parameterIndex) { except_wrapper(PreparedStatement_setNull(t_, parameterIndex)); }
        
        // Helper function to recursively bind all values
        template<typename T, typename... Rest>
        void bindValuesHelper(int index, T&& arg, Rest&&... rest) {
            bind(index, std::forward<T>(arg));
            if constexpr (sizeof...(Rest) > 0) {
                bindValuesHelper(index + 1, std::forward<Rest>(rest)...);
            }
        }
    };
    
    
    /**
     * @class Connection
     * @brief Represents a connection to a SQL database system.
     *
     * Use a Connection to execute SQL statements. There are three ways to
     * execute statements: execute() is used to execute SQL
     * statements that do not return a result set. Such statements are INSERT,
     * UPDATE or DELETE. executeQuery() is used to execute a SQL
     * SELECT statement and return a result set. These methods can handle
     * various data types, including binary data, by automatically creating
     * a PreparedStatement when arguments are provided. For more complex
     * scenarios or when reusing statements, you can explicitly create a
     * PreparedStatement object using the prepareStatement() method.
     *
     * The method executeQuery() will return an empty ResultSet (not null)
     * if the SQL statement did not return any values. A ResultSet is valid 
     * until the next call to Connection execute or until the Connection is
     * returned to the ConnectionPool. If an error occurs during execution,
     * an sql_exception is thrown.
     *
     * Any SQL statement that changes the database (basically, any SQL
     * command other than SELECT) will automatically start a transaction
     * if one is not already in effect. Automatically started transactions
     * are committed at the conclusion of the command.
     *
     * Transactions can also be started manually using beginTransaction(). Such
     * transactions usually persist until the next call to commit() or rollback().
     * A transaction will also rollback if the database is closed or if an
     * error occurs. Nested transactions are not allowed.
     *
     * ## Examples
     *
     * ### Basic Query Execution
     * @code
     * Connection con = pool.getConnection();
     * ResultSet result = con.executeQuery("SELECT name, age FROM users WHERE id = ?", 1);
     * if (result.next()) {
     *     std::cout << "Name: " << result.getString("name").value_or("N/A")
     *               << ", Age: " << result.getInt("age") << std::endl;
     * }
     * @endcode
     *
     * ### Transaction Example
     *
     * @code
     * try {
     *     Connection con = pool.getConnection();
     *     con.beginTransaction();
     *     con.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?", 100.0, 1);
     *     con.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?", 100.0, 2);
     *     con.commit();
     *     std::cout << "Transfer successful" << std::endl;
     * } catch (const sql_exception& e) {
     *     // See note below why we don't have to explicit call rollback here
     *     std::cerr << "Transfer failed (" << e.error_code << "): << e.what() << std::endl;
     * }
     * @endcode
     *
     * ### Using PreparedStatement
     * @code
     * Connection con = pool.getConnection();
     * auto stmt = con.prepareStatement("INSERT INTO logs (message, timestamp) VALUES (?, ?)");
     * stmt.bindValues("User logged in", std::time(nullptr));
     * stmt.execute();
     * std::cout << "Rows affected: " << stmt.rowsChanged() << std::endl;
     * @endcode
     *
     * _A Connection is reentrant, but not thread-safe and should only be used by one
     * thread at a time._
     *
     * @note When a Connection object goes out of scope, it is automatically
     * returned to the pool. If the connection is still in a transaction at
     * this point, the transaction will be automatically rolled back, ensuring
     * data integrity even in the face of exceptions.
     *
     * @warning Connection objects are internally managed by the ConnectionPool that
     * created them and are not copyable or movable. Always ensure that the originating
     * ConnectionPool object remains valid for the entire duration of the Connection's
     * use. It is recommended to obtain a Connection, use it for a specific task, and
     * then close it (return it to the pool) as soon as possible, rather than holding
     * onto it for extended periods.
     */
    class Connection : private noncopyable {
    public:
        /**
         * @brief Destructor, closes the connection and returns it to the pool.
         * @private
         */
        ~Connection() { if (t_) close(); }
        
        /**
         * @brief Implicit conversion to the underlying Connection_T type.
         * @return The underlying Connection_T object.
         * @private
         */
        operator Connection_T() const noexcept { return t_; }
        
        /// @name Properties
        /// @{

        /**
         * @brief Sets the query timeout for this Connection.
         *
         * If the limit is exceeded, the statement will return immediately with an error.
         * The timeout is set per connection/session. The default is no query timeout.
         *
         * @param ms Timeout in milliseconds.
         */
        void setQueryTimeout(int ms) noexcept { Connection_setQueryTimeout(t_, ms); }
        
        /**
         * @brief Gets the query timeout for this Connection.
         * @return The query timeout in milliseconds.
         */
        [[nodiscard]] int getQueryTimeout() noexcept { return Connection_getQueryTimeout(t_); }
        
        /**
         * @brief Sets the maximum number of rows for ResultSet objects.
         *
         * If the limit is exceeded, the excess rows are silently dropped.
         *
         * @param max Maximum number of rows.
         */
        void setMaxRows(int max) noexcept { Connection_setMaxRows(t_, max); }
        
        /**
         * @brief Gets the maximum number of rows for ResultSet objects.
         * @return The maximum number of rows.
         */
        [[nodiscard]] int getMaxRows() noexcept { return Connection_getMaxRows(t_); }
        
        /**
         * @brief Sets the number of rows to fetch for ResultSet objects.
         *
         * The default value is 100, meaning that a ResultSet will prefetch rows in
         * batches of 100 rows to reduce the network roundtrip to the database. This
         * value can also be set via the URL parameter `fetch-size` to apply to all
         * connections. This method and the concept of pre-fetching rows are only
         * applicable to MySQL and Oracle.
         *
         * @param rows Number of rows to fetch.
         */
        void setFetchSize(int rows) noexcept { Connection_setFetchSize(t_, rows); }
        
        /**
         * @brief Gets the number of rows to fetch for ResultSet objects.
         * @return The number of rows to fetch.
         */
        [[nodiscard]] int getFetchSize() noexcept { return Connection_getFetchSize(t_); }
        
        /// @}
        /// @name Functions
        /// @{

        /**
         * @brief Pings the database server to check if the connection is alive.
         * @return true if the connection is alive, false otherwise.
         */
        [[nodiscard]] bool ping() noexcept { return Connection_ping(t_); }
        
        /**
         * @brief Clears any ResultSet and PreparedStatements in the Connection.
         *
         * Normally it is not necessary to call this method, but for some
         * implementations (SQLite) it *may, in some situations,* be
         * necessary to call this method if an execution sequence error occurs.
         */
        void clear() noexcept { Connection_clear(t_); }
        
        /**
         * @brief Returns the connection to the connection pool.
         *
         * The same as calling ConnectionPool::returnConnection() on a connection.
         * If the connection is in an uncommitted transaction, rollback is called.
         * It is an unchecked error to attempt to use the Connection after this
         * method was called
         */
        void close() noexcept {
            if (t_) {
                Connection_close(t_);
                setClosed();
            }
        }
        
        /**
         * @brief Begins a new transaction with optional isolation level.
         *
         * Example usage:
         * @code
         *   // Use default isolation level
         *   connection.beginTransaction();
         *
         *   // Specify isolation level
         *   connection.beginTransaction(TRANSACTION_SERIALIZABLE);
         * @endcode
         *
         * @param type The transaction isolation level (default: TRANSACTION_DEFAULT).
         *             @see TRANSACTION_TYPE enum for available options.
         * @throws sql_exception If a database error occurs or if a transaction is already in progress.
         * @note All transactions must be ended with either commit() or rollback().
         *       Nested transactions are not supported.
         */
        void beginTransaction(TRANSACTION_TYPE type = TRANSACTION_DEFAULT) {
            except_wrapper(Connection_beginTransactionType(t_, type));
        }
        
        /**
         * @brief Checks if this Connection is in an uncommitted transaction.
         * @return true if in a transaction, false otherwise.
         */
        [[nodiscard]] bool inTransaction() const noexcept {
            return Connection_inTransaction(t_);
        }
        
        /**
         * @brief Commits the current transaction.
         * @throws sql_exception If a database error occurs.
         */
        void commit() { except_wrapper(Connection_commit(t_)); }
        
        /**
         * @brief Rolls back the current transaction.
         *
         * This method will first call clear() before performing the rollback to
         * clear any statements in progress such as selects.
         *
         * @throws sql_exception If a database error occurs.
         */
        void rollback() { except_wrapper(Connection_rollback(t_)); }
        
        /**
         * @brief Gets the last inserted row ID for auto-increment columns.
         * @return The last inserted row ID.
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
         * con.execute("INSERT INTO users(name) VALUES(?)", "Min-seo");
         * long long id = con.lastRowId();
         *
         * // For PostgreSQL (assuming id is the auto increment column name):
         * ResultSet r = con.executeQuery("INSERT INTO users(name) VALUES(?) RETURNING id", "Min-seo");
         * long long id = r.next() ? r.getLLong(1) : -1;
         *
         * // For Oracle:
         * ResultSet r = con.executeQuery("INSERT INTO users(name) VALUES(?) RETURNING ROWID", "Min-seo");
         * long long id = r.next() ? r.getLLong(1) : -1;
         * @endcode
         *
         * @see ResultSet
         */
        [[nodiscard]] long long lastRowId() noexcept { return Connection_lastRowId(t_); }
        
        /**
         * @brief Gets the number of rows affected by the last execute() statement.
         *
         * If used with a transaction, this method should be called *before* commit is
         * executed, otherwise 0 is returned.
         *
         * @return The number of rows changed.
         */
        [[nodiscard]] long long rowsChanged() noexcept { return Connection_rowsChanged(t_); }
        
        /**
         * @brief Executes a SQL statement, with or without parameters.
         *
         * This method can be used in two ways:
         * 1. With only a SQL string, which directly executes the statement(s).
         * 2. With a SQL string and additional arguments, which creates a PreparedStatement,
         *    binds the provided parameters, and then executes it.
         *
         * @param sql The SQL statement to execute.
         * @param args (Optional) Arguments to bind to the statement. These can be of various types,
         *             including string-like types, numeric types, blob-like types,
         *             std::chrono::system_clock::time_point, and nullptr.
         * @throws sql_exception If a database access error occurs or if the types of the provided
         *                       arguments don't match the expected types in the SQL statement.
         * @note When used without arguments, this method is more efficient as it doesn't create
         *       a PreparedStatement. When used with arguments, it provides protection against SQL
         *       injection.
         *
         * Example usage:
         * @code
         * // Without parameters
         * con.execute("DELETE FROM users WHERE inactive = true");
         *
         * // With parameters
         * con.execute("INSERT INTO users (name, age) VALUES (?, ?)", "John Doe", 30);
         * @endcode
         */
        template<typename... Args>
        void execute(const std::string& sql, Args&&... args) {
            if constexpr (sizeof...(Args) == 0) {
                except_wrapper(Connection_execute(t_, "%s", sql.c_str()));
            } else {
                PreparedStatement p(this->prepareStatement(sql));
                bindValues(p, std::forward<Args>(args)...);
                p.execute();
            }
        }
        
        /**
         * @brief Executes a SQL query and returns a ResultSet.
         *
         * This method can be used in two ways:
         * 1. With only a SQL string, which directly executes the query.
         * 2. With a SQL string and additional arguments, which creates a PreparedStatement,
         *    binds the provided parameters, and then executes it.
         *
         * @param sql The SQL query to execute.
         * @param args (Optional) Arguments to bind to the query. These can be of various types,
         *             including string-like types, numeric types, blob-like types,
         *             std::chrono::system_clock::time_point, and nullptr.
         * @return A ResultSet containing the query results.
         * @throws sql_exception If a database access error occurs or if the types of the provided
         *                       arguments don't match the expected types in the SQL query.
         * @note When used without arguments, this method is more efficient as it doesn't create
         *       a PreparedStatement. When used with arguments, it provides protection against SQL
         *       injection.
         *
         * Example usage:
         * @code
         * // Without parameters
         * auto result1 = con.executeQuery("SELECT * FROM users");
         *
         * // With parameters
         * auto result2 = con.executeQuery("SELECT * FROM users WHERE age > ? AND name LIKE ?", 18, "John%");
         * @endcode
         */
        template<typename... Args>
        [[nodiscard]] ResultSet executeQuery(const std::string& sql, Args&&... args) {
            if constexpr (sizeof...(Args) == 0) {
                except_wrapper(
                               ResultSet_T r = Connection_executeQuery(t_, "%s", sql.c_str());
                               RETURN ResultSet(r);
                               );
            } else {
                PreparedStatement p(this->prepareStatement(sql));
                bindValues(p, std::forward<Args>(args)...);
                return p.executeQuery();
            }
        }
        
        /**
         * @brief Prepares a SQL statement for execution.
         *
         * This method creates a PreparedStatement object that can be reused with different
         * parameters. It's particularly useful for statements that will be executed multiple
         * times with different values.
         *
         * @param sql The SQL statement to prepare.
         * @return A PreparedStatement object.
         * @throws sql_exception If a database error occurs during preparation.
         *
         * Example usage:
         * @code
         * auto stmt = con.prepareStatement("INSERT INTO users (name, age) VALUES (?, ?)");
         * for (const auto& user : users) {
         *     stmt.bindValues(user.name, user.age);
         *     stmt.execute();
         * }
         * @endcode
         */
        [[nodiscard]] PreparedStatement prepareStatement(const std::string& sql) {
            except_wrapper(
                           PreparedStatement_T p = Connection_prepareStatement(t_, "%s", sql.c_str());
                           RETURN PreparedStatement(p);
                           );
        }
        
        /**
         * @brief Gets the last SQL error message.
         * @return The last error message as a string view.
         */
        [[nodiscard]] std::optional<std::string_view> getLastError() const noexcept {
            return _to_optional(Connection_getLastError(t_));
        }

        /// @}
        
        /**
         * @brief Checks if the specified database system is supported.
         * @param url A database URL string or protocol.
         * @return true if supported, false otherwise.
         */
        [[nodiscard]] static bool isSupported(const std::string& url) noexcept {
            return Connection_isSupported(url.c_str());
        }

    protected:
        friend class ConnectionPool;
        
        /**
         * @brief Constructs a Connection with an existing Connection_T object.
         * @param C The existing Connection_T object.
         * @private
         */
        explicit Connection(Connection_T C) : t_(C) {}
        
        /**
         * @brief Marks the connection as closed.
         * @private
         */
        void setClosed() noexcept { t_ = nullptr; }
        
        /**
         * @brief Helper function to bind values to a PreparedStatement.
         * @private
         */
        template<typename... Args>
        void bindValues(PreparedStatement& stmt, Args&&... args) {
            int index = 1;
            (stmt.bind(index++, std::forward<Args>(args)), ...);
        }
        
    private:
        Connection_T t_;
    };
    
    
    /**
     * @class ConnectionPool
     * @brief Represents a database connection pool.
     *
     * A ConnectionPool can be used to obtain connections to a database and execute statements.
     * This class opens a number of database connections and allows callers to obtain and use
     * a database connection in a reentrant manner. Applications can instantiate as many
     * ConnectionPool objects as needed and against as many different database systems as needed.
     *
     * ## Connection URL:
     *
     * The URL given to a Connection Pool at creation time specifies a database
     * connection in the standard URL format:
     *
     * ```
     * database://[user:password@][host][:port]/database[?propertyName1][=propertyValue1][&propertyName2][=propertyValue2]...
     * ```
     *
     * The property names `user` and `password` are always recognized and specify how to log in
     * to the database. Other properties depend on the database server in question. Username and
     * password can alternatively be specified in the auth-part of the URL. If port number is
     * omitted, the default port number for the database server is used.
     *
     * ### MySQL:
     *
     * Example URL for MySQL:
     * ```
     * mysql://localhost:3306/test?user=root&password=swordfish
     * ```
     * Or using the auth-part of the URL:
     * ```
     * mysql://root:swordfish@localhost:3306/test
     * ```
     * See [mysql options](mysqloptions.html) for all properties that can be set for a mysql connection URL.
     *
     * ### SQLite:
     *
     * For SQLite, the URL should specify a database file. SQLite uses [pragma commands](http://sqlite.org/pragma.html) for performance tuning and other special purpose
     * database commands. Pragma syntax in the form `name=value` can be added as properties to the URL. In addition to pragmas, the following properties are supported:
     *
     * - `serialized=true` - Make SQLite switch to serialized mode
     *   if value is true, otherwise multi-thread mode is used (the default).
     * - `shared-cache=true` - Make SQLite use shared-cache if value is true.
     *   Using shared cache can significantly reduce database lock errors in
     *   some scenarios where two or more connections might write to the database.
     *   It is also recommended to build libsqlite and libzdb with unlock  notify.
     *
     * Example URL for SQLite (with recommended pragmas):
     * ```
     * sqlite:///var/sqlite/test.db?synchronous=normal&foreign_keys=on&journal_mode=wal&temp_store=memory
     * ```
     *
     * ### PostgreSQL:
     *
     * Example URL for PostgreSQL:
     * ```
     * postgresql://localhost:5432/test?user=root&password=swordfish
     * ```
     * Or using the auth-part and SSL:
     * ```
     * postgresql://root:swordfish@localhost/test?use-ssl=true
     * ```
     * See [postgresql options](postgresoptions.html) for all properties that can be set for a postgresql connection URL.
     *
     * ### Oracle:
     *
     * Example URL for Oracle:
     * ```
     * oracle://localhost:1521/servicename?user=scott&password=tiger
     * ```
     * Or using the auth-part and SYSDBA role:
     * ```
     * oracle://sys:password@localhost:1521/servicename?sysdba=true
     * ```
     * See [oracle options](oracleoptions.html) for all properties that can be set for an oracle connection URL.
     *
     * ## Pool Management:
     *
     * The pool is designed to dynamically manage the number of active connections
     * based on usage patterns. A `reaper` thread is automatically started when the
     * pool is initialized to sweep through the pool at regular intervals (default
     * every 60 seconds) and close connections that have been inactive for a specified
     * time (default 90 seconds).
     *
     * ## Realtime inspection:
     *
     * Three methods can be used to inspect the pool at runtime. The method
     * size() returns the number of connections in the pool, that is, both active
     * and inactive connections. The method active() returns the number of active
     * connections, i.e., those connections in current use by your application.
     * The method isFull() can be used to check if the pool is full and unable to
     * return a connection.
     *
     * ## Example Usage:
     *
     * @code
     * ConnectionPool pool("mysql://localhost/test?user=root&password=swordfish");
     * pool.start();
     * // ...
     * Connection con = pool.getConnection();
     * ResultSet result = con.executeQuery("SELECT id, name, photo FROM employee WHERE salary > ?", 50000);
     * while (result.next()) {
     *     int id = result.getInt("id");
     *     auto name = result.getString("name");
     *     auto photo = result.getBlob("photo");
     *     // Process data...
     * }
     * @endcode
     *
     * @note This ConnectionPool is thread-safe.
     *
     * @warning A ConnectionPool is neither copyable nor movable. It is designed to be a
     * long-lived object that manages database connections throughout the lifetime of your
     * application. Typically, you would instantiate one or more ConnectionPool objects
     * as part of a resource management class or in the global scope of your application.
     */
    class ConnectionPool : private noncopyable {
    public:
        /**
         * @brief Constructs a ConnectionPool with the given URL string.
         * @param url The database connection URL string.
         * @throws sql_exception If the URL is invalid.
         */
        explicit ConnectionPool(const std::string& url) : url_(url) {
            if (!url_)
                throw sql_exception("Invalid URL");
            t_ = ConnectionPool_new(url_);
        }
        
        /**
         * @brief Constructs a ConnectionPool with a URL object.
         * @param url The database connection URL object to move from.
         * @throws sql_exception If the URL is invalid.
         */
        explicit ConnectionPool(URL&& url) : url_(std::move(url)) {
            if (!url_)
                throw sql_exception("Invalid URL");
            t_ = ConnectionPool_new(url_);
        }
        
        /**
         * @brief Destructor, releases resources allocated by the pool.
         * @private
         */
        ~ConnectionPool() { ConnectionPool_free(&t_); }
        
        /**
         * @brief Implicit conversion to the underlying ConnectionPool_T type.
         * @return The underlying ConnectionPool_T object.
         * @private
         */
        operator ConnectionPool_T() noexcept { return t_; }
        
        /// @name Properties
        /// @{

        /**
         * @brief Retrieves the database type for this ConnectionPool
         * @return The CONNECTIONPOOL_TYPE representing the database backend for this ConnectionPool
         * @see CONNECTIONPOOL_TYPE
         */
        CONNECTIONPOOL_TYPE getType() const noexcept { return ConnectionPool_getType(t_); }

        /**
         * @brief Gets the URL of the connection pool.
         * @return The URL of the connection pool.
         */
        [[nodiscard]] const URL& getURL() const noexcept { return url_; }
        
        /**
         * @brief Sets the number of initial connections in the pool.
         * @param initialConnections The number of initial connections.
         */
        void setInitialConnections(int initialConnections) noexcept {
            ConnectionPool_setInitialConnections(t_, initialConnections);
        }
        
        /**
         * @brief Gets the number of initial connections in the pool.
         * @return The number of initial connections.
         */
        [[nodiscard]] int getInitialConnections() noexcept {
            return ConnectionPool_getInitialConnections(t_);
        }
        
        /**
         * @brief Sets the maximum number of connections in the pool.
         *
         * If max connections has been reached, getConnection() will fail on
         * the next call. It is a checked runtime error for maxConnections to
         * be less than initialConnections.
         *
         * @param maxConnections The maximum number of connections.
         */
        void setMaxConnections(int maxConnections) noexcept {
            ConnectionPool_setMaxConnections(t_, maxConnections);
        }
        
        /**
         * @brief Gets the maximum number of connections in the pool.
         * @return The maximum number of connections.
         */
        [[nodiscard]] int getMaxConnections() noexcept {
            return ConnectionPool_getMaxConnections(t_);
        }
        
        /**
         * @brief Sets the connection inactive timeout value in seconds.
         *
         * The method reapConnections(), if called, will close inactive
         * connections in the pool which have not been in use for
         * `connectionTimeout` seconds. The default connectionTimeout is
         * 90 seconds.
         *
         * @param connectionTimeout The timeout value in seconds. It is a
         * checked runtime error for connectionTimeout to be <= 0
         */
        void setConnectionTimeout(int connectionTimeout) noexcept {
            ConnectionPool_setConnectionTimeout(t_, connectionTimeout);
        }
        
        /**
         * @brief Gets the connection timeout value.
         * @return The connection timeout value in seconds.
         */
        [[nodiscard]] int getConnectionTimeout() noexcept {
            return ConnectionPool_getConnectionTimeout(t_);
        }
        
        using AbortHandler = std::function<void(std::string_view)>;
        
        /**
         * @brief Sets the function to call if a fatal error occurs in the library.
         *
         * In practice this means Out-Of-Memory errors or uncaught exceptions.
         * Clients may optionally provide this function. If not provided
         * the library will call `abort(3)` or `exit(1)` upon encountering a fatal error.
         * It is an unchecked runtime error to continue using the library after
         * the `abortHandler` was called.
         *
         * @param abortHandler The handler function to call on fatal errors.
         */
        void setAbortHandler(AbortHandler abortHandler = nullptr) noexcept {
            g_abortHandler = std::move(abortHandler);
            ConnectionPool_setAbortHandler(t_, g_abortHandler ? bridgeAbortHandler : nullptr);
        }
        
        /**
         * @brief Customizes the reaper thread behavior or disables it.
         *
         * By default, a reaper thread is automatically started when the pool
         * is initialized, with a default sweep interval of 60 seconds. This method
         * allows you to change the sweep interval or disable the reaper entirely.
         *
         * The reaper thread closes inactive Connections in the pool, down to the
         * initial connection count. An inactive Connection is closed if its
         * `connectionTimeout` has expired. Active Connections (those in current use)
         * are never closed by this thread.
         *
         * This method can be called before or after ConnectionPool::start(). If
         * called after start, the changes will take effect on the next sweep cycle.
         *
         * @param sweepInterval Number of seconds between sweeps of the reaper thread.
         *        Set to 0 or a negative value to disable the reaper thread, **before**
         *        calling ConnectionPool::start().
         */
        void setReaper(int sweepInterval) noexcept {
            ConnectionPool_setReaper(t_, sweepInterval);
        }
        
        /// @}
        
        /**
         * @brief Gets the current number of connections in the pool.
         * @return The total number of connections in the pool.
         */
        [[nodiscard]] int size() noexcept { return ConnectionPool_size(t_); }
        
        /**
         * @brief Gets the number of active connections in the pool.
         * @return The number of active connections in the pool.
         */
        [[nodiscard]] int active() noexcept { return ConnectionPool_active(t_); }
        
        /**
         * @brief Checks if the pool is full.
         *
         * The pool is full if the number of *active* connections equals max
         * connections and the pool is unable to return a connection.
         *
         * @return true if pool is full, false otherwise
         * @note A full pool is unlikely to occur in practice if you ensure that
         *       connections are returned to the pool after use.
         */
        [[nodiscard]] bool isFull() noexcept { return ConnectionPool_isFull(t_); }
        
        /**
         * @brief Prepares the pool for active use.
         *
         * This method must be called before the pool is used. It will connect to the database
         * server, create the initial connections for the pool, and start the reaper
         * thread with default settings, unless previously disabled via setReaper().
         *
         * @throws sql_exception If a database error occurs.
         */
        void start() { except_wrapper(ConnectionPool_start(t_)); }
        
        /**
         * @brief Gracefully terminates the pool.
         *
         * This method should be the last one called on a given instance
         * of this component. Calling this method closes down all connections in the
         * pool, disconnects the pool from the database server, and stops the reaper
         * thread if it was started.
         *
         * @throws sql_exception If there are active connections.
         */
        void stop() {
            if (this->active() > 0) {
                throw sql_exception("Trying to stop the pool with active Connections. "
                                    "Please close all active Connections first");
            }
            except_wrapper(ConnectionPool_stop(t_));
        }
        
        /**
         * @brief Gets a connection from the pool.
         *
         * The returned Connection is guaranteed to be alive and connected to the database.
         *
         * An sql_exception may be thrown if the pool is full or if a database error occurs
         * (e.g., network issues, database unavailability).
         *
         * Here's a basic example of how to use getConnection and handle potential errors:
         *
         * ```cpp
         * try {
         *     Connection con = pool.getConnection();
         *     // Use the connection ...
         * } catch (const sql_exception& e) {
         *     std::cerr << "Error: " << e.what() << std::endl;
         * }
         * ```
         *
         * @return A Connection object.
         * @throws sql_exception If a database connection cannot be obtained
         */
        [[nodiscard]] Connection getConnection() {
            except_wrapper(
                           Connection_T c = ConnectionPool_getConnectionOrException(t_);
                           RETURN Connection(c);
                           );
        }
        
        /**
         * @brief Returns a connection to the pool.
         *
         * The same as calling Connection::close() on a connection. If the connection
         * is in an uncommitted transaction, rollback is called. It is an unchecked
         * error to attempt to use the Connection after this method is called.
         *
         * @param con The Connection to return.
         */
        void returnConnection(Connection& con) noexcept { con.close(); }
        
        /**
         * @brief Reaps inactive connections in the pool.
         * @return The number of connections reaped.
         */
        int reapConnections() noexcept {
            return ConnectionPool_reapConnections(t_);
        }
                
    private:
        URL url_;
        ConnectionPool_T t_;
    };
    
} // namespace zdb

#endif
