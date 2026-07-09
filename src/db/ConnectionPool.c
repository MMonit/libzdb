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

#include "URL.h"
#include "Thread.h"
#include "MemoryException.h"
#include "system/Time.h"
#include "Vector.h"
#include "ResultSet.h"
#include "PreparedStatement.h"
#include "Connection.h"
#include "ConnectionPool.h"


/**
 * Implementation of the ConnectionPool interface
 *
 * This implementation provides a thread-safe, self-managing connection pool.
 * - Dynamic pool size management between initial and max connections
 * - Periodic connection reaping to remove idle connections
 * - Efficient "rolling window" approach: removing old connections from the start
 *   of the pool vector, adding new ones to the end
 * - Check connection validity before serving to clients
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#define T ConnectionPool_T
struct ConnectionPool_S {
        URL_T url;
        bool filled;
        bool doSweep;
        bool reaperStarted;
        Sem_T alarm;
        Mutex_T mutex;
        Vector_T pool;
        Thread_T reaper;
        int sweepInterval;
        int maxConnections;
        volatile bool stopped;
        int connectionTimeout;
        int initialConnections;
        CONNECTIONPOOL_TYPE type;
};

int ZBDEBUG = false;
#ifdef PACKAGE_PROTECTED
#pragma GCC visibility push(hidden)
#endif
void(*AbortHandler)(const char *error) = NULL;
#ifdef PACKAGE_PROTECTED
#pragma GCC visibility pop
#endif


/* ------------------------------------------------------- Private methods */


static CONNECTIONPOOL_TYPE _getType(T P) {
        const char *databaseType = URL_getProtocol(P->url);
        if (IS(databaseType, "mysql")) {
                return CONNECTIONPOOL_MYSQL;
        } else if (IS(databaseType, "postgresql")) {
                return CONNECTIONPOOL_POSTGRESQL;
        } else if (IS(databaseType, "sqlite")) {
                return CONNECTIONPOOL_SQLITE;
        } else if (IS(databaseType, "oracle")) {
                return CONNECTIONPOOL_ORACLE;
        } else {
                return CONNECTIONPOOL_NONE;
        }
}


static void _drainPool(T P) {
        while (! Vector_isEmpty(P->pool)) {
                Connection_T con = Vector_pop(P->pool);
                Connection_free(&con);
        }
}


static bool _fillPool(T P, char error[static STRLEN]) {
        for (int i = 0; i < P->initialConnections; i++) {
                char *connectionError = NULL;
                Connection_T con = Connection_new(P, &connectionError);
                if (! con) {
                        bool rv = false;
                        if (i > 0) {
                                DEBUG("Pool partially filled with initial connections -- %s\n", STR_DEF(connectionError) ? connectionError : "unknown error");
                                rv = true;
                        } else {
                                snprintf(error, STRLEN, "%s", STR_DEF(connectionError) ? connectionError : "unknown error");
                        }
                        FREE(connectionError);
                        return rv;
                }
                Vector_push(P->pool, con);
        }
        return true;
}


static void _mapActive(const void *con, void *ap) {
        int *active = ap;
        if (!Connection_isAvailable((Connection_T)con)) *active += 1;
}


static int _active(T P) {
        int active = 0;
        Vector_map(P->pool, _mapActive, &active);
        return active;
}


static inline Connection_T _getAvailableConnection(T P) {
        Connection_T con = NULL;
        int size = Vector_size(P->pool);
        for (int i = 0; i < size; i++) {
                con = Vector_get(P->pool, i);
                if (Connection_isAvailable(con)) {
                        Connection_setAvailable(con, false);
                        return con;
                }
        }
        return NULL;
}


static inline Connection_T _createConnection(T P, char error[static STRLEN]) {
        char *connectionError = NULL;
        Connection_T con = Connection_new(P, &connectionError);
        Mutex_lock(P->mutex);
        TRY
        {
                if (con) {
                        Connection_setAvailable(con, false);
                        Vector_push(P->pool, con);
                }
        }
        CATCH(MemoryException)
        {
                Connection_free(&con);
        }
        FINALLY
        {
                Mutex_unlock(P->mutex);
        }
        END_TRY;
        if (! con) {
                snprintf(error, STRLEN, "Failed to create a connection -- %s", STR_DEF(connectionError) ? connectionError : "unknown error");
                FREE(connectionError);
        }
        return con;
}


static Connection_T _getConnection(T P, char error[static STRLEN]) {
        Connection_T con = NULL;
        int size = 0;
        int activeConnections = 0;
        int availableConnections = 0;
        *error = 0;
        LOCK(P->mutex)
        {
                size = Vector_size(P->pool);
                activeConnections = _active(P);
                availableConnections = size - activeConnections;
        }
        END_LOCK;
        while (availableConnections > 0) {
                LOCK(P->mutex)
                {
                        con = _getAvailableConnection(P);
                }
                END_LOCK;
                if (!con) {
                        break;
                }
                if (Connection_ping(con)) {
                        return con;
                } else {
                        LOCK(P->mutex)
                        {
                                Vector_remove(P->pool, Vector_indexOf(P->pool, con));
                                size = Vector_size(P->pool);
                                availableConnections--;
                        }
                        END_LOCK;
                        Connection_free(&con);
                }
        }
        // If we're here, either all available connections failed or there were none
        // Try to create a new connection if the pool isn't full
        // Note: 'size' might not reflect the current pool size due to concurrent modifications.
        // We risk potential temporary over-allocation to prioritize getting a creation error
        // if the database is down. 
        if (size < P->maxConnections) {
                con = _createConnection(P, error);
                if (con) return con;
        } else {
                snprintf(error, STRLEN, "Failed to get a connection -- pool is full (max connections reached)");
        }
        DEBUG("%s\n", error);
        return NULL;
}


static int _reapConnections(T P) {
        int n = 0;
        int x = Vector_size(P->pool) - _active(P) - P->initialConnections;
        time_t timedout = Time_now() - P->connectionTimeout;
        // We don't always examine all idle connections in a single run,
        // but over multiple runs this should cycles through all connections.
        // We only remove connections that have exceeded its timeout.
        // Connection_ping() is not performed here to avoid blocking the pool
        // with network I/O while holding the lock. Dead connections are detected
        // and removed by _getConnection() before being returned to clients.
        for (int i = 0; ((n < x) && (i < Vector_size(P->pool))); i++) {
                Connection_T con = Vector_get(P->pool, i);
                if (Connection_isAvailable(con)) {
                        if ((Connection_getLastAccessedTime(con) < timedout)) {
                                Vector_remove(P->pool, i);
                                Connection_free(&con);
                                n++;
                                i--;
                        }
                }
        }
        return n;
}


static void *_doSweep(void *args) {
        T P = args;
        struct timespec wait = {};
        Mutex_lock(P->mutex);
        while (! P->stopped) {
                wait.tv_sec = Time_now() + P->sweepInterval;
                Sem_timeWait(P->alarm,  P->mutex, wait);
                if (P->stopped) break;
                _reapConnections(P);
        }
        Mutex_unlock(P->mutex);
        DEBUG("Reaper thread stopped\n");
        return NULL;
}


/* ---------------------------------------------------------------- Public */


T ConnectionPool_new(URL_T url) {
        T P;
        assert(url);
        System_init();
        NEW(P);
        P->url = url;
        Sem_init(P->alarm);
        Mutex_init(P->mutex);
        P->doSweep = true;
        P->type = _getType(P);
        P->sweepInterval = SQL_DEFAULT_SWEEP_INTERVAL;
        P->maxConnections = SQL_DEFAULT_MAX_CONNECTIONS;
        P->pool = Vector_new(SQL_DEFAULT_MAX_CONNECTIONS);
        P->initialConnections = SQL_DEFAULT_INIT_CONNECTIONS;
        P->connectionTimeout = SQL_DEFAULT_CONNECTION_TIMEOUT;
        return P;
}


void ConnectionPool_free(T *P) {
        Vector_T pool;
        assert(P && *P);
        pool = (*P)->pool;
        if (! (*P)->stopped)
                ConnectionPool_stop((*P));
        Vector_free(&pool);
        Mutex_destroy((*P)->mutex);
        Sem_destroy((*P)->alarm);
        FREE(*P);
}


/* ------------------------------------------------------------ Properties */


CONNECTIONPOOL_TYPE ConnectionPool_getType(T P) {
        assert(P);
        return P->type;
}


URL_T ConnectionPool_getURL(T P) {
        assert(P);
        return P->url;
}


void ConnectionPool_setInitialConnections(T P, int initialConnections) {
        assert(P);
        assert(initialConnections >= 0);
        LOCK(P->mutex)
        {
                P->initialConnections = initialConnections;
        }
        END_LOCK;
}


