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

#ifndef	_DMFSAPI_DMPORT_H
#define	_DMFSAPI_DMPORT_H

#ifdef	__cplusplus
extern	"C" {
#endif

/**************************************************************************
 *                                                                        *
 * DMF's use of DMAPI is based upon the X/Open document 		  *
 *      Systems Management: Data Storage Managment (XDSM) API             *
 * dated February 1997.  However, no implementation of DMAPI perfectly	  *
 * matches the spec.  Each implementation has some functions that it	  *
 * didn't implement, non-standard functions that it added, differences	  *
 * in function parameter types, return value types, number and order of	  *
 * parameters, etc.  There are also differences in the formats of some	  *
 * of the DMAPI structures used by those functions.  Finally, there are	  *
 * many scalar values for which DMAPI assigned no particular size.  For	  *
 * example, a dm_fsid_t is 32 bits under Veritas and 64 bits under SGI.   *
 *									  *
 * To hide the differences as much as possible, this include file tries	  *
 * to shoehorn each DMAPI implementation into the XDSM mold as much as	  *
 * possible.  Functions which are not compatible with the XDSM spec are   *
 * redefined using wrapper routines to make them conform to the spec.	  *
 * Functions which were not implemented have stubs defined which return   *
 * ENOSYS (see file stubs.c for all wrappers and stubs).  In dmport.h,	  *
 * missing structures and scalers are defined, incompatible typedefs are  *
 * redefined, etc.							  *
 *									  *
 * The goal is to reduce as much as possible the number of #ifdefs that   *
 * have to be added to DMF to handle specific DMAPI implementations.      *
 * Some #ifdefs may still be required because of semantic differences     *
 * between the XDSM spec definition and the actual implementation.	  *
 *									  *
 * Following all the implementation-specific definitions, dmport.h	  *
 * includes a complete set of prototypes for all functions that are part  *
 * of  the XDSM specification.  This is done as a double-check.  Should   *
 * any of the actual implementations change, the compiler should tip us   *
 * off by complaining about imcompatible function redefinitions.	  *
 *                                                                        *
 **************************************************************************/


/* ---------------- Veritas-specific hack documentation -----------------

The following functions have no prototypes in the Veritas include file
<sys/dmapi/dmapi.h>.  They do not link either.

extern int dm_handle_is_valid(void *, size_t);


The following functions have prototypes in <sys/dmapi/dmapi.h>, but
they do not link.

extern int dm_getall_disp(dm_sessid_t, size_t, void *, size_t *);
extern int dm_getall_dmattr(dm_sessid_t, void *, size_t, 
		dm_token_t, size_t, void *, size_t *);


The following functions have no prototypes in <sys/dmapi/dmapi.h> but do link.

extern int dm_obj_ref_hold(dm_sessid_t, dm_token_t, void *, size_t);
extern int dm_obj_ref_rele(dm_sessid_t, dm_token_t, void *, size_t);
extern int dm_obj_ref_query(dm_sessid_t, dm_token_t, void *, size_t);


The following Veritas prototypes are different in some way from the
spec prototypes, either in parameter types, return value types, or in
some semantic way.  Look below to see the spec versions of the prototypes.

extern int dm_downgrade_right(dm_sessid_t, void *, size_t, 
		dm_token_t, int, dm_right_t);
extern int dm_get_config_events(void *, size_t, u_int,
		dm_eventset_t *, u_int *);
extern int dm_get_events(dm_sessid_t, u_int, int, size_t, void *, size_t *);
extern int dm_get_mountinfo(dm_sessid_t, dm_token_t, void *, size_t, 
		size_t, void *, size_t *);
extern int dm_init_service(void);
extern int dm_make_handle(dev_t , long, long, void **, size_t *);
extern int dm_make_fshandle(dev_t , void **, size_t *);
extern dev_t dm_handle_to_dev(void *, size_t);
extern long dm_handle_to_fgen(void *, size_t);
extern long dm_handle_to_ino(void *, size_t);
extern int dm_pending(dm_sessid_t, void *, size_t, dm_token_t, int);
extern dm_ssize_t dm_read_invis(dm_sessid_t, void *, size_t,
		dm_token_t, dm_off_t, dm_ssize_t, void *);
extern dm_ssize_t dm_write_invis(dm_sessid_t, void *, size_t,
		dm_token_t, dm_off_t, dm_ssize_t, void *, int);
extern int dm_request_right(dm_sessid_t, void *, size_t, 
		dm_token_t, int, dm_right_t);
extern int dm_set_inherit(dm_sessid_t, void *, size_t,
		dm_token_t, dm_attrname_t *, u_int);
extern int dm_set_return_ondestroy(dm_sessid_t, void *, size_t,
		dm_token_t,  dm_attrname_t *, int);
extern int dm_upgrade_right(dm_sessid_t, void *, size_t, 
		dm_token_t, int, dm_right_t);
extern int dm_set_region(dm_sessid_t, void *, size_t,
		dm_token_t, u_int, dm_region_t *, u_int *);
extern dm_ssize_t dm_sync_by_handle(dm_sessid_t, void *, size_t, dm_token_t);


The following Veritas prototype exists in <sys/dmapi/dmapi.h> but
does not appear in the spec.

extern void dm_attach_event(dm_sessid_t, dm_token_t);

The following functions exist in the Veritas library libdmi.a, but have no
prototypes within <sys/dmapi/dmapi.h>.  Their function is unknown.

dm_attach_event_nofork
dm_event_query
dm_event_valid
dm_get_disp
dm_set_resident
dmregion_compare
kdm_get_eventinfo
kdm_get_sessioninfo

-------------- end of Veritas-specific hack documentation --------------- */

#ifdef	VERITAS_21

#include <sys/types.h>

#include <sys/dmapi/dmapi.h>

/* Rename functions whose prototypes clash with the XDSM standard.  (Library
   routines map from XDSM functions back to Veritas functions.)
*/

#define	dm_downgrade_right	xvfs_dm_downgrade_right
#define dm_fd_to_handle		xvfs_dm_fd_to_handle
#define	dm_get_events		xvfs_dm_get_events
#define	dm_get_mountinfo	xvfs_dm_get_mountinfo
#define	dm_handle_to_ino	xvfs_dm_handle_to_ino
#define	dm_init_service		xvfs_dm_init_service
#define	dm_make_fshandle	xvfs_dm_make_fshandle
#define	dm_make_handle		xvfs_dm_make_handle
#define dm_path_to_fshandle	xvfs_dm_path_to_fshandle
#define dm_path_to_handle	xvfs_dm_path_to_handle
#define	dm_pending		xvfs_dm_pending
#define	dm_read_invis		xvfs_dm_read_invis
#define	dm_write_invis		xvfs_dm_write_invis
#define	dm_request_right	xvfs_dm_request_right
#define	dm_set_inherit		xvfs_dm_set_inherit
#define	dm_set_region		xvfs_dm_set_region
#define	dm_sync_by_handle	xvfs_dm_sync_by_handle
#define	dm_upgrade_right	xvfs_dm_upgrade_right


#define	DM_ATTR_NAME_SIZE	8
#undef	DM_ATTRNAME_SIZE
#define	DM_VER_STR_CONTENTS	"Veritas DMAPI V1.0"

#define	DM_EV_WAIT	0x1
#define	DM_RR_WAIT	0x1
#undef	DM_WAIT
#undef	DM_NOWAIT
#undef	DM_EVENT_NOWAIT

#define	DM_CONFIG_INHERIT_ATTRIBS	(DM_CONFIG_LOCK_UPGRADE + 1)
#undef	DM_CONFIG_INHERIT
#define	DM_CONFIG_MAX_HANDLE_SIZE	(DM_CONFIG_INHERIT_ATTRIBS + 1)
#undef	DM_CONFIG_MAX_FILE_HANDLE_SIZE
#define	DM_CONFIG_MAX_ATTR_ON_DESTROY	(DM_CONFIG_MAX_HANDLE_SIZE + 1)
#undef	DM_CONFIG_MAX_ATTR_BYTES_ON_DESTROY
#define	DM_CONFIG_OBJ_REF		(DM_CONFIG_MAX_ATTR_ON_DESTROY + 1)
#define	DM_CONFIG_DTIME_OVERLOAD	(DM_CONFIG_OBJ_REF + 1)
#undef	DM_CONFIG_MAX

#define	DM_AT_DTIME	DM_AT_DTIMES

/* In the dm_fileattr_t structure, Veritas used 'timeval' structures for all
   the time fields while XDSM uses 'time_t' structures.  Define some symbols
   that can be used for the time fields with all implementation types.
*/

#define FA_ATIME	fa_atime.tv_sec
#define FA_MTIME	fa_atime.tv_sec
#define FA_CTIME	fa_ctime.tv_sec
#define FA_DTIME	fa_dtime.tv_sec

#define	DM_WRITE_SYNC	0x1	/* used in dm_write_invis() */

#define	DM_UNMOUNT_FORCE 0x1	/* ne_mode field in dm_namesp_event_t */

/* Rename one event to match the XDSM standard. */

#define	DM_EVENT_CLOSE		(DM_EVENT_DESTROY + 1)
#undef	DM_EVENT_CLOSED

/* DM_EVENT_CANCEL is defined above DM_EVENT_MAX, and is unsupported. */

#undef	DM_REGION_VALIDFLAG

/* Add missing typedefs */

typedef	u_int	dm_boolean_t;
typedef	dev_t	dm_fsid_t;	/* This could be made a uint64_t with work! */
typedef	long	dm_igen_t;
typedef	long	dm_ino_t;
typedef	u_int	dm_sequence_t;


/* Define missing fields within the various event structures. */

#define ev_sequence     ev_token	/* for compilation only!  Won't work! */

#define	ds_handle	te_handle	/* fix fields in dm_destroy_event_t */
#define	ds_attrname	te_attrname
#define	ds_attrcopy	te_attrdata


struct	dm_cancel_event {		/* define missing dm_cancel_event_t */
	dm_sequence_t	ce_sequence;
	dm_token_t	ce_token;
};
typedef	struct dm_cancel_event	dm_cancel_event_t;


struct	dm_mount_event {		/* define missing dm_mount_event_t */
	mode_t		me_mode;
	dm_vardata_t	me_handle1;
	dm_vardata_t	me_handle2;
	dm_vardata_t	me_name1;
	dm_vardata_t	me_name2;
	dm_vardata_t	me_roothandle;
};
typedef	struct dm_mount_event	dm_mount_event_t;


/* Add the missing dm_timestruct_t structure used by dm_pending. */

struct	dm_timestruct {
	time_t		dm_tv_sec;
	int		dm_tv_nsec;
};
typedef	struct dm_timestruct dm_timestruct_t;

#endif



/* -------------------- SGI-specific hack documentation -------------------

   There are various fields within DMAPI structures that are not supported.
   Fill in this information someday.

   The following functions have prototypes defined in <sys/dmi.h> and have
   entry points defined in libdm.so, but they are not actually implemented
   within the kernel.  They all return ENOSYS if called.

	dm_clear_inherit
	dm_create_by_handle
	dm_get_bulkall
	dm_getall_inherit
	dm_mkdir_by_handle
	dm_set_inherit
	dm_symlink_by_handle

   The DMAPI functions which deal with rights do not work as described in
   the specification.  While the functions exist and are callable, they
   always return successfully without actually obtaining any locks within
   the filesystem.                        

	dm_downgrade_right
	dm_query_right
	dm_release_right
	dm_request_right
	dm_upgrade_right

   The following non-standard SGI prototype is added to the DMAPI interface
   so that real-time files may be migrated and recalled.

	dm_get_dioinfo

-------------- end of SGI-specific hack documentation --------------- */

#ifdef	linux

#include <dmapi.h>

/* In the dm_fileattr_t structure, Veritas used 'timeval' structures for all
   the time fields while XDSM uses 'time_t' structures.  Define some symbols
   that can be used for the time fields with all implementation types.
*/

#define FA_ATIME	fa_atime
#define FA_MTIME	fa_mtime
#define FA_CTIME	fa_ctime
#define FA_DTIME	fa_dtime

#endif /* linux */

/* ----------------------- XDSM standard prototypes ----------------------- */

/* The following list provides the prototypes for all functions defined in
   the DMAPI interface spec from X/Open (XDSM) dated February 1997.  They
   are here to force compiler errors in case the Veritas or SGI DMAPI
   prototypes should ever change without our knowing about it.
*/

extern int
dm_clear_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep);

extern int
dm_create_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname);

extern int
dm_create_session(
	dm_sessid_t	oldsid,
	char		*sessinfop,
	dm_sessid_t	*newsidp);

extern int
dm_create_userevent(
	dm_sessid_t	sid,
	size_t		msglen,
	void		*msgdatap,
	dm_token_t	*tokenp);

extern int
dm_destroy_session(
	dm_sessid_t	sid);

extern int
dm_downgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern int
dm_fd_to_handle(
	int		fd,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_find_eventmsg(
	dm_sessid_t	sid,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_allocinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	*offp,
	u_int		nelem,
	dm_extent_t	*extentp,
	u_int		*nelemp);

extern int
dm_get_bulkall(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrname_t	*attrnamep,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_bulkattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_config(
	void		*hanp,
	size_t		hlen,
	dm_config_t	flagname,
	dm_size_t	*retvalp);

extern int
dm_get_config_events(
	void		*hanp,
	size_t		hlen,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp);

extern int
dm_get_dirattrs(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp);

extern int
dm_get_events(
	dm_sessid_t	sid,
	u_int		maxmsgs,
	u_int		flags,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_fileattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_stat_t	*statp);

extern int
dm_get_mountinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	u_int		*nelemp);

extern int
dm_getall_disp(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_getall_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_getall_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_inherit_t	*inheritbufp,
	u_int		*nelemp);

extern int
dm_getall_sessions(
	u_int		nelem,
	dm_sessid_t	*sidbufp,
	u_int		*nelemp);

extern int
dm_getall_tokens(
	dm_sessid_t	sid,
	u_int		nelem,
	dm_token_t	*tokenbufp,
	u_int		*nelemp);

extern int
dm_handle_cmp(
	void		*hanp1,
	size_t		hlen1,
	void		*hanp2,
	size_t		hlen2);

extern void
dm_handle_free(
	void		*hanp,
	size_t		hlen);

extern u_int
dm_handle_hash(
	void		*hanp,
	size_t		hlen);

extern dm_boolean_t
dm_handle_is_valid(
	void		*hanp,
	size_t		hlen);

extern int
dm_handle_to_fshandle(
	void		*hanp,
	size_t		hlen,
	void		**fshanpp,
	size_t		*fshlenp);

extern int
dm_handle_to_fsid(
	void		*hanp,
	size_t		hlen,
	dm_fsid_t	*fsidp);

extern int
dm_handle_to_igen(
	void		*hanp,
	size_t		hlen,
	dm_igen_t	*igenp);

extern int
dm_handle_to_ino(
	void		*hanp,
	size_t		hlen,
	dm_ino_t	*inop);

extern int
dm_handle_to_path(
	void		*dirhanp,
	size_t		dirhlen,
	void		*targhanp,
	size_t		targhlen,
	size_t		buflen,
	char		*pathbufp,
	size_t		*rlenp);

extern int
dm_init_attrloc(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrloc_t	*locp);

extern int
dm_init_service(
	char		**versionstrpp);

extern int
dm_make_fshandle(
	dm_fsid_t	*fsidp,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_make_handle(
	dm_fsid_t	*fsidp,
	dm_ino_t	*inop,
	dm_igen_t	*igenp,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_mkdir_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname);

extern int
dm_move_event(
	dm_sessid_t	srcsid,
	dm_token_t	token,
	dm_sessid_t	targetsid,
	dm_token_t	*rtokenp);

extern int
dm_obj_ref_hold(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen);

extern int
dm_obj_ref_query(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen);

extern int
dm_obj_ref_rele(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen);

extern int
dm_path_to_fshandle(
	char		*path,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_path_to_handle(
	char		*path,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_pending(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_timestruct_t	*delay);

extern int
dm_probe_hole(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	dm_off_t	*roffp,
	dm_size_t	*rlenp);

extern int
dm_punch_hole(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len);

extern int
dm_query_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_right_t	*rightp);

extern int
dm_query_session(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern dm_ssize_t
dm_read_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp);

extern int
dm_release_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern int
dm_remove_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		setdtime,
	dm_attrname_t	*attrnamep);

extern int
dm_request_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		flags,
	dm_right_t	right);

extern int
dm_respond_event(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_response_t	response,
	int		reterror,
	size_t		buflen,
	void		*respbufp);

extern int
dm_send_msg(
	dm_sessid_t	targetsid,
	dm_msgtype_t	msgtype,
	size_t		buflen,
	void		*bufp);

extern int
dm_set_disp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent);

extern int
dm_set_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	int		setdtime,
	size_t		buflen,
	void		*bufp);

extern int
dm_set_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent);

extern int
dm_set_fileattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_fileattr_t	*attrp);

extern int
dm_set_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	mode_t		mode);

extern int
dm_set_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	dm_boolean_t	*exactflagp);

extern int
dm_set_return_on_destroy(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	dm_boolean_t	enable);

extern int
dm_symlink_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname,
	char		*path);

extern int
dm_sync_by_handle(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern int
dm_upgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern dm_ssize_t
dm_write_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		flags,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp);


#ifdef	__cplusplus
}
#endif

#endif	/* _DMFSAPI_DMPORT_H */
