#
# Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

CIFS_DIR = cifs
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(CIFS_DIR)

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(TARGET_DIR)
	$(INSTALL) -m 755 $(TESTS) $(TARGET_DIR)
	$(INSTALL) -m 644 group $(TARGET_DIR)
	$(INSTALL) -m 644 $(OUTFILES) $(TARGET_DIR)

# Nothing.
install-dev install-lib:
