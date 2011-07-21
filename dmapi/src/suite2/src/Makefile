#
# Copyright (C) 2011 SGI  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
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
