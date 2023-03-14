#
# Copyright (c) 2023 Rodrigo Campos Catelin (Microsoft). All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/include/builddefs
include $(TOPDIR)/include/buildgrouplist

TMPFS_DIR = tmpfs
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(TMPFS_DIR)
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
