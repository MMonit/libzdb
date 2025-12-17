/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 * Copyright (c) 1994,1995,1996,1997 by David R. Hanson.
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

#ifndef EXCEPTION_INCLUDED
#define EXCEPTION_INCLUDED
#include <setjmp.h>
#include <pthread.h>

/**
 * @brief An **Exception** indicates an error condition from which recovery may
 * be possible.
 *
 * The Library *raises* exceptions, which can be handled by recovery code, if
 * recovery is possible. When an exception is raised, it is handled by the
 * handler that was most recently instantiated. If no handlers are defined an
 * exception will cause the library to call its abort handler to abort with
 * an error message.
 *
 * Handlers are instantiated by the TRY-CATCH and TRY-FINALLY statements,
 * which are implemented as macros in this interface. These statements handle
 * nested exceptions and manage exception-state data. The syntax of the
 * TRY-CATCH statement is,
 *
 * ```c
 * TRY
 *      S
 * CATCH(e1)
 *      S1
 * CATCH(e2)
 *      S2
 * [...]
 * CATCH(en)
 *      Sn
 * END_TRY;
 * ```
 *
 * The TRY-CATCH statement establishes handlers for the exceptions named
 * `e1, e2,.., en` and execute the statements **S**.
 * If no exceptions are raised by **S**, the handlers are dismantled and
 * execution continues at the statement after the END_TRY. If **S** raises
 * an exception `e` which is one of *e1..en* the execution
 * of **S** is interrupted and control transfers immediately to the
 * statements following the relevant CATCH clause. If **S** raises an
 * exception that is *not* one of *e1..en*, the exception will raise
 * up the call-stack and unless a previous installed handler catch the
 * exception, it will cause the application to abort.
 *
 * The TRY-FINALLY statement is similar to TRY-CATCH but in addition
 * adds a FINALLY clause which is always executed, regardless if an exception
 * was raised or not. The syntax of the TRY-FINALLY statement is,
 * ```c
 * TRY
 *      S
 * CATCH(e1)
 *      S1
 * CATCH(e2)
 *      S2
 *      [...]
 * CATCH(en)
 *      Sn
 * FINALLY
 *      Sf
 * END_TRY;
 * ```
 *
 * Note that `Sf` is executed whether **S** raises an exception
 * or not. One purpose of the TRY-FINALLY statement is to give clients an
 * opportunity to "clean up" when an exception occurs. For example,
 * ```c
 * TRY
 * {
 *      Connection_execute(c, sql);
 * }
 * FINALLY
 * {
 *      Connection_close(c);
 * }
 * END_TRY;
 * ```
 * closes the database Connection regardless if an exception
 * was thrown or not by the code in the TRY-block. The above example also
 * demonstrates that FINALLY can be used without an exception handler, if an
 * exception was thrown it will be rethrown after the control reaches the
 * end of the finally block. Meaning that we can cleanup even if an exception
 * was thrown and the exception will automatically propagate up the call stack
 * afterwards.
 *
 * Finally, the RETURN statement, defined in this interface, must be used
 * instead of C return statements inside a try-block. If any of the
 * statements in a try block must do a return, they **must** do so with
 * this macro instead of the usual C return statement.
 *
 *
 * ## Recommended: Use TRY-ELSE
 *
 * For most use cases, we recommend using TRY-ELSE rather than TRY-CATCH.
 * The ELSE block catches *any* exception, which simplifies client code
 * since libzdb can throw various Exception types. Unless you need to
 * differentiate between specific exception types, TRY-ELSE provides
 * a cleaner and more robust pattern:
 *
 * ```c
 * TRY
 * {
 *      Connection_execute(c, sql);
 * }
 * ELSE
 * {
 *      // Handle error
 * }
 * END_TRY;
 * ```
 *
 * Use TRY-CATCH only when you need to handle different exception types
 * differently. For general error handling where the response is the same
 * regardless of the exception type, TRY-ELSE is the preferred approach.
 *
 *
 * ## Exception details
 *
 * Inside an exception handler, details about an exception are
 * available in the variable `Exception_frame`. The following
 * demonstrates usage of this variable to provide detailed logging of an
 * exception. For SQL errors, Connection_getLastError() can also be used,
 * though `Exception_frame` is recommended since in addition to
 * SQL errors, it also covers API errors not directly related to SQL.
 *
 * ```c
 * TRY
 * {
 *      <code that can throw an exception>
 * }
 * ELSE
 * {
 *      fprintf(stderr, "%s: %s raised in %s at %s:%d\n",
 *              Exception_frame.exception->name,
 *              Exception_frame.message,
 *              Exception_frame.func,
 *              Exception_frame.file,
 *              Exception_frame.line);
 * }
 * END_TRY;
 * ```
 *
 *
 * ## Error codes
 *
 * In addition to the exception message, `Exception_frame.errorCode`
 * provides the numeric error code from the underlying database when
 * available. This allows for more robust error handling. For example,
 * to handle a MySQL deadlock:
 *
 * ```c
 * TRY
 * {
 *      Connection_execute(c, sql);
 * }
 * ELSE
 * {
 *      if (Exception_frame.errorCode == ER_LOCK_DEADLOCK) {
 *              // Retry the transaction
 *      } else {
 *              log("Database error %d: %s\n",
 *                  Exception_frame.errorCode,
 *                  Exception_frame.message);
 *      }
 * }
 * END_TRY;
 * ```
 *
 * The error code is database-specific; consult your database's documentation
 * for the meaning of specific codes (e.g., MySQL error codes, PostgreSQL
 * SQLSTATE values, etc.). A value of 0 typically indicates no specific
 * error code was provided.
 *
 *
 * ## Database-Specific Error Codes
 *
 * The error code in `Exception_frame.errorCode` is database-specific. Each
 * database backend provides error codes in their own format:
 *
 * ### PostgreSQL
 *
 * PostgreSQL uses SQLSTATE codes, a five-character standard defined by
 * ISO/IEC 9075. libzdb encodes these as integers in SQLState.h.
 *
 * ```c
 *
 * TRY
 *     Connection_execute(c, sql);
 * ELSE
 *     if (Exception_frame.errorCode == SQLSTATE_unique_violation) {
 *         // Handle duplicate key (SQLSTATE 23505)
 *     } else if (Exception_frame.errorCode == SQLSTATE_foreign_key_violation) {
 *         // Handle FK violation (SQLSTATE 23503)
 *     } else if (Exception_frame.errorCode == SQLSTATE_deadlock_detected) {
 *         // Handle deadlock - consider retry (SQLSTATE 40P01)
 *     } else if (Exception_frame.errorCode == SQLSTATE_lock_not_available) {
 *         // Handle lock timeout - consider retry (SQLSTATE 55P03)
 *     }
 * END_TRY;
 * ```
 *
 * For debugging, use SQLState_toString() to convert the code back to
 * its standard 5-character representation:
 *
 * ```c
 * char sqlstate[6];
 * SQLState_toString(Exception_frame.errorCode, sqlstate);
 * printf("SQLSTATE: %s\n", sqlstate);  // e.g., "40P01"
 * ```
 *
 * See: https://www.postgresql.org/docs/current/errcodes-appendix.html
 *
 * ### MySQL/MariaDB
 *
 * MySQL error codes are native integers from mysql_errno(). Common codes
 * are defined in MySQL's errmsg.h and mysqld_error.h headers. Examples:
 *
 * - 1062 (ER_DUP_ENTRY) - Duplicate entry for key
 * - 1213 (ER_LOCK_DEADLOCK) - Deadlock found
 * - 1205 (ER_LOCK_WAIT_TIMEOUT) - Lock wait timeout
 * - 1452 (ER_NO_REFERENCED_ROW_2) - Foreign key constraint fails
 *
 * ```c
 * #include <mysqld_error.h>  // If available
 *
 * TRY
 *     Connection_execute(c, sql);
 * ELSE
 *     if (Exception_frame.errorCode == 1062) {  // ER_DUP_ENTRY
 *         // Handle duplicate key
 *     }
 * END_TRY;
 * ```
 *
 * See: https://dev.mysql.com/doc/mysql-errors/8.0/en/server-error-reference.html
 *
 * ### SQLite
 *
 * SQLite uses extended error codes from sqlite3_extended_errcode(). These
 * are defined in sqlite3.h. Examples:
 *
 * - SQLITE_CONSTRAINT_UNIQUE (2067) - UNIQUE constraint failed
 * - SQLITE_CONSTRAINT_PRIMARYKEY (1555) - PRIMARY KEY constraint failed
 * - SQLITE_CONSTRAINT_FOREIGNKEY (787) - FOREIGN KEY constraint failed
 * - SQLITE_BUSY (5) - Database is locked
 * - SQLITE_LOCKED (6) - Database table is locked
 *
 * ```c
 * #include <sqlite3.h>
 *
 * TRY
 *     Connection_execute(c, sql);
 * ELSE
 *     if (Exception_frame.errorCode == SQLITE_CONSTRAINT_UNIQUE) {
 *         // Handle duplicate key
 *     }
 * END_TRY;
 * ```
 *
 * See: https://www.sqlite.org/rescode.html
 *
 * ### Oracle
 *
 * Oracle uses ORA- error numbers (the numeric portion without the ORA- prefix).
 * Common codes include:
 *
 * - 1 (ORA-00001) - Unique constraint violated
 * - 60 (ORA-00060) - Deadlock detected while waiting for resource
 * - 1017 (ORA-01017) - Invalid username/password
 * - 1400 (ORA-01400) - Cannot insert NULL
 * - 1438 (ORA-01438) - Value larger than allowed precision
 * - 2291 (ORA-02291) - Integrity constraint violated - parent key not found
 * - 2292 (ORA-02292) - Integrity constraint violated - child record found
 *
 * ```c
 * TRY
 *     Connection_execute(c, sql);
 * ELSE
 *     if (Exception_frame.errorCode == 1) {
 *         // Handle unique constraint violation (ORA-00001)
 *     } else if (Exception_frame.errorCode == 60) {
 *         // Handle deadlock - consider retry (ORA-00060)
 *     }
 * END_TRY;
 * ```
 *
 * See: https://docs.oracle.com/en/database/oracle/oracle-database/19/errmg/
 *
 *
 * ## Volatile and assignment inside a try-block
 *
 * A variable declared outside a try-block and assigned a value inside said
 * block should be declared `volatile` if the variable will be
 * accessed from an exception handler. Otherwise the compiler will/may
 * optimize away the value set in the try-block and the handler will not see
 * the new value. Declaring the variable volatile is only necessary
 * if the variable is to be used inside a CATCH or ELSE block. Example:
 * ```c
 * volatile int i = 0;
 * TRY
 * {
 *      i = 1;
 *      THROW(SQLException, "SQLException");
 * }
 * ELSE
 * {
 *      assert(i == 1); // Unless declared volatile i would be 0 here
 * }
 * END_TRY;
 * assert(i == 1); // i will be 1 here regardless if it is declared volatile or not
 * ```
 *
 *
 * ## Thread-safe
 *
 * The Exception stack is stored in a thread-specific variable so Exceptions
 * are made thread-safe. *This means that Exceptions are thread local and an
 * Exception thrown in one thread cannot be caught in another thread*.
 * This also means that clients must handle Exceptions per thread and cannot
 * use one TRY-ELSE block in the main program to catch all Exceptions. This is
 * only possible if no threads were started.
 *
 * This implementation is a minor modification of the Except code found in
 * [David R. Hanson's](http://www.drhanson.net/) excellent
 * book [C Interfaces and Implementations](http://www.cs.princeton.edu/software/cii/).
 * @see SQLException.h
 * @file
 */


#define T Exception_T
/** @cond hide */
#ifndef CLANG_ANALYZER_NORETURN
#if defined(__clang__)
#define CLANG_ANALYZER_NORETURN __attribute__((analyzer_noreturn))
#else
#define CLANG_ANALYZER_NORETURN
#endif
#endif
typedef struct T {
        const char *name;
} T;
#define EXCEPTION_MESSAGE_LENGTH 512
typedef struct Exception_Frame Exception_Frame;
struct Exception_Frame {
        int line;
        int errorCode;
        sigjmp_buf env;
        const char *func;
        const char *file;
        const T *exception;
        Exception_Frame *prev;
        char message[EXCEPTION_MESSAGE_LENGTH + 1];
};
enum { Exception_entered=0, Exception_thrown, Exception_handled, Exception_finalized };
extern pthread_key_t Exception_stack;
void Exception_init(void);
void Exception_reset(void);
void Exception_vthrow(const T *e, int errorCode, const char *func, const char *file, int line, const char *cause, ...) CLANG_ANALYZER_NORETURN;
void Exception_throw(const T *e, int errorCode, const char *func, const char *file, int line, const char *message) CLANG_ANALYZER_NORETURN;

#define pop_exception_stack pthread_setspecific(Exception_stack, ((Exception_Frame*)pthread_getspecific(Exception_stack))->prev)
/** @endcond */


/**
 * Throws an exception.
 * @param e The Exception to throw
 * @param cause The cause. A NULL value is permitted, and
 *              indicates that the cause is unknown.
 * @hideinitializer
 */
#define THROW(e, cause, ...) \
        Exception_vthrow(&(e), 0, __func__, __FILE__, __LINE__, cause, ##__VA_ARGS__)


/**
 * Throws an SQLException with a database error code.
 * @param errorCode The SQL Error Code
 * @param cause The cause. A NULL value is permitted, and
 *              indicates that the cause is unknown.
 * @hideinitializer
 */
#define THROW_SQL(errorCode, cause, ...) \
        Exception_vthrow(&(SQLException), errorCode, __func__, __FILE__, __LINE__, cause, ##__VA_ARGS__)


/**
 * Re-throws an exception. In a CATCH or ELSE block clients can use RETHROW
 * to re-throw the Exception
 * @hideinitializer
 */
#define RETHROW Exception_throw(Exception_frame.exception, Exception_frame.errorCode,\
        Exception_frame.func, Exception_frame.file, Exception_frame.line, Exception_frame.message)


/**
 * Clients **must** use this macro instead of C return statements
 * inside a try-block
 * @hideinitializer
 */
#define RETURN switch((pop_exception_stack,0)) default:return


/**
 * Defines a block of code that can potentially throw an exception
 * @hideinitializer
 */
#define TRY do { \
        volatile int Exception_flag; \
        Exception_Frame Exception_frame = {}; \
        Exception_frame.message[0] = 0; \
        Exception_frame.prev = (Exception_Frame*)pthread_getspecific(Exception_stack); \
        pthread_setspecific(Exception_stack, &Exception_frame); \
        Exception_flag = sigsetjmp(Exception_frame.env, 0); \
        if (Exception_flag == Exception_entered) {
                

/**
 * Defines a block containing code for handling an exception thrown in
 * the TRY block.
 * @param e The Exception to handle
 * @hideinitializer
 */
#define CATCH(e) \
                if (Exception_flag == Exception_entered) pop_exception_stack; \
        } else if (Exception_frame.exception == &(e)) { \
                Exception_flag = Exception_handled;


/**
 * Defines a block containing code for handling any exception thrown in
 * the TRY block. An ELSE block catches any exception type not already
 * caught in a previous CATCH block.
 * @hideinitializer
 */
#define ELSE \
                if (Exception_flag == Exception_entered) pop_exception_stack; \
        } else { \
                Exception_flag = Exception_handled;


/**
 * Defines a block of code that is subsequently executed whether an
 * exception is thrown or not
 * @hideinitializer
 */
#define FINALLY \
                if (Exception_flag == Exception_entered) pop_exception_stack; \
        } { \
                if (Exception_flag == Exception_entered) \
                        Exception_flag = Exception_finalized;
                

/**
 * Ends a TRY-CATCH block
 * @hideinitializer
 */
#define END_TRY \
                if (Exception_flag == Exception_entered) pop_exception_stack; \
        } if (Exception_flag == Exception_thrown) RETHROW; \
        } while (0)


#undef T
#endif
