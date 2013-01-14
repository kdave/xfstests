#
# Copyright (C) 2000-2008, 2011 SGI  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
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

SRCTAR = $(PKG_NAME)-$(PKG_VERSION).tar.gz

TESTS = $(shell sed -n -e '/^[0-9][0-9][0-9]*/s/ .*//p' group)
CONFIGURE = configure include/builddefs include/config.h
LSRCFILES = configure configure.ac aclocal.m4 README VERSION
LDIRT = config.log .ltdep .dep config.status config.cache confdefs.h \
	conftest* check.log check.time

ifeq ($(HAVE_BUILDDEFS), yes)
LDIRT += $(SRCTAR)
endif

LIB_SUBDIRS = include lib
TOOL_SUBDIRS = ltp src m4
ifeq ($(HAVE_DMAPI), true)
TOOL_SUBDIRS += dmapi
endif

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: include/builddefs $(DMAPI_MAKEFILE) $(TESTS)
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
	autoheader
	autoconf

include/builddefs include/config.h: configure
	./configure \
                --libexecdir=/usr/lib

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

depend: include/builddefs $(addsuffix -depend,$(SUBDIRS))

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
