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

#include <errno.h>

#include <stdlib.h>
#include <lib/dmport.h>


/* See dmport.h for a description of why all these wrapper routines are
   necessary.  Because SGI defines stub routines for all functions it didn't
   implement, no SGI-specific wrappers are required.
*/


#ifdef	VERITAS_21

/* The Veritas version of dm_downgrade_right has two additional parameters
   not defined in the XDSM spec.  Provide values that make dm_downgrade_right
   work according to the spec.
*/

extern int
xvfs_dm_downgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
#undef	dm_downgrade_right
	return(dm_downgrade_right(sid, hanp, hlen, token, 0, DM_RIGHT_SHARED));
}


/* The last byte in a Veritas handle is a 'pad' byte which they don't bother
   to initialize to zero.  As a result, you can't do binary compares of
   two handles to see if they are the same.  This stub (along with others)
   forces the pad byte to zero so that binary compares are possible
*/

extern int
xvfs_dm_fd_to_handle(
	int		fd,
	void		**hanpp,
	size_t		*hlenp)
{
#undef	dm_fd_to_handle
	int		rc;

	if ((rc = dm_fd_to_handle(fd, hanpp, hlenp)) == 0) {
		if (*hlenp == 16) {
			char	*cp = (char *)*hanpp;
			cp[15] = '\0';
		}
	}
	return(rc);
}


/* The Veritas version of dm_get_config_events has a slightly different name.
*/

extern int
dm_get_config_events(
	void		*hanp,
	size_t		hlen,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp)
{
	return(dm_get_config_event(hanp, hlen, nelem, eventsetp, nelemp));
}


/* In version 2.1 uflags was defined as 'int nowait', so a value of zero in
   this field means that the caller wants to wait.  In XDSM this was reversed,
   where if the bottom bit of uflags is set then the caller wants to wait.
   This routine makes the conversion.
*/

extern int
xvfs_dm_get_events(
	dm_sessid_t	sid,
	u_int		maxmsgs,
	u_int		uflags,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
#undef	dm_get_events
	int	nowait = 1;

	if (uflags & DM_EV_WAIT)
		nowait = 0;

	return(dm_get_events(sid, maxmsgs, nowait, buflen, bufp, rlenp));
}


/* The Veritas version of dm_get_mountinfo has the parameters in a different
   order than in the XDSM spec.  Hide this in the wrapper.
*/

extern int
xvfs_dm_get_mountinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
#undef	dm_get_mountinfo
	return(dm_get_mountinfo(sid, token, hanp, hlen, buflen, bufp, rlenp));
}


/* Veritas does not support the dm_getall_disp function.  Furthermore, their
   dm_dispinfo_t structure is missing the _link field that is needed for the
   DM_STEP_TO_NEXT() macro to work.
*/

extern int
dm_getall_disp(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return(ENOSYS);
}


/* Veritas does not support the dm_getall_dmattr function.  Furthermore, their
   dm_attrlist_t structure is missing the _link field that is needed for the
   DM_STEP_TO_NEXT() macro to work.
*/

extern int
dm_getall_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return(ENOSYS);
}


/* Veritas does not support dm_handle_is_valid.  We emulate it by checking
   fields which have known values, as DMF uses this a lot.
*/

extern dm_boolean_t
dm_handle_is_valid(
	void		*hanp,
	size_t		hlen)
{
	char		*cp = (char *)hanp;

	if (hlen != 16) {
		return(DM_FALSE);
	}
	if (cp[15] != '\0') {
		return(DM_FALSE);
	}
	switch (cp[14]) {
	case 1:
	case 2:
	case 4:
		break;
	default:
		return(DM_FALSE);
	}
	switch (cp[13]) {
	case 0:
	case 1:
		break;
	default:
		return(DM_FALSE);
	}
	return(DM_TRUE);
}


/* Veritas uses a dev_t for their dm_fsid_t, and named their routines
   accordingly.  Hide this in the wrapper.  Note that a dev_t is 32 bits
   and the SGI dm_fsid_t is defined to be a uint64_t.  If this gets to
   be a problem (e.g. %x verus %llx), the Veritas dm_fsid_t could be made
   to match SGI with a little casting here and there.
*/

extern int
dm_handle_to_fsid(
	void		*hanp,
	size_t		hlen,
	dm_fsid_t	*fsidp)
{
	dev_t		dev;

	dev = dm_handle_to_dev(hanp, hlen);
	*fsidp = (dm_fsid_t)dev;
	return(0);
}


/* The Veritas dm_handle_to_fgen returns a long which is really the
   file's generation number.  dm_igen_t is typedef'd to be a long also,
   so the cast here is safe.
*/

extern int
dm_handle_to_igen(
	void		*hanp,
	size_t		hlen,
	dm_igen_t	*igenp)
{
#undef	dm_handle_to_igen
	long		igen;

	igen = (dm_igen_t)dm_handle_to_fgen(hanp, hlen);
	*igenp = (dm_igen_t)igen;
	return(0);
}


/* Veritas uses a long for their dm_ino_t, which is really an ino_t.
   Hide this in the wrapper.  Note that a ino_t is 32 bits and the SGI
   dm_ino_t is defined to be a uint64_t.  If this gets to be a problem
   (e.g. %x verus %llx), the Veritas dm_ino_t could be made to match SGI
   with a little casting here and there.
*/


extern int
xvfs_dm_handle_to_ino(
	void		*hanp,
	size_t		hlen,
	dm_ino_t	*inop)
{
#undef	dm_handle_to_ino
	long		ino;

	ino = (dm_ino_t)dm_handle_to_ino(hanp, hlen);
	*inop = (dm_ino_t)ino;
	return(0);
}


/* Version 2.1 of the spec did not specify the versionstrpp parameter.  This
   code makes the routine appear to work correctly to the caller, but it
   is not really able to do the runtime check that the parameter is
   supposed to provide.
*/

extern int
xvfs_dm_init_service(
	char		**versionstrpp)
{
#undef	dm_init_service
	*versionstrpp = DM_VER_STR_CONTENTS;
	return(dm_init_service());
}


extern int
xvfs_dm_make_fshandle(
	dm_fsid_t	*fsidp,
	void		**hanpp,
	size_t	  *hlenp)
{
#undef	dm_make_fshandle
	dev_t		dev;

	dev = (dev_t)*fsidp;
	return(dm_make_fshandle(dev, hanpp, hlenp));
}


extern int
xvfs_dm_make_handle(
	dm_fsid_t	*fsidp,
	dm_ino_t	*inop,
	dm_igen_t	*igenp,
	void		**hanpp,
	size_t	  *hlenp)
{
#undef	dm_make_handle
	dev_t		dev;
	long		ino;
	long		igen;

	dev = (dev_t)*fsidp;
	ino = (long)*inop;
	igen = (long)*igenp;
	return(dm_make_handle(dev, ino, igen, hanpp, hlenp));
}


/* The last byte in a Veritas handle is a 'pad' byte which they don't bother
   to initialize to zero.  As a result, you can't do binary compares of
   two handles to see if they are the same.  This stub (along with others)
   forces the pad byte to zero so that binary compares are possible
*/

extern int
xvfs_dm_path_to_fshandle(
	char		*path,
	void		**fshanpp,
	size_t		*fshlenp)
{
#undef	dm_path_to_fshandle
	int		rc;

	if ((rc = dm_path_to_fshandle(path, fshanpp, fshlenp)) == 0) {
		if (*fshlenp == 16) {
			char	*cp = (char *)*fshanpp;
			cp[15] = '\0';
		}
	}
	return(rc);
}


/* The last byte in a Veritas handle is a 'pad' byte which they don't bother
   to initialize to zero.  As a result, you can't do binary compares of
   two handles to see if they are the same.  This stub (along with others)
   forces the pad byte to zero so that binary compares are possible
*/

extern int
xvfs_dm_path_to_handle(
	char		*path,
	void		**hanpp,
	size_t		*hlenp)
{
#undef	dm_path_to_handle
	int		rc;

	if ((rc = dm_path_to_handle(path, hanpp, hlenp)) == 0) {
		if (*hlenp == 16) {
			char	*cp = (char *)*hanpp;
			cp[15] = '\0';
		}
	}
	return(rc);
}


/* Veritas has a prototype for this function even though it is not
   implemented.
*/

extern int
xvfs_dm_pending(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_timestruct_t	*delay)
{
#undef	dm_pending
	return(ENOSYS);
}


extern int
xvfs_dm_read_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp)
{
#undef	dm_read_invis
	return(dm_read_invis(sid, hanp, hlen, token, off, (dm_ssize_t)len, bufp));
}


/* In version 2.1 uflags was defined as 'int nowait', so a value of zero in
   this field means that the caller wants to wait.  In XDSM this was reversed,
   where if the bottom bit of uflags is set then the caller wants to wait.
   This routine makes the conversion.
*/

extern int
xvfs_dm_request_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		uflags,
	dm_right_t	right)
{
#undef	dm_request_right
	int	nowait = 1;

	if (uflags & DM_RR_WAIT)
		nowait = 0;
	return(dm_request_right(sid, hanp, hlen, token, nowait, right));
}


extern int
xvfs_dm_set_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	mode_t		mode)
{
#undef	dm_set_inherit
	return(dm_set_inherit(sid, hanp, hlen, token, attrnamep, (u_int)mode));
}


extern int
xvfs_dm_set_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	dm_boolean_t	*exactflagp)
{
#undef	dm_set_region
	return(dm_set_region(sid, hanp, hlen, token, nelem, regbufp, (u_int *)exactflagp));
}


extern int
dm_set_return_on_destroy(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	dm_boolean_t	enable)
{
	return(dm_set_return_ondestroy(sid, hanp, hlen, token,  attrnamep, (int)enable));
}


extern int
xvfs_dm_sync_by_handle(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
#undef	dm_sync_by_handle
	return((int)dm_sync_by_handle(sid, hanp, hlen, token));
}

extern int
xvfs_dm_upgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
#undef	dm_upgrade_right
	return(dm_upgrade_right(sid, hanp, hlen, token, 0, DM_RIGHT_EXCL));
}


extern int
xvfs_dm_write_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		flags,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp)
{
#undef	dm_write_invis
	return(dm_write_invis(sid, hanp, hlen, token, off, (dm_ssize_t)len, bufp, flags));
}

#endif
