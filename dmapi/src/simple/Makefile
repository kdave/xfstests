# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../../..

include $(TOPDIR)/include/builddefs

TARGETS = dm_create_session dm_destroy_session dm_find_eventmsg \
		dm_getall_sessions dm_getall_tokens dm_query_session

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

CFLAGS += -I$(TOPDIR)/dmapi/src/common -I/usr/include/xfs

LLDLIBS = $(LIBDM)

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

install install-dev: default

$(TARGETS):
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

-include .dep
