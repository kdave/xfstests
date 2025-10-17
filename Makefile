# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2000-2008, 2011 SGI  All Rights Reserved.
##
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

CHECK=sparse
CHECK_OPTS=-Wsparse-all -Wbitwise -Wno-transparent-union -Wno-return-void -Wno-undef \
	-Wno-non-pointer-null -D__CHECK_ENDIAN__ -D__linux__

ifeq ("$(origin C)", "command line")
  CHECK_CMD=$(CHECK) $(CHECK_OPTS)
  CHECKSRC=$(C)
else
  CHECK_CMD=@true
  CHECKSRC=0
endif

export CHECK_CMD CHECKSRC

MAKEOPTS = --no-print-directory Q=$(Q)

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
else
export TESTS_DIR = tests
endif

SRCTAR = $(PKG_NAME)-$(PKG_VERSION).tar.gz

CONFIGURE = configure include/config.h include/config.h.in \
	    aclocal.m4 config.guess config.sub install-sh ltmain.sh \
	    m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 \
	    m4/lt~obsolete.m4
LSRCFILES = configure configure.ac aclocal.m4 README VERSION
LDIRT = config.log .ltdep .dep config.status config.cache confdefs.h \
	conftest* check.log check.time libtool include/builddefs

ifeq ($(HAVE_BUILDDEFS), yes)
LDIRT += $(SRCTAR)
endif

LIB_SUBDIRS = include lib
TOOL_SUBDIRS = ltp src m4 common tools

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS) $(TESTS_DIR)

default: include/builddefs
ifeq ($(HAVE_BUILDDEFS), no)
	$(Q)$(MAKE) $(MAKEOPTS) $@
else
	$(Q)$(MAKE) $(MAKEOPTS) $(SUBDIRS)
endif

# tool/lib dependencies
$(TOOL_SUBDIRS): $(LIB_SUBDIRS)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:  # if configure hasn't run, nothing to clean
endif

configure: configure.ac
	libtoolize -cfi
	cp include/install-sh .
	aclocal -I m4
	autoheader
	autoconf

include/builddefs include/config.h: configure
	./configure \
                --libexecdir=/usr/lib \
                --exec_prefix=/var/lib

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

depend: include/builddefs $(addsuffix -depend,$(SUBDIRS))

install: default $(addsuffix -install,$(SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)
	$(INSTALL) -m 755 check $(PKG_LIB_DIR)
	$(INSTALL) -m 644 randomize.awk $(PKG_LIB_DIR)

# Nothing.
install-dev install-lib:

%-install:
	$(MAKE) $(MAKEOPTS) -C $* install

realclean distclean: clean
	$(Q)rm -f $(LDIRT) $(CONFIGURE)
	$(Q)rm -rf autom4te.cache Logs

dist: include/builddefs include/config.h default
ifeq ($(HAVE_BUILDDEFS), no)
	$(Q)$(MAKE) $(MAKEOPTS) -C . $@
else
	$(Q)$(MAKE) $(MAKEOPTS) $(SRCTAR)
endif

$(SRCTAR) : default
	$(Q)git archive --prefix=$(PKG_NAME)-$(PKG_VERSION)/ --format=tar \
	  v$(PKG_VERSION) > $(PKG_NAME)-$(PKG_VERSION).tar
	$(Q)$(TAR) --transform "s,^,$(PKG_NAME)-$(PKG_VERSION)/," \
	  -rf $(PKG_NAME)-$(PKG_VERSION).tar $(CONFIGURE)
	$(Q)$(ZIP) $(PKG_NAME)-$(PKG_VERSION).tar
	echo Wrote: $@
