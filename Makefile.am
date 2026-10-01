# Copyright (C) Tildeslash Ltd. All rights reserved.

AUTOMAKE_OPTIONS = foreign no-dependencies subdir-objects
ACLOCAL_AMFLAGS  = -I m4 -I config

SUBDIRS         = . $(UNIT_TEST)
DIST_SUBDIRS    = . test

EXTRA_DIST      = README AUTHORS CHANGES COPYING bootstrap doc test src tools config

LIBRARY_NAME    = zdb

RE2C		= @RE2C@
RE2CFLAGS       = -i
FILTERH         = ./tools/bin/filterh

AM_CPPFLAGS     = $(CPPFLAGS) $(DBCPPFLAGS)
AM_CPPFLAGS     += -Isrc -Isrc/util -Isrc/net -Isrc/db -Isrc/db/oracle -Isrc/exceptions

pkgconfigdir 	= $(libdir)/pkgconfig
pkgconfig_DATA 	= $(LIBRARY_NAME).pc

lib_LTLIBRARIES = libzdb.la
libzdb_la_SOURCES = src/util/Str.c src/util/Vector.c src/util/StringBuffer.c \
                    src/system/Mem.c src/system/System.c src/system/Time.c \
                    src/db/ConnectionPool.c src/db/Connection.c src/db/ResultSet.c \
                    src/db/PreparedStatement.c src/db/SQLState.c \
                    src/exceptions/assert.c src/exceptions/Exception.c

if ! WITH_ZILD
libzdb_la_SOURCES += src/net/URL.c 
endif
if WITH_MYSQL
libzdb_la_SOURCES += src/db/mysql/MysqlConnection.c \
                     src/db/mysql/MysqlResultSet.c \
                     src/db/mysql/MysqlPreparedStatement.c
endif
if WITH_POSTGRESQL
libzdb_la_SOURCES += src/db/postgresql/PostgresqlConnection.c \
                     src/db/postgresql/PostgresqlResultSet.c \
                     src/db/postgresql/PostgresqlPreparedStatement.c
endif
if WITH_SQLITE
libzdb_la_SOURCES += src/db/sqlite/SQLiteConnection.c \
                     src/db/sqlite/SQLiteResultSet.c \
                     src/db/sqlite/SQLitePreparedStatement.c \
                     src/db/sqlite/SQLiteAdapter.c
endif
if WITH_ORACLE
libzdb_la_SOURCES += src/db/oracle/OracleConnection.c \
                     src/db/oracle/OracleResultSet.c \
                     src/db/oracle/OraclePreparedStatement.c
endif

API_INTERFACES  = src/zdb.h src/zdbpp.h src/db/ConnectionPool.h \
                  src/db/Connection.h src/db/ResultSet.h src/net/URL.h \
                  src/db/PreparedStatement.h src/exceptions/SQLException.h \
                  src/exceptions/Exception.h src/db/SQLState.h

nobase_nodist_include_HEADERS = $(patsubst %, $(LIBRARY_NAME)/%, $(notdir $(API_INTERFACES)))

libzdb_la_LDFLAGS = $(DBLDFLAGS) -version-info 19:0:0

BUILT_SOURCES   = $(nobase_nodist_include_HEADERS)

CLEANFILES      = $(BUILT_SOURCES)

DISTCLEANFILES  = *~ 

dist-hook::
	-rm -rf `find $(distdir) -name ".git"`
	-rm -rf `find $(distdir) -name "._*"`
	-rm -rf `find $(distdir) -name ".DS_Store"`
	-rm -rf `find $(distdir) -name ".libs"`
	-rm -f $(distdir)/src/xconfig.h $(distdir)/src/stamp-* \
              $(distdir)/tools/bin/filterh
	-rm -f $(distdir)/test/Makefile
        
clean-local::
		-rm -f `find src -name "*.o" -o -name "*.lo" -o -name "*.loT" \
                -o -name "*~" -o -name ".#*" -o -name "core*"`
                
distclean-local::
	-rm -f Makefile.in Makefile \
 		src/zdb.h \
                libzdb-[0-9].*tar.gz
	-rm -rf autom4te.cache/ \
                build/ \
                $(LIBRARY_NAME)
	-rm -f tools/bin/filterh \
           src/xconfig.h.in

verify: libzdb.la
	cd $(srcdir)/test && $(MAKE) verify	

doc: $(nobase_nodist_include_HEADERS)
	doxygen config/Doxyfile
	-cp doc/api-docs/files.html doc/api-docs/index.html

define check-exit
|| exit 1

endef

$(nobase_nodist_include_HEADERS): $(API_INTERFACES)
	$(shell test -d $(LIBRARY_NAME) || mkdir $(LIBRARY_NAME))
	$(foreach file, $(API_INTERFACES), \
                $(FILTERH) < $(file) > $(LIBRARY_NAME)/$(notdir $(file)) \
	$(check-exit))

#<< start-filter out
clean-local::
	@echo "Cleaning up Doxygen generated files..."
	@find doc/api-docs/ -type f ! -name 'database.png' \
                                    ! -name 'database++.png' \
                                    ! -name 'libzdboptions.css' \
                                    ! -name 'oracleoptions.html' \
                                    ! -name 'libzdb.css' \
                                    ! -name 'mysqloptions.html' \
                                    ! -name 'postgresoptions.html' \
                                    -exec rm -f {} +

distclean-local::
	-rm -f  src/net/URL.c \
            src/system/Time.c

cleanall: clean distclean
	-rm -f configure conf/config.* \
                $(pkgconfig_DATA) \
                aclocal.m4
	git clean -fxd -e project.xcworkspace -e oci

src/net/URL.c: src/net/URL.re
	$(RE2C) $(RE2CFLAGS) $< > $@
src/system/Time.c: src/system/Time.re
	$(RE2C) $(RE2CFLAGS) $< > $@

dist-hook::
	-rm -f $(distdir)/src/net/URL.re
	-rm -f $(distdir)/src/system/Time.re
	$(FILTERH) < Makefile.am > $(distdir)/Makefile.am
	$(FILTERH) < Makefile.in > $(distdir)/Makefile.in
#>> end-filter out


