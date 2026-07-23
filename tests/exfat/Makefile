# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2026 Rochan Avlur.  All Rights Reserved.

TOPDIR = ../..
include $(TOPDIR)/include/builddefs
include $(TOPDIR)/include/buildgrouplist

EXFAT_DIR = exfat
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(EXFAT_DIR)
DIRT = group.list

default: $(DIRT)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(TARGET_DIR)
	$(INSTALL) -m 755 $(TESTS) $(TARGET_DIR)
	$(INSTALL) -m 644 group.list $(TARGET_DIR)
	$(INSTALL) -m 644 $(OUTFILES) $(TARGET_DIR)

# Nothing.
install-dev install-lib:
