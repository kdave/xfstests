#
# Copyright (c) 2025 Red Hat Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TOOLS_DIR = tools

include $(BUILDRULES)

default:

install: default
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/$(TOOLS_DIR)

install-dev install-lib:
