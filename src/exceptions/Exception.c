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

#include "Config.h"

#include <stdio.h>
#include <string.h>

#include "Thread.h"
#include "Exception.h"


/**
 * Implementation of the Exception interface. Defines the Thread local 
 * Exception stack and Exceptions used in the library. 
 * 
 * This implementation is a minor modification of the Except code found 
 * in David R. Hanson's excellent book "C Interfaces and Implementations".
 * See http://www.cs.princeton.edu/software/cii/
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#define T Exception_T
/* Placeholder for systems exceptions. */
T SQLException = {"SQLException"};
#ifdef ZILD_PACKAGE_PROTECTED
#pragma GCC visibility push(hidden)
#endif
T AssertException = {"AssertException"};
T MemoryException = {"MemoryException"};
/* Thread specific Exception stack */
ThreadData_T Exception_stack;
#ifdef ZILD_PACKAGE_PROTECTED
#pragma GCC visibility pop
#endif
static Once_T once_control = PTHREAD_ONCE_INIT;


/* -------------------------------------------------------- Privat methods */


static void init_once(void) { ThreadData_create(Exception_stack, NULL); }


/* ----------------------------------------------------- Protected methods */


#ifdef PACKAGE_PROTECTED
#pragma GCC visibility push(hidden)
#endif

void Exception_init(void) { Thread_once(once_control, init_once); }
void Exception_reset(void) { ThreadData_set(Exception_stack, NULL); }

#ifdef PACKAGE_PROTECTED
#pragma GCC visibility pop
#endif


/* -------------------------------------------------------- Public methods */


#ifndef ZILD_PACKAGE_PROTECTED

void Exception_throw(const T *e, int errorCode, const char *func, const char *file, int line, const char *message) {
        Exception_Frame *p = ThreadData_get(Exception_stack);
        assert(e);
        if (p) {
                p->exception = e;
                p->func = func;
                p->file = file;
                p->line = line;
                p->errorCode = errorCode;
                if (message)
                        Str_copy(p->message, message, EXCEPTION_MESSAGE_LENGTH);
                pop_exception_stack;
                siglongjmp(p->env, Exception_thrown);
        } else if (message) {
                ABORT("%s: %s\n raised in %s at %s:%d\n", e->name, message,
                      func ? func : "?", file ? file : "?", line);
        } else {
                ABORT("%s: 0x%p\n raised in %s at %s:%d\n", e->name, e,
                      func ? func : "?", file ? file : "?", line);
        }
}

void Exception_vthrow(const T *e, int errorCode, const char *func, const char *file, int line, const char *cause, ...) {
        char message[EXCEPTION_MESSAGE_LENGTH + 1];
        if (cause) {
                va_list ap;
                va_start(ap, cause);
                vsnprintf(message, EXCEPTION_MESSAGE_LENGTH, cause, ap);
                va_end(ap);
                Exception_throw(e, errorCode, func, file, line, message);
        } else {
                Exception_throw(e, errorCode, func, file, line, NULL);
        }
}

#endif
