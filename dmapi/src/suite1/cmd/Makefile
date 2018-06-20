# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../../../..

include $(TOPDIR)/include/builddefs

# things needing -ldm -lhandle -ldmtest
DM_TARGS = test_assumption get_eventlist set_eventlist set_disp \
		get_region get_dmattr set_dmattr remove_dmattr \
		probe_hole probe_punch_xfsctl_hole punch_hole \
		get_fileattr sync_by_handle getall_dmattr \
		set_fileattr path_to_handle path_to_fshandle \
		get_mountinfo getall_disp get_events dm_handle \
		handle_to_fshandle get_config_events get_allocinfo \
		create_userevent request_right release_right \
		upgrade_right query_right downgrade_right \
		obj_ref_hold obj_ref_rele obj_ref_query print_event \
		get_dirattrs

# things needing -ldm -lhandle
NT_TARGS = fd_to_handle handle_to_path init_service pending \
		print_fshandle respond_event

# things needing -ldm -ldmtest
NTDM_TARGS = make_sparse randomize_file rwt struct_test

# things left out for some reason...
UNUSED_TARGS = make_rt_sparse security_hole2 security_hole

TARGETS = $(DM_TARGS) $(NT_TARGS) $(NTDM_TARGS) link_test

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

CFLAGS += -I$(TOPDIR)/dmapi/src/common -I/usr/include/xfs

LLDLIBS = $(TOPDIR)/dmapi/src/common/lib/libdmtest.la $(LIBDM)

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

install install-dev: default

$(TARGETS):
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

-include .dep
