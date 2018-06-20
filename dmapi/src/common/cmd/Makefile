# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../../../..

include $(TOPDIR)/include/builddefs

TARGETS = read_invis set_region set_return_on_destroy write_invis

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

CFLAGS += -I.. -I/usr/include/xfs

LLDLIBS = $(TOPDIR)/dmapi/src/common/lib/libdmtest.la $(LIBDM)

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

install install-dev: default

$(TARGETS):
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

-include .dep
