#
# Copyright (c) 2016 Red Hat Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/include/builddefs
include $(TOPDIR)/include/buildgrouplist

OVERLAY_DIR = overlay
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(OVERLAY_DIR)
DIRT = group.list

default: $(DIRT)

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(TARGET_DIR)
	$(INSTALL) -m 755 $(TESTS) $(TARGET_DIR)
	$(INSTALL) -m 644 group.list $(TARGET_DIR)
	$(INSTALL) -m 644 $(OUTFILES) $(TARGET_DIR)

# Nothing.
install-dev install-lib:
