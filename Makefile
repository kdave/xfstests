#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
endif

TESTS = $(shell sed -n -e '/^[0-9][0-9][0-9]*/s/ .*//p' group)
CONFIGURE = configure include/builddefs include/config.h
LSRCFILES = configure configure.in aclocal.m4 README VERSION
LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	check.log check.time

SUBDIRS = include lib ltp src m4

default: $(CONFIGURE) new remake check $(TESTS)
ifeq ($(HAVE_BUILDDEFS), no)
	$(MAKE) -C . $@
else
	$(SUBDIRS_MAKERULE)
endif

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:  # if configure hasn't run, nothing to clean
endif

$(CONFIGURE):
	autoheader
	autoconf
	./configure

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

install install-dev install-lib:

realclean distclean: clean
	rm -f $(LDIRT) $(CONFIGURE)
	rm -rf autom4te.cache Logs
