#
# Copyright (c) 2003-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

COMMON_DIR = common

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/$(COMMON_DIR)
	$(INSTALL) -m 644 * $(PKG_LIB_DIR)/$(COMMON_DIR)

install-dev install-lib:
