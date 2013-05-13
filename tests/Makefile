#
# Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TESTS_SUBDIRS = $(sort $(dir $(wildcard $(PWD)/$(TESTS_DIR)/[a-z]*/)))

include $(BUILDRULES)

install: $(addsuffix -install,$(TESTS_SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/$(TESTS_DIR)

# Nothing.
install-dev install-lib:

%-install:
	$(MAKE) $(MAKEOPTS) -C $* install
