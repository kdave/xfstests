#
# Copyright (c) 2017 Red Hat, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

NFS_DIR = nfs
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(NFS_DIR)

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(TARGET_DIR)
	$(INSTALL) -m 755 $(TESTS) $(TARGET_DIR)
	$(INSTALL) -m 644 group $(TARGET_DIR)
	$(INSTALL) -m 644 $(OUTFILES) $(TARGET_DIR)

# Nothing.
install-dev install-lib:
