## Process this file with automake to produce Makefile.in

## 
## Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
## 
## This program is free software; you can redistribute it and/or modify it
## under the terms of version 2 of the GNU General Public License as
## published by the Free Software Foundation.
## 
## This program is distributed in the hope that it would be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## 
## Further, this software is distributed without any warranty that it is
## free of the rightful claim of any third person regarding infringement
## or the like.  Any license provided herein, whether implied or
## otherwise, applies only to this software file.  Patent licenses, if
## any, provided herein do not apply to combinations of this program with
## other software, or any other product whatsoever.
## 
## You should have received a copy of the GNU General Public License along
## with this program; if not, write the Free Software Foundation, Inc., 59
## Temple Place - Suite 330, Boston MA 02111-1307, USA.
## 
## Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
## Mountain View, CA  94043, or:
## 
## http://www.sgi.com 
## 
## For further information regarding this notice, see: 
## 
## http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
##

# things needing -ldm -lhandle -ldmtest
DM_TARGS = \
	test_assumption \
	get_eventlist \
	set_eventlist \
	set_disp \
	get_region \
	get_dmattr \
	set_dmattr \
	remove_dmattr \
	probe_hole \
	punch_hole \
	get_fileattr \
	sync_by_handle \
	getall_dmattr \
	set_fileattr \
	path_to_handle \
	path_to_fshandle \
	get_mountinfo \
	getall_disp \
	get_events \
	dm_handle \
	handle_to_fshandle \
	get_config_events \
	get_allocinfo \
	create_userevent \
	request_right \
	release_right \
	upgrade_right \
	query_right \
	downgrade_right \
	obj_ref_hold \
	obj_ref_rele \
	obj_ref_query \
	print_event

# things needing -ldm -lhandle
NT_TARGS = \
	fd_to_handle \
	handle_to_path \
	init_service \
	pending \
	print_fshandle \
	respond_event

# things needing -ldm -ldmtest
NTDM_TARGS = \
	make_sparse \
	randomize_file \
	rwt \
	struct_test


bin_PROGRAMS = $(DM_TARGS) $(NT_TARGS) $(NTDM_TARGS) link_test

LDADD = $(top_builddir)/src/common/lib/libdmtest.la
