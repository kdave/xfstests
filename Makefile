#
# Copyright (c) 2000-2008 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
endif

TESTS = $(shell sed -n -e '/^[0-9][0-9][0-9]*/s/ .*//p' group)
CONFIGURE = configure include/builddefs include/config.h
DMAPI_MAKEFILE = dmapi/Makefile
LSRCFILES = configure configure.in aclocal.m4 README VERSION
LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	check.log check.time

LIB_SUBDIRS = include lib
TOOL_SUBDIRS = ltp src m4

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: include/builddefs include/config.h $(DMAPI_MAKEFILE) new remake check $(TESTS)
ifeq ($(HAVE_BUILDDEFS), no)
	$(MAKE) $@
else
	$(MAKE) $(SUBDIRS)
	# automake doesn't always support "default" target 
	# so do dmapi make explicitly with "all"
ifeq ($(HAVE_DMAPI), true)
	$(MAKE) -C $(TOPDIR)/dmapi all
endif
endif

# tool/lib dependencies
src ltp: lib

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:  # if configure hasn't run, nothing to clean
endif

include/builddefs:
	autoheader
	autoconf
	./configure \
                --libexecdir=/usr/lib \
                --enable-lib64=yes

include/config.h: include/builddefs
## Recover from the removal of $@
	@if test -f $@; then :; else \
		rm -f include/builddefs; \
		$(MAKE) $(AM_MAKEFLAGS) include/builddefs; \
	fi

$(DMAPI_MAKEFILE):
	cd $(TOPDIR)/dmapi/ ; ./configure

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

install install-dev install-lib:

realclean distclean: clean
	rm -f $(LDIRT) $(CONFIGURE)
	rm -rf autom4te.cache Logs
