# SPDX-License-Identifier: GPL-2.0

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

CEPH_DIR = ceph
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(CEPH_DIR)

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(TARGET_DIR)
	$(INSTALL) -m 755 $(TESTS) $(TARGET_DIR)
	$(INSTALL) -m 644 group $(TARGET_DIR)
	$(INSTALL) -m 644 $(OUTFILES) $(TARGET_DIR)

# Nothing.
install-dev install-lib:
