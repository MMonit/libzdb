# Copyright (C) Tildeslash Ltd. All rights reserved.

AC_PREREQ([2.53])

AC_INIT([libzdb], [3.7.0], [bugs-libzdb@tildeslash.com])
AC_SUBST([VERSION_MAJOR], [`echo $PACKAGE_VERSION | cut -d. -f1`])
AC_SUBST([VERSION_MINOR], [`echo $PACKAGE_VERSION | cut -d. -f2`])
AC_SUBST([VERSION_REVISION], [`echo $PACKAGE_VERSION | cut -d. -f3`])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/Config.h])
AC_CONFIG_COMMANDS([libtool_patch],[test `uname` = "OpenBSD" && perl -p -i -e "s/deplibs_check_method=.*/deplibs_check_method=pass_all/g" libtool])

# ------------------------------------------------------------------------
# Architecture/OS
# ------------------------------------------------------------------------

case `uname` in
Linux*)
	AC_DEFINE([LINUX], 1, [Define to 1 if the system is Linux])
	;;
FreeBSD*)
	AC_DEFINE([FREEBSD], 1, [Define to 1 if the system is FreeBSD])
	;;
OpenBSD*)
	AC_DEFINE([OPENBSD], 1, [Define to 1 if the system is OpenBSD])
	;;
Darwin*)
	AC_DEFINE([DARWIN], 1, [Define to 1 if the system is OSX])
	LDFLAGS="$LDFLAGS -Wl,-search_paths_first"
    ondarwin=1
	;;
SunOS*)
	AC_DEFINE([SOLARIS], 1, [Define to 1 if the system is SOLARIS])
	;;
NetBSD*)
	AC_DEFINE([NETBSD], 1, [Define to 1 if the system is NETBSD])
	;;
AIX*)
	AC_DEFINE([AIX], 1, [Define to 1 if the system is AIX])
	;;
esac

# ---------------------------------------------------------------------------
# Compiler
# ---------------------------------------------------------------------------

AC_PROG_CC
AC_PROG_CXX
AC_C_CONST
AC_C_BIGENDIAN

AS_IF([test "`uname`" != "Darwin"],
  [
        # Not Darwin, so use _GNU_SOURCE
        CFLAGS="$CFLAGS -D_GNU_SOURCE"
  ],
  [
        # On Darwin, use _DARWIN_C_SOURCE and _POSIX_C_SOURCE
        CFLAGS="$CFLAGS -D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=199506L"
  ]
)

# Compiler; tune warnings
CFLAGS="$CFLAGS -Wall -Wunused";
# Allow intermix unsigned char and char
CFLAGS="$CFLAGS -Wno-pointer-sign";

# enable fortification level 2
CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"

# Require C11 and use the latest available (C23 > C17 > C11)
AC_COMPUTE_INT([STDC_VERSION],[__STDC_VERSION__],,[STDC_VERSION=0])
AS_IF([test "$STDC_VERSION" -ge 202311],
    [CFLAGS="$CFLAGS -std=c23"],
    [AS_IF([test "$STDC_VERSION" -ge 201710],
        [CFLAGS="$CFLAGS -std=c17"],
        [AS_IF([test "$STDC_VERSION" -ge 201112],
            [CFLAGS="$CFLAGS -std=c11"],
            [AC_MSG_ERROR([Your compiler does not support C11 or later])]
        )]
    )]
)

# Require C++20 or later to build C++ test code and use zdbpp.h
AC_LANG_PUSH([C++])
AC_CACHE_CHECK([whether the C++ compiler supports C++20],
    [ac_cv_cxx_compile_cxx20],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
        #include <version>
        #if __cplusplus < 202002L
        #error C++20 is not supported
        #endif
    ]])],
    [AC_MSG_ERROR([Your compiler does not support C++20 or later])],
    [ac_cv_cxx_compile_cxx20=yes])])
CXXFLAGS="$CXXFLAGS -std=c++20"
AC_LANG_POP([C++])

# Check for C99 headers
AC_CHECK_HEADERS([stdint.h stdbool.h], [], [AC_MSG_ERROR([toolchain does not have C99 headers])])

# Check for C11 atomic operations support
AC_CHECK_HEADERS([stdatomic.h], [], [AC_MSG_ERROR([toolchain does not support C11 atomic operations])])

# ------------------------------------------------------------------------
# AddressSanitizer
# ------------------------------------------------------------------------

AC_MSG_CHECKING([for AddressSanitizer support])
AC_ARG_WITH(asan,
    [  --with-asan             enable the use of AddressSanitizer (default: disabled)],
    [
        dnl Check the withvalue
        if test "x$withval" = "xno" ; then
            asan=false
            AC_MSG_RESULT([disabled])
        fi
        if test "x$withval" = "xyes" ; then
            asan=true
            AC_MSG_RESULT([enabled])
        fi
    ],
    [
        asan=false
        AC_MSG_RESULT([disabled])
    ]
)

if test "$asan" = "true"; then
        AX_CHECK_COMPILE_FLAG([-fsanitize=address], [CFLAGS="$CFLAGS -fsanitize=address"])
        AX_CHECK_COMPILE_FLAG([-fno-omit-frame-pointer], [CFLAGS="$CFLAGS -fno-omit-frame-pointer"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=bool], [CFLAGS="$CFLAGS -fsanitize=bool"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=enum], [CFLAGS="$CFLAGS -fsanitize=enum"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=float-divide-by-zero], [CFLAGS="$CFLAGS -fsanitize=float-divide-by-zero"])
        AX_CHECK_COMPILE_FLAG([-fsanitize-address-use-after-scope], [CFLAGS="$CFLAGS -fsanitize-address-use-after-scope"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=signed-integer-overflow], [CFLAGS="$CFLAGS -fsanitize=signed-integer-overflow"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=vla-bound], [CFLAGS="$CFLAGS -fsanitize=vla-bound"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=bounds-strict], [CFLAGS="$CFLAGS -fsanitize=bounds-strict"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=null], [CFLAGS="$CFLAGS -fsanitize=null "])
        AX_CHECK_COMPILE_FLAG([-fsanitize=object-size], [CFLAGS="$CFLAGS -fsanitize=object-size"])
        AX_CHECK_COMPILE_FLAG([-fsanitize=undefined], [CFLAGS="$CFLAGS -fsanitize=undefined"])
        AC_DEFINE([HAVE_ASAN], 1, [Define to 1 if you have address sanitizer.])
else
        # enable fortification level 2
        CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
fi

# ---------------------------------------------------------------------------
# Programs
# ---------------------------------------------------------------------------