int ConnectionPool_getInitialConnections(T P) {
        assert(P);
        return P->initialConnections;
}


void ConnectionPool_setMaxConnections(T P, int maxConnections) {
        assert(P);
        assert(P->initialConnections <= maxConnections);
        LOCK(P->mutex)
        {
                P->maxConnections = maxConnections;
        }
        END_LOCK;
}


int ConnectionPool_getMaxConnections(T P) {
        assert(P);
        return P->maxConnections;
}


void ConnectionPool_setConnectionTimeout(T P, int connectionTimeout) {
        assert(P);
        assert(connectionTimeout > 0);
        P->connectionTimeout = connectionTimeout;
}


int ConnectionPool_getConnectionTimeout(T P) {
        assert(P);
        return P->connectionTimeout;
}


void ConnectionPool_setAbortHandler(T P, void(*abortHandler)(const char *error)) {
        assert(P);
        AbortHandler = abortHandler;
}


void ConnectionPool_setReaper(T P, int sweepInterval) {
        assert(P);
        LOCK(P->mutex)
        {
                if (sweepInterval > 0) {
                        P->doSweep = true;
                        P->sweepInterval = sweepInterval;
                } else {
                        P->doSweep = false;
                }
        }
        END_LOCK;
}


/* -------------------------------------------------------- Public methods */


void ConnectionPool_start(T P) {
        assert(P);
        bool filled = false;
        char error[STRLEN] = {};
        Mutex_lock(P->mutex);
        TRY
        {
                P->stopped = false;
                if (! P->filled) {
                        P->filled = _fillPool(P, error);
                        if (P->filled) {
                                if (P->doSweep) {
                                        DEBUG("Starting Database reaper thread\n");
                                        Thread_create(P->reaper, _doSweep, P);
                                        P->reaperStarted = true;
                                }
                        }
                }
                filled = P->filled;
        }
        FINALLY
        {
                Mutex_unlock(P->mutex);
        }
        END_TRY;
        if (! filled)
                THROW(SQLException, "Failed to start connection pool -- %s", error);
}


void ConnectionPool_stop(T P) {
        bool stopSweep = false;
        assert(P);
        LOCK(P->mutex)
        {
                P->stopped = true;
                if (P->filled) {
                        _drainPool(P);
                        P->filled = false;
                }
                stopSweep = P->reaperStarted;
                P->reaperStarted = false;
        }
        END_LOCK;
        if (stopSweep) {
                DEBUG("Stopping Database reaper thread...\n");
                Sem_signal(P->alarm);
                Thread_join(P->reaper);
        }
}


Connection_T ConnectionPool_getConnection(T P) {
        assert(P);
        return _getConnection(P, (char[STRLEN]){});
}


Connection_T ConnectionPool_getConnectionOrException(T P) {
        assert(P);
        char error[STRLEN] = {};
        Connection_T con = _getConnection(P, error);
        if (!con) {
                THROW(SQLException, "%s", error);
        }
        return con;
}


void ConnectionPool_returnConnection(T P, Connection_T connection) {
        assert(P);
        assert(connection);
        if (Connection_inTransaction(connection)) {
                TRY
                        Connection_rollback(connection);
                ELSE
                        DEBUG("Failed to rollback transaction -- %s\n", Exception_frame.message);
                END_TRY;
        }
        Connection_clear(connection);
        LOCK(P->mutex)
        {
                Connection_setAvailable(connection, true);
        }
        END_LOCK;
}


int ConnectionPool_reapConnections(T P) {
        int n = 0;
        assert(P);
        LOCK(P->mutex)
        {
                n = _reapConnections(P);
        }
        END_LOCK;
        return n;
}


int ConnectionPool_size(T P) {
        assert(P);
        int size = 0;
        LOCK(P->mutex)
        {
                size = Vector_size(P->pool);
        }
        END_LOCK;
        return size;
}


int ConnectionPool_active(T P) {
        assert(P);
        int n = 0;
        LOCK(P->mutex)
        {
                n = _active(P);
        }
        END_LOCK;
        return n;
}


bool ConnectionPool_isFull(T P) {
        assert(P);
        bool full = false;
        LOCK(P->mutex)
        {
                full = (_active(P) >= P->maxConnections);
        }
        END_LOCK;
        return full;
}


/* --------------------------------------------------------- Class methods */


const char *ConnectionPool_version(void) {
        return ABOUT;
}
