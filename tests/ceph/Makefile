# SPDX-License-Identifier: GPL-2.0

TOPDIR = ../..
include $(TOPDIR)/include/builddefs
include $(TOPDIR)/include/buildgrouplist

CEPH_DIR = ceph
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(CEPH_DIR)
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
