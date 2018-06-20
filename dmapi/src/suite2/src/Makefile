# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../../../..

include $(TOPDIR)/include/builddefs

TARGETS = test_rights test_fileattr test_hole test_dmattr \
		test_eventlist test_region test_bulkall \
		test_bulkattr send_msg dm_test_daemon test_invis \
		invis_test region_test test_efault mmap

# things left out for some reason...
UNUSED_TARGS = check_dmapi.c mmap_cp.c mm_fill.c

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

CFLAGS += -I.. -I$(TOPDIR)/dmapi/src/common -I/usr/include/xfs

LLDLIBS = $(TOPDIR)/dmapi/src/common/lib/libdmtest.la $(LIBDM)

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

install install-dev: default

$(TARGETS):
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

-include .dep
