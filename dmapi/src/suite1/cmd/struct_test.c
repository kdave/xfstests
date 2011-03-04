/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
	
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef linux
#include <dmapi.h>
#else
#include <sys/dmi.h>
#endif

/* The purpose of this test is to make sure that each field in each structure
   defined in dmi.h is properly aligned so that there is no dead space, and
   that the sizes and offsets of those fields remain constant no matter what
   mode the program is compiled with, i.e. -32, -n32 or -64.

   Run the program with no parameters.  If everything is correct, no message
   will start with the word "ERROR:".  Compile and run the test in each of
   the three mods and diff the outputs.  They must all be identical.

   Note: this test cannot detect the addition of new structures to dmi.h, so
   it will have to periodically be compared with dmi.h manually to ensure that
   all the structures are still being validated!
*/

#define	S_START(struct_name)	{ offset = 0; s_name = #struct_name; }

#define	S_NEXT(struct_name, field_name) \
{ \
	struct_name	X; \
 \
	f_name = #field_name; \
	printf("field %s.%s offset is %d\n", s_name, f_name, offset); \
	if (offsetof(struct_name, field_name) != offset) { \
		printf("ERROR: field %s should be %d\n", \
			#struct_name "." #field_name, \
			(int) offsetof(struct_name, field_name)); \
	} \
	offset = offsetof(struct_name, field_name) + sizeof(X.field_name); \
}

#define	S_END(struct_name) \
{ \
	printf("struct %s size is %d\n", s_name, offset); \
	if (sizeof(struct_name) != offset) { \
		printf("ERROR: struct %s should be %zd\n", \
			s_name, sizeof(struct_name)); \
	} \
}


char *Progname;

int main(
	int	argc,
	char	**argv)
{
	char	*s_name = NULL;
	char	*f_name = NULL;
	int	offset = 0;


	S_START(dm_vardata_t);
	S_NEXT(dm_vardata_t, vd_offset);
	S_NEXT(dm_vardata_t, vd_length);
	S_END(dm_vardata_t);


	S_START(dm_attrname_t);
	S_NEXT(dm_attrname_t, an_chars);
	S_END(dm_attrname_t);


	S_START(dm_attrlist_t);
	S_NEXT(dm_attrlist_t, 	_link);
	S_NEXT(dm_attrlist_t, al_name);
	S_NEXT(dm_attrlist_t, al_data);
	S_END(dm_attrlist_t);


	S_START(dm_dispinfo_t);
	S_NEXT(dm_dispinfo_t, _link);
	S_NEXT(dm_dispinfo_t, di_pad1);
	S_NEXT(dm_dispinfo_t, di_fshandle);
	S_NEXT(dm_dispinfo_t, di_eventset);
	S_END(dm_dispinfo_t);


	S_START(dm_eventmsg_t);
	S_NEXT(dm_eventmsg_t, _link);
	S_NEXT(dm_eventmsg_t, ev_type);
	S_NEXT(dm_eventmsg_t, ev_token);
	S_NEXT(dm_eventmsg_t, ev_sequence);
	S_NEXT(dm_eventmsg_t, ev_data);
	S_END(dm_eventmsg_t);


	S_START(dm_cancel_event_t);
	S_NEXT(dm_cancel_event_t, ce_sequence);
	S_NEXT(dm_cancel_event_t, ce_token);
	S_END(dm_cancel_event_t);


	S_START(dm_data_event_t);
	S_NEXT(dm_data_event_t, de_handle);
	S_NEXT(dm_data_event_t, de_offset);
	S_NEXT(dm_data_event_t, de_length);
	S_END(dm_data_event_t);


	S_START(dm_destroy_event_t);
	S_NEXT(dm_destroy_event_t, ds_handle);
	S_NEXT(dm_destroy_event_t, ds_attrname);
	S_NEXT(dm_destroy_event_t, ds_attrcopy);
	S_END(dm_destroy_event_t);


	S_START(dm_mount_event_t);
	S_NEXT(dm_mount_event_t, me_mode);
	S_NEXT(dm_mount_event_t, me_handle1);
	S_NEXT(dm_mount_event_t, me_handle2);
	S_NEXT(dm_mount_event_t, me_name1);
	S_NEXT(dm_mount_event_t, me_name2);
	S_NEXT(dm_mount_event_t, me_roothandle);
	S_END(dm_mount_event_t);


	S_START(dm_namesp_event_t);
	S_NEXT(dm_namesp_event_t, ne_mode);
	S_NEXT(dm_namesp_event_t, ne_handle1);
	S_NEXT(dm_namesp_event_t, ne_handle2);
	S_NEXT(dm_namesp_event_t, ne_name1);
	S_NEXT(dm_namesp_event_t, ne_name2);
	S_NEXT(dm_namesp_event_t, ne_retcode);
	S_END(dm_namesp_event_t);


	S_START(dm_extent_t);
	S_NEXT(dm_extent_t, ex_type);
	S_NEXT(dm_extent_t, ex_pad1);
	S_NEXT(dm_extent_t, ex_offset);
	S_NEXT(dm_extent_t, ex_length);
	S_END(dm_extent_t);


	S_START(dm_fileattr_t);
	S_NEXT(dm_fileattr_t, fa_mode);
	S_NEXT(dm_fileattr_t, fa_uid);
	S_NEXT(dm_fileattr_t, fa_gid);
	S_NEXT(dm_fileattr_t, fa_atime);
	S_NEXT(dm_fileattr_t, fa_mtime);
	S_NEXT(dm_fileattr_t, fa_ctime);
	S_NEXT(dm_fileattr_t, fa_dtime);
	S_NEXT(dm_fileattr_t, fa_pad1);
	S_NEXT(dm_fileattr_t, fa_size);
	S_END(dm_fileattr_t);


	S_START(dm_inherit_t);
	S_NEXT(dm_inherit_t, ih_name);
	S_NEXT(dm_inherit_t, ih_filetype);
	S_END(dm_inherit_t);


	S_START(dm_region_t);
	S_NEXT(dm_region_t, rg_offset);
	S_NEXT(dm_region_t, rg_size);
	S_NEXT(dm_region_t, rg_flags);
	S_NEXT(dm_region_t, rg_pad1);
	S_END(dm_region_t);


	S_START(dm_stat_t);
	S_NEXT(dm_stat_t, _link);
	S_NEXT(dm_stat_t, dt_handle);
	S_NEXT(dm_stat_t, dt_compname);
	S_NEXT(dm_stat_t, dt_nevents);
	S_NEXT(dm_stat_t, dt_emask);
	S_NEXT(dm_stat_t, dt_pers);
	S_NEXT(dm_stat_t, dt_pmanreg);
	S_NEXT(dm_stat_t, dt_dtime);
	S_NEXT(dm_stat_t, dt_change);
	S_NEXT(dm_stat_t, dt_pad1);

	S_NEXT(dm_stat_t, dt_dev);
	S_NEXT(dm_stat_t, dt_ino);
	S_NEXT(dm_stat_t, dt_mode);
	S_NEXT(dm_stat_t, dt_nlink);
	S_NEXT(dm_stat_t, dt_uid);
	S_NEXT(dm_stat_t, dt_gid);
	S_NEXT(dm_stat_t, dt_rdev);
	S_NEXT(dm_stat_t, dt_pad2);
	S_NEXT(dm_stat_t, dt_size);
	S_NEXT(dm_stat_t, dt_atime);
	S_NEXT(dm_stat_t, dt_mtime);
	S_NEXT(dm_stat_t, dt_ctime);
	S_NEXT(dm_stat_t, dt_blksize);
	S_NEXT(dm_stat_t, dt_blocks);

	S_NEXT(dm_stat_t, dt_pad3);
	S_NEXT(dm_stat_t, dt_fstype);

	S_NEXT(dm_stat_t, dt_xfs_igen);
	S_NEXT(dm_stat_t, dt_xfs_xflags);
	S_NEXT(dm_stat_t, dt_xfs_extsize);
	S_NEXT(dm_stat_t, dt_xfs_extents);
	S_NEXT(dm_stat_t, dt_xfs_aextents);
	S_NEXT(dm_stat_t, dt_xfs_dmstate);
	S_END(dm_stat_t);


	S_START(dm_xstat_t);
	S_NEXT(dm_xstat_t, dx_statinfo);
	S_NEXT(dm_xstat_t, dx_attrdata);
	S_END(dm_xstat_t);
	exit(0);
}
