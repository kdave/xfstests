#
# Copyright (c) 2000-2008 Silicon Graphics, Inc.  All Rights Reserved.
#

ifeq ("$(origin V)", "command line")
  BUILD_VERBOSE = $(V)
endif
ifndef BUILD_VERBOSE
  BUILD_VERBOSE = 0
endif

ifeq ($(BUILD_VERBOSE),1)
  Q =
else
  Q = @
endif

MAKEOPTS = --no-print-directory Q=$(Q)

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
endif

TESTS = $(shell sed -n -e '/^[0-9][0-9][0-9]*/s/ .*//p' group)
CONFIGURE = configure include/builddefs include/config.h
LSRCFILES = configure configure.in aclocal.m4 README VERSION
LDIRT = config.log .ltdep .dep config.status config.cache confdefs.h \
	conftest* check.log check.time

ifeq ($(HAVE_DMAPI), true)
DMAPI_MAKEFILE = dmapi/Makefile
endif

LIB_SUBDIRS = include lib
TOOL_SUBDIRS = ltp src m4

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: include/builddefs include/config.h $(DMAPI_MAKEFILE) new remake check $(TESTS)
ifeq ($(HAVE_BUILDDEFS), no)
	$(Q)$(MAKE) $(MAKEOPTS) $@
else
	$(Q)$(MAKE) $(MAKEOPTS) $(SUBDIRS)
	# automake doesn't always support "default" target 
	# so do dmapi make explicitly with "all"
ifeq ($(HAVE_DMAPI), true)
	$(Q)$(MAKE) $(MAKEOPTS) -C $(TOPDIR)/dmapi all
endif
endif

# tool/lib dependencies
src ltp: lib

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:  # if configure hasn't run, nothing to clean
endif

configure include/builddefs:
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

install: default $(addsuffix -install,$(SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)
	$(INSTALL) -m 755 check $(PKG_LIB_DIR)
	$(INSTALL) -m 755 [0-9]?? $(PKG_LIB_DIR)
	$(INSTALL) -m 755 run.* $(PKG_LIB_DIR)
	$(INSTALL) -m 644 group $(PKG_LIB_DIR)
	$(INSTALL) -m 644 randomize.awk $(PKG_LIB_DIR)
	$(INSTALL) -m 644 [0-9]??.* $(PKG_LIB_DIR)
	$(INSTALL) -m 644 common* $(PKG_LIB_DIR)

# Nothing.
install-dev install-lib:

%-install:
	$(MAKE) $(MAKEOPTS) -C $* install

realclean distclean: clean
	rm -f $(LDIRT) $(CONFIGURE)
	rm -rf autom4te.cache Logs
