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

#include <stdlib.h>
#include <lib/dmport.h>

/*
	The variable 'flags' was used with two different meanings within
        the spec.  Sometimes it was an int, and sometimes a u_int.  I 
        changed all the u_int references to use the new symbol uflags.

        The variable 'rlenp' was also used with two different meanings;
        sometimes size_t, and sometimes dm_size_t.  I introduced the new
	symbol 'dmrlenp' for the dm_size_t cases.
*/

char *Progname;

int
main(
	int	argc,
	char	**argv)
{
	dm_sessid_t	sid = 0, oldsid = 0, targetsid = 0;
	dm_sessid_t	*newsidp = NULL, *sidbufp = NULL;
	dm_token_t	token = 0, *tokenp = NULL;
	dm_token_t	*rtokenp = NULL, *tokenbufp = NULL;
	dm_attrname_t	*attrnamep = NULL;
	dm_off_t	off = 0, *offp = NULL, *roffp = NULL;
	dm_extent_t	*extentp = NULL;
	dm_inherit_t	*inheritbufp = NULL;
	dm_stat_t	*statp = NULL;
	dm_size_t	len = 0, *dmrlenp = NULL, *retvalp = NULL;
	dm_attrloc_t	*locp = NULL;
	dm_eventset_t	*eventsetp = NULL;
	dm_config_t	flagname = DM_CONFIG_INVALID;
	dm_region_t	*regbufp = NULL;
	dm_response_t	response = DM_RESP_INVALID;
	dm_right_t	right = DM_RIGHT_NULL, *rightp = NULL;
	dm_igen_t	igen, *igenp = NULL;
	dm_msgtype_t	msgtype = DM_MSGTYPE_INVALID;
	dm_fileattr_t	*attrp = NULL;
	dm_boolean_t	enable = 0, *exactflagp = NULL;
	dm_timestruct_t	*delay = NULL;
	mode_t		mode = 0;
	size_t		hlen = 0, dirhlen = 0, hlen1 = 0, hlen2 = 0;
	size_t		targhlen = 0, *fshlenp = NULL, *hlenp = NULL;
	size_t		msglen = 0, buflen = 0, *rlenp = NULL;
	u_int		nelem = 0, mask = 0, maxmsgs = 0, uflags = 0;
	u_int		*nelemp = NULL, maxevent = 0;
	void		*hanp = NULL, *dirhanp = NULL;
	void		*hanp1 = NULL, *hanp2 = NULL, *targhanp = NULL;
	void		*msgdatap = NULL, *bufp = NULL, **hanpp = NULL;
	void		*respbufp = NULL, **fshanpp = NULL;
	dm_fsid_t	fsid, *fsidp = NULL;
	dm_ino_t	ino, *inop = NULL;
	char		*cname = NULL, *sessinfop = NULL;
	char		*path = NULL, *pathbufp = NULL, **versionstrpp = NULL;
	int		flags = 0, fd = 0, setdtime = 0, reterror = 0;
	u_int		urc;
	int		rc;
	dm_ssize_t	ssrc;

/* Definitions per the prototypes in dmport.h, in the same order. */

	rc = dm_clear_inherit(sid, hanp, hlen, token, attrnamep);
	rc = dm_create_by_handle(sid, dirhanp, dirhlen, token,
		hanp, hlen, cname);
	rc = dm_create_session(oldsid, sessinfop, newsidp);
	rc = dm_create_userevent(sid, msglen, msgdatap, tokenp);
	rc = dm_destroy_session(sid);
	rc = dm_downgrade_right(sid, hanp, hlen, token);
	rc = dm_fd_to_handle(fd, hanpp, hlenp);
	rc = dm_find_eventmsg(sid, token, buflen, bufp, rlenp);
	rc = dm_get_allocinfo(sid, hanp, hlen,
		token, offp, nelem, extentp, nelemp);
	rc = dm_get_bulkall(sid, hanp, hlen, token, mask, attrnamep,
		locp, buflen, bufp, rlenp);
	rc = dm_get_bulkattr(sid, hanp, hlen, token, mask, locp, buflen, 
		bufp, rlenp);
	rc = dm_get_config(hanp, hlen, flagname, retvalp);
	rc = dm_get_config_events(hanp, hlen, nelem, eventsetp, nelemp);
	rc = dm_get_dirattrs(sid, hanp, hlen, token, mask, locp, buflen,
		bufp, rlenp);
	rc = dm_get_dmattr(sid, hanp, hlen, token, attrnamep, buflen,
		bufp, rlenp);
	rc = dm_get_eventlist(sid, hanp, hlen, token, nelem, eventsetp, nelemp);
	rc = dm_get_events(sid, maxmsgs, flags, buflen, bufp, rlenp);
	rc = dm_get_fileattr(sid, hanp, hlen, token, mask, statp);
	rc = dm_get_mountinfo(sid, hanp, hlen, token, buflen, bufp, rlenp);
	rc = dm_get_region(sid, hanp, hlen, token, nelem, regbufp, nelemp);
	rc = dm_getall_disp(sid, buflen, bufp, rlenp);
	rc = dm_getall_dmattr(sid, hanp, hlen, token, buflen, bufp, rlenp);
	rc = dm_getall_inherit(sid, hanp, hlen,
		token, nelem, inheritbufp, nelemp);
	rc = dm_getall_sessions(nelem, sidbufp, nelemp);
	rc = dm_getall_tokens(sid, nelem, tokenbufp, nelemp);
	rc = dm_handle_cmp(hanp1, hlen1, hanp2, hlen2);
	dm_handle_free(hanp, hlen);
	urc = dm_handle_hash(hanp, hlen);
	rc = dm_handle_is_valid(hanp, hlen);
	rc = dm_handle_to_fshandle(hanp, hlen, fshanpp, fshlenp);
	rc = dm_handle_to_fsid(hanp, hlen, fsidp);
	rc = dm_handle_to_igen(hanp, hlen, igenp);
	rc = dm_handle_to_ino(hanp, hlen, inop);
	rc = dm_handle_to_path(dirhanp, dirhlen, targhanp, targhlen, 
		buflen, pathbufp, rlenp);
	rc = dm_init_attrloc(sid, hanp, hlen, token, locp);
	rc = dm_init_service(versionstrpp);
	rc = dm_make_handle(&fsid, &ino, &igen, hanpp, hlenp);
	rc = dm_make_fshandle(&fsid, hanpp, hlenp);
	rc = dm_mkdir_by_handle(sid, dirhanp, dirhlen, token,
		hanp, hlen, cname); 
	rc = dm_move_event(sid, token, targetsid, rtokenp);
	rc = dm_obj_ref_hold(sid, token, hanp, hlen);
	rc = dm_obj_ref_query(sid, token, hanp, hlen);
	rc = dm_obj_ref_rele(sid, token, hanp, hlen);
	rc = dm_path_to_fshandle(path, hanpp, hlenp);
	rc = dm_path_to_handle(path, hanpp, hlenp);
	rc = dm_pending(sid, token, delay);
	rc = dm_probe_hole(sid, hanp, hlen, token, off, len, roffp, dmrlenp);
	rc = dm_punch_hole(sid, hanp, hlen, token, off, len);
	rc = dm_query_right(sid, hanp, hlen, token, rightp);
	rc = dm_query_session(sid, buflen, bufp, rlenp);
	ssrc = dm_read_invis(sid, hanp, hlen, token, off, len, bufp);
	rc =  dm_release_right(sid, hanp, hlen, token);
	rc = dm_remove_dmattr(sid, hanp, hlen, token, setdtime, attrnamep);
	rc = dm_request_right(sid, hanp, hlen, token, uflags, right);
	rc = dm_respond_event(sid, token, response, reterror, buflen, respbufp);
	rc = dm_send_msg(sid, msgtype, buflen, bufp);
	rc = dm_set_disp(sid, hanp, hlen, token, eventsetp, maxevent);
	rc = dm_set_dmattr(sid, hanp, hlen,
		token, attrnamep, setdtime, buflen, bufp);
	rc = dm_set_eventlist(sid, hanp, hlen, token, eventsetp, maxevent);
	rc = dm_set_fileattr(sid, hanp, hlen, token, mask, attrp);
	rc = dm_set_inherit(sid, hanp, hlen, token, attrnamep, mode);
	rc = dm_set_region(sid, hanp, hlen, token, nelem, regbufp, exactflagp);
	rc = dm_set_return_on_destroy(sid, hanp, hlen,
		token,  attrnamep, enable);
	rc = dm_symlink_by_handle(sid, dirhanp, dirhlen, token,
		hanp, hlen, cname, path); 
	rc = dm_sync_by_handle(sid, hanp, hlen, token);
	rc = dm_upgrade_right(sid, hanp, hlen, token);
	ssrc = dm_write_invis(sid, hanp, hlen, flags, token, off, len, bufp);
	exit(0);
}
