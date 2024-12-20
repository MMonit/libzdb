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
 * Here's a concrete example calling a method in the libzdb API which may throw
 * an exception. If the method Connection_execute() fails it will throw an 
 * SQLException. The CATCH statement will catch this exception, if thrown, 
 * and log an error message
 * ```c
 * TRY
 *      Connection_execute(c, sql);
 * CATCH(SQLException)
 *      log("SQL error: %s\n", Connection_getLastError(c)); 
 * END_TRY;
 * ```
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
 * ## Exception details
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
 *      TRHOW(SQLException, "SQLException");
 * }
 * CATCH(SQLException)
 * {
 *      assert(i == 1); // Unless declared volatile i would be 0 here
 * }
 * END_TRY;
 * assert(i == 1); // i will be 1 here regardless if it is declared volatile or not 
 * ```
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
	jmp_buf env;
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
void Exception_vthrow(const T *e, const char *func, const char *file, int line, const char *cause, ...) CLANG_ANALYZER_NORETURN;
void Exception_throw(const T *e, const char *func, const char *file, int line, const char *message) CLANG_ANALYZER_NORETURN;

#define pop_Exception_stack pthread_setspecific(Exception_stack, ((Exception_Frame*)pthread_getspecific(Exception_stack))->prev)
/** @endcond */


/**
 * Throws an exception.
 * @param e The Exception to throw
 * @param cause The cause. A NULL value is permitted, and
 * indicates that the cause is unknown.
 * @hideinitializer
 */
#define THROW(e, cause, ...) \
        Exception_vthrow(&(e), __func__, __FILE__, __LINE__, cause, ##__VA_ARGS__, NULL)


/**
 * Re-throws an exception. In a CATCH or ELSE block clients can use RETHROW
 * to re-throw the Exception
 * @hideinitializer
 */
#define RETHROW Exception_throw(Exception_frame.exception, \
        Exception_frame.func, Exception_frame.file, Exception_frame.line, Exception_frame.message)


/**
 * Clients **must** use this macro instead of C return statements
 * inside a try-block
 * @hideinitializer
 */
#define RETURN switch((pop_Exception_stack,0)) default:return


/**
 * Defines a block of code that can potentially throw an exception
 * @hideinitializer
 */
#define TRY do { \
	volatile int Exception_flag; \
        Exception_Frame Exception_frame; \
        Exception_frame.message[0] = 0; \
        Exception_frame.prev = (Exception_Frame*)pthread_getspecific(Exception_stack); \
        pthread_setspecific(Exception_stack, &Exception_frame); \
        Exception_flag = setjmp(Exception_frame.env); \
        if (Exception_flag == Exception_entered) {
                

/**
 * Defines a block containing code for handling an exception thrown in 
 * the TRY block.
 * @param e The Exception to handle
 * @hideinitializer
 */
#define CATCH(e) \
                if (Exception_flag == Exception_entered) pop_Exception_stack; \
        } else if (Exception_frame.exception == &(e)) { \
                Exception_flag = Exception_handled; 


/**
 * Defines a block containing code for handling any exception thrown in 
 * the TRY block. An ELSE block catches any exception type not already 
 * caught in a previous CATCH block.
 * @hideinitializer
 */
#define ELSE \
                if (Exception_flag == Exception_entered) pop_Exception_stack; \
        } else { \
                Exception_flag = Exception_handled;


/**
 * Defines a block of code that is subsequently executed whether an 
 * exception is thrown or not
 * @hideinitializer
 */
#define FINALLY \
                if (Exception_flag == Exception_entered) pop_Exception_stack; \
        } { \
                if (Exception_flag == Exception_entered) \
                        Exception_flag = Exception_finalized;
                

/**
 * Ends a TRY-CATCH block
 * @hideinitializer
 */
#define END_TRY \
                if (Exception_flag == Exception_entered) pop_Exception_stack; \
        } if (Exception_flag == Exception_thrown) RETHROW; \
        } while (0)


#undef T
#endif