AC_PATH_PROG([RE2C], [re2c], [no], [$PATH:/usr/local/bin:/usr/bin])
if test "x$RE2C" = "xno"; then
        # Require re2c unless URL.c and Time.c already are built
        if test ! -f src/net/URL.c -a ! -f src/system/Time.c; then
                AC_MSG_ERROR([Libzdb require re2c. Download re2c from http://re2c.org/ or use your package manager])
        fi
else
        # Check re2c version (require >= 3.0)
        AC_MSG_CHECKING([re2c version])
        RE2C_VERSION=$($RE2C --version | sed 's/.*\([[0-9]][[0-9]]*\.[[0-9]][[0-9]]*\).*/\1/')
        AC_MSG_RESULT([$RE2C_VERSION])
        
        RE2C_MAJOR=$(echo $RE2C_VERSION | cut -d. -f1)
        
        if test "$RE2C_MAJOR" -lt 3; then
                AC_MSG_ERROR([re2c version >= 3.0 required (found $RE2C_VERSION)])
        fi
fi
AC_PROG_LEX([noyywrap])
if test -z "${LEXLIB+set}"; then
        # Require flex unless lex.yy.c already is built
        if test ! -f ./tools/filterh/lex.yy.c; then
                AC_MSG_ERROR([flex is required. Download from https://www.gnu.org/software/flex/ or use your package manager])
        fi
fi

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------

if test ! -d ./tools/bin/
then
        mkdir ./tools/bin || AC_MSG_ERROR([Failed to create ./tools/bin])
fi
# Build tools in-place in a subshell
if test ! -f ./tools/filterh/lex.yy.c; then
        (cd ./tools/filterh && $LEX filterh.l && $CC lex.yy.c -o ../bin/filterh  && rm -f lex.yy.o) || AC_MSG_ERROR([Failed to build tools])
else
        (cd ./tools/filterh && $CC lex.yy.c -o ../bin/filterh && rm -f lex.yy.o) || AC_MSG_ERROR([Failed to build tools])
fi
# Assert that we succeded building filterh
test -f ./tools/bin/filterh || AC_MSG_ERROR([Failed to build tools])

# ---------------------------------------------------------------------------
# Libtool
# ---------------------------------------------------------------------------

LT_INIT

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------

DBLDFLAGS="-lm "

AC_ARG_ENABLE(optimized,
        AS_HELP_STRING([--enable-optimized],
                [Build software optimized]),
    [
        if test "x$enableval" = "xyes" ; then
                CFLAGS=`echo $CFLAGS|sed 's/\-g[[^ ]]*//g'`
                CFLAGS=`echo $CFLAGS|sed 's/\-O.//'`
                CFLAGS="$CFLAGS -O3"
                OPTIMIZED=1
        else
                OPTIMIZED=0
        fi
    ],
    [
        OPTIMIZED=0
    ]
)

AC_ARG_ENABLE(profiling,
        AS_HELP_STRING([--enable-profiling],
                [Build with debug and profiling options]),
    [
        if test "x$enableval" = "xyes" ; then
            AC_MSG_CHECKING([whether the compiler supports profiling options])
            svd_CFLAGS="$CFLAGS"
            CFLAGS="-pg $CFLAGS"
            AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return 0;])], [
                AC_MSG_RESULT([yes])
                CFLAGS=`echo $CFLAGS|sed 's/-O.//'`
                CFLAGS="$CFLAGS -g"
                PROFILE=1
            ], [
                AC_MSG_RESULT([no])
                CFLAGS="$svd_CFLAGS"
                PROFILE=0
            ])
        else
                PROFILE=0
        fi
    ],
    [
        PROFILE=0
    ]
)

AC_ARG_ENABLE([zild],
        AS_HELP_STRING([--enable-zild],
                [Reduce visibility of objects for linking with the zild app server.
                Require gcc version >= 4.x or clang. This is an internal Tildeslash
                build option and should NOT be used by a third-party]),
     [
        if test "x$enableval" = "xyes" ; then
                ZILD_PROTECT=1
                AC_DEFINE([PACKAGE_PROTECTED], 1, [Define to 1 to package protect (hide) non-api objects])
                AC_DEFINE([ZILD_PACKAGE_PROTECTED], 1, [Define to 1 to hide objects for linking with zild])
        else
                ZILD_PROTECT=0
        fi
     ],[ZILD_PROTECT=0]
)
AM_CONDITIONAL([WITH_ZILD], test $ZILD_PROTECT -eq 1)

AC_ARG_ENABLE([protected],
        AS_HELP_STRING([--enable-protected],
        [Package protect non-API objects. Require gcc version >= 4.x or clang.
         This option hide objects in the DSO which are not part of the API and
         not to be exported. The rationale is to optimize the ABI and protect
         non-public methods of the DSO and thereby reduce the potential for
         namespace conflicts for clients linking with the library. Recommend]),
     [
        if test "x$enableval" = "xyes" ; then
                PROTECT=1
                AC_DEFINE([PACKAGE_PROTECTED], 1, [Define to 1 to package protect (hide) non-api objects])
        else
             PROTECT=0
        fi
     ],[PROTECT=0]
)

AC_ARG_ENABLE([openssl],
        AS_HELP_STRING([--enable-openssl(=<path>)],
                [Link libzdb with openssl. If database libraries were linked static,
                libzdb may have to link with openssl to support crypto and ssl
                functionality in SQL client libraries. An optional path argument may
                be given to specify the top-level directory to search for openssl to
                link with]),
     [
        if test "x$enableval" = "xno" ; then
                OPENSSL=0
        else
                OPENSSL=1
                if test "x$enableval" = "xyes"; then
                        AC_CHECK_LIB([ssl], [SSL_CTX_new], [], [AC_MSG_ERROR([libssl not found])])
                        AC_CHECK_LIB([crypto], [SHA1_Init], [], [AC_MSG_ERROR([libcrypto not found])])
                else
                        AC_MSG_CHECKING([for openssl in $enableval])
                        LDFLAGS="-L$enableval/lib -lssl -lcrypto $LDFLAGS"
                        CPPFLAGS="-I$enableval/include $CPPFLAGS"
                        if test -r "$enableval/lib/libssl.a" -a -r "$enableval/lib/libcrypto.a"; then
                                AC_MSG_RESULT([ok])
                        else
                                AC_MSG_ERROR([openssl not found in $enableval])
                        fi
                fi
        fi
     ],[OPENSSL=0]
)

AC_ARG_ENABLE(sqliteunlock,
        AS_HELP_STRING([--enable-sqliteunlock],
                [Enable the SQLite unlock notification API (requires SQLite >= 3.6.12
                compiled with the SQLITE_ENABLE_UNLOCK_NOTIFY C-preprocessor symbol).
                Using this option will greatly improve SQLite concurrency when libzdb
                and SQLite are used from a multi-threaded program.]),
    [
        if test "x$enableval" = "xyes" ; then
                SQLITEUNLOCK=1
                CFLAGS="$CFLAGS -DSQLITEUNLOCK"
        else
                SQLITEUNLOCK=0
        fi
    ],
    [
        SQLITEUNLOCK=0
    ]
)

AC_ARG_ENABLE([sqlite-shared-cache],
    AS_HELP_STRING([--enable-sqlite-shared-cache],
        [Always enable shared-cache mode for SQLite connections. This overrides the
        URL parameter 'shared-cache' and forces shared-cache on for all connections.
        CAUTION: This option is intended for specific use cases and should not be
        enabled by default in general-purpose builds or distributions. Most users
        should use the URL property shared-cache=true instead.]),
    [
        if test "x$enableval" = "xyes" ; then
            AC_DEFINE([ENABLE_SQLITE_SHARED_CACHE], 1, [Define to 1 to always enable SQLite shared-cache mode])
        fi
    ],[]
)

if test $PROTECT -eq 0 -a $ZILD_PROTECT -eq 0; then
        test_build=1
        UNIT_TEST="test"
else
        test_build=0
        UNIT_TEST=""
fi
AC_SUBST(UNIT_TEST)

# ------------------------------------------------------------------------
# Functions
# ------------------------------------------------------------------------

# Require a working sigsetjmp
AC_CHECK_DECLS([sigsetjmp], [], [AC_MSG_FAILURE([sigsetjmp is required])], [[#include <setjmp.h>]])

# Require that we have vsnprintf that conforms to c11. I.e. does bounds check
AC_CACHE_CHECK([vsnprintf is c11 conformant],
               [libzdb_cv_vsnprintf_c11_conformant],
               [AC_RUN_IFELSE([AC_LANG_PROGRAM(
                        [[#include <stdarg.h>
                          #include <stdio.h>]],
                        [[char t[1]; va_list ap; int n = vsnprintf(t, 1, "hello", ap); if(n == 5) return 0;return 1;]])],
                        [AC_MSG_RESULT(yes)],
                        [AC_MSG_FAILURE([vsnprintf does not conform to c11])],
                        [AC_MSG_ERROR(cross-compiling: please set 'libzdb_cv_vsnprintf_c11_conformant=[yes|no]')])])

AC_CHECK_FUNCS([timegm])

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------

AC_SEARCH_LIBS([pthread_create], [pthread], [], [AC_MSG_ERROR([POSIX thread library is required])])

# Database Libraries

postgresql="yes"
check_postgres_config()
{
        AC_PATH_PROG([PGCONFIG], [pg_config], [no], [$PATH:/usr/local/bin:/usr/local/pgsql/bin])
        if test "x$PGCONFIG" = "xno"
        then
                AC_MSG_WARN([pg_config is required to build libzdb with postgresql])
                postgresql="no"
        fi
}
AC_MSG_CHECKING(for postgresql)
AC_ARG_WITH([postgresql],
        AS_HELP_STRING([--with-postgresql(=<path>)],
                [Path is optional and if given should specify the full path to the PostgreSQL
                 configure script, pg_config. E.g. --with-postgresql=/<path>/pg_config]),
        [
                if test "xno" = "x$with_postgresql"; then
                        AC_MSG_RESULT([no])
                        postgresql="no"
                else
                        AC_MSG_RESULT([yes])
                        AC_CHECK_FILE([$with_postgresql], [PGCONFIG=$with_postgresql],[check_postgres_config])
                fi
        ],
        [
                AC_MSG_RESULT([yes])
                check_postgres_config
        ])
if test "xyes" = "x$postgresql"; then
        svd_CPPFLAGS=$CPPFLAGS
        svd_LDFLAGS=$LDFLAGS
        CPPFLAGS="-I`$PGCONFIG --includedir` $CPPFLAGS"
        LDFLAGS="-L`$PGCONFIG --libdir` $LDFLAGS"
        AC_CHECK_HEADERS([libpq-fe.h], [], [postgresql="no"])
        if test "xyes" = "x$postgresql"; then
                DBCPPFLAGS="$DBCPPFLAGS -I`$PGCONFIG --includedir`"
                DBLDFLAGS="$DBLDFLAGS -L`$PGCONFIG --libdir` -lpq"
                AC_DEFINE([HAVE_LIBPQ], 1, [Define to 1 to enable postgresql])
        else
                CPPFLAGS=$svd_CPPFLAGS
                LDFLAGS=$svd_LDFLAGS
        fi
fi
AM_CONDITIONAL([WITH_POSTGRESQL], test "xyes" = "x$postgresql")


sqlite="yes"
AC_MSG_CHECKING(for SQLite3)
AC_ARG_WITH([sqlite],
        AS_HELP_STRING([--with-sqlite=<path>],
                [Path is optional and if given should specify the full path to the SQLite installation.
                 E.g. /usr/local/sqlite3]),
        [
                if test "xno" = "x$with_sqlite"; then
                        AC_MSG_RESULT([no])
                        sqlite="no"
                else
                        AC_MSG_RESULT([yes])
                        AC_CHECK_FILE([$with_sqlite],
                        [
                                svd_LDFLAGS=$LDFLAGS
                                svd_CPPFLAGS=$CPPFLAGS
                                LDFLAGS="-L$with_sqlite/lib $LDFLAGS"
                                CPPFLAGS="-I$with_sqlite/include $CPPFLAGS"
                                AC_SEARCH_LIBS([sqlite3_open], [sqlite3],
                                [
                                        DBCPPFLAGS="$DBCPPFLAGS -I$with_sqlite/include"
                                        DBLDFLAGS="$DBLDFLAGS -L$with_sqlite/lib/ -lsqlite3"
                                ],[sqlite="no"],[-ldl -lm])
                                LDFLAGS=$svd_LDFLAGS
                                CPPFLAGS=$svd_CPPFLAGS

                        ],
                        AC_SEARCH_LIBS([sqlite3_open], [sqlite3], [], [sqlite="no"], [-ldl -lm]))
                fi
        ],
        [
                AC_MSG_RESULT([yes])
                AC_SEARCH_LIBS([sqlite3_open], [sqlite3], [], [sqlite="no"])
        ])
if test "xyes" = "x$sqlite"; then
        AC_DEFINE([HAVE_LIBSQLITE3], 1, [Define to 1 to enable sqlite3])
        AC_SEARCH_LIBS([sqlite3_errstr], [sqlite3], [AC_DEFINE([HAVE_SQLITE3_ERRSTR], [1], [sqlite3_errstr])], [], [-ldl -lm])
fi
AM_CONDITIONAL([WITH_SQLITE], test "xyes" = "x$sqlite")

mysql="yes"
check_mysql_config()
{
        AC_PATH_PROG([MYSQLCONFIG], [mysql_config], [no], [$PATH:/usr/local/bin:/usr/local/mysql/bin])
        if test "x$MYSQLCONFIG" = "xno"
        then
                AC_MSG_WARN([mysql_config is required to build libzdb with mysql])
                mysql="no"
        fi
}
AC_MSG_CHECKING(for mysql)
AC_ARG_WITH([mysql],
        AS_HELP_STRING([--with-mysql(=<path>)],
                [Path is optional and if given should specify the full path to the MySQL
                 configure script, mysql_config. E.g. --with-mysql=/<path>/mysql_config]),
        [
                if test "xno" = "x$with_mysql"; then
                        AC_MSG_RESULT([no])
                        mysql="no"
                else
                        AC_MSG_RESULT([yes])
                        AC_CHECK_FILE([$with_mysql], [MYSQLCONFIG=$with_mysql], [check_mysql_config])
                fi
        ],
        [
                AC_MSG_RESULT([yes])
                check_mysql_config
        ])
if test "xyes" = "x$mysql"; then
        svd_CPPFLAGS=$CPPFLAGS
        svd_LDFLAGS=$LDFLAGS
        CPPFLAGS="`$MYSQLCONFIG --include` $CPPFLAGS"
        LDFLAGS="`$MYSQLCONFIG --libs` $LDFLAGS"
        AC_CHECK_HEADERS([mysql.h], [], [mysql="no"])
        if test "xyes" = "x$mysql"; then
                DBCPPFLAGS="$DBCPPFLAGS `$MYSQLCONFIG --include`"
                DBLDFLAGS="$DBLDFLAGS `$MYSQLCONFIG --libs`"
                AC_DEFINE([HAVE_LIBMYSQLCLIENT], 1, [Define to 1 to enable mysql])
        else
                CPPFLAGS=$svd_CPPFLAGS
                LDFLAGS=$svd_LDFLAGS
        fi
fi
AM_CONDITIONAL([WITH_MYSQL], test "xyes" = "x$mysql")

oracle="yes"
AC_MSG_CHECKING(for oracle)
AX_LIB_ORACLE_OCI
if test -n "$ORACLE_OCI_CFLAGS" -a -n "$ORACLE_OCI_LDFLAGS"; then
        DBCPPFLAGS="$DBCPPFLAGS $ORACLE_OCI_CFLAGS"
        DBLDFLAGS="$DBLDFLAGS $ORACLE_OCI_LDFLAGS"
        AC_DEFINE([HAVE_ORACLE], 1, [Define to 1 to enable oracle])
else
        oracle="no"
fi
AM_CONDITIONAL([WITH_ORACLE], test "xyes" = "x$oracle")

# Test if any database system was found
if test "xno" = "x$postgresql" -a "xno" = "x$mysql" -a "xno" = "x$sqlite" -a "xno" = "x$oracle"; then
        AC_MSG_ERROR([No available database found or selected. Try configure --help])
fi

AC_SUBST(DBLDFLAGS)
AC_SUBST(DBCPPFLAGS)

# ---------------------------------------------------------------------------
# Data Types
# ---------------------------------------------------------------------------

AC_CHECK_TYPES([uchar_t])
AC_CHECK_MEMBERS([struct tm.tm_gmtoff], [], [], [[#include <time.h>]])

# ---------------------------------------------------------------------------
# Outputs
# ---------------------------------------------------------------------------

AC_CONFIG_HEADERS(src/xconfig.h)
AC_CONFIG_FILES([
        src/zdb.h
        Makefile
        test/Makefile
        zdb.pc
])
AC_OUTPUT

AX_INFO_GPL()
AX_INFO_TITLE([Libzdb is configured as follows])
AX_INFO_ENABLED([Optimized:],        [test $OPTIMIZED -eq 1])
AX_INFO_ENABLED([Protected:],        [test $PROTECT -eq 1 -o $ZILD_PROTECT -eq 1])
AX_INFO_ENABLED([Profiling:],        [test $PROFILE -eq 1])
AX_INFO_ENABLED([AddressSanitizer:], [test "xfalse" != "x$asan"])
AX_INFO_ENABLED([Zild:],             [test $ZILD_PROTECT -eq 1])
AX_INFO_ENABLED([Sqlite3 unlock:],   [test $SQLITEUNLOCK -eq 1])
AX_INFO_ENABLED([Openssl:],          [test $OPENSSL -eq 1])
AX_INFO_ENABLED([Unit Tests Build:], [test $test_build -eq 1])
AX_INFO_SEPARATOR()
AX_INFO_ENABLED([SQLite3:],          [test \"x$sqlite\" = \"xyes\"])
AX_INFO_ENABLED([MySQL:],            [test \"x$mysql\" = \"xyes\"])
AX_INFO_ENABLED([PostgreSQL:],       [test \"x$postgresql\" = \"xyes\"])
AX_INFO_ENABLED([Oracle:],           [test \"x$oracle\" = \"xyes\"])
AX_INFO_BREAK()
