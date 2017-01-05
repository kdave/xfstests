#
# Copyright (c) 2017 Oracle.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

OCFS2_DIR = ocfs2
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(OCFS2_DIR)

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(TARGET_DIR)
	$(INSTALL) -m 755 $(TESTS) $(TARGET_DIR)
	$(INSTALL) -m 644 group $(TARGET_DIR)
	$(INSTALL) -m 644 $(OUTFILES) $(TARGET_DIR)

# Nothing.
install-dev install-lib:
