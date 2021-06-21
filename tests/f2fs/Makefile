#
#  Copyright (c) 2015 Fujitsu Ltd.
#  Author: Xiao Yang <yangx.jy@cn.fujitsu.com>
#

TOPDIR = ../..
include $(TOPDIR)/include/builddefs
include $(TOPDIR)/include/buildgrouplist

F2FS_DIR = f2fs
TARGET_DIR = $(PKG_LIB_DIR)/$(TESTS_DIR)/$(F2FS_DIR)
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
