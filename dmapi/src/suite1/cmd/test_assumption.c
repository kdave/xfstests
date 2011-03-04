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

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include <lib/hsm.h>

/*
	Test that the new session ID is correctly returned to the caller on
	dm_create_session when the oldsid parameter is not DM_NO_SESSION.

	Test to make sure that DM_NO_TOKEN does not work if the session ID
	is invalid.

	Both are needed so that session assumption works correctly.  The
	test creates a session, queries the session, assumes the session,
	then attempts to check the event list of the old session ID while
	using DM_NO_TOKEN.

	The only parameter is the pathname of a DMAPI filesystem.  If it is
	working correctly, you should get the error message:

		SUCCESS!

	Any message containing the word FAILURE indicates a problem.
*/

char		*Progname;
dm_sessid_t	sid = DM_NO_SESSION;	/* session ID of original session */
dm_sessid_t	newsid = DM_NO_SESSION;	/* session ID after session resumed */
dm_eventset_t	eventlist;

int
main(
	int	argc, 
	char	**argv)
{
	char		buffer[DM_SESSION_INFO_LEN];
	void		*hanp;
	size_t		hlen;
	u_int		nelem;
	size_t	 	 rlen;

	Progname = argv[0];

	if (argc != 2) {
		fprintf(stderr, "usage:\t%s filesystem\n", Progname);
		exit(1);
	}

	/* Initialize the DMAPI interface and obtain a session ID, then verify
	   that the filesystem supports DMAPI.
	*/

	if (setup_dmapi(&sid))
		exit(1);

	if (dm_path_to_handle(argv[1], &hanp, &hlen)) {
		perror("FAILURE: can't get handle for filesystem");
		exit(1);
	}

	/* Query the session just to make sure things are working okay. */

        if (dm_query_session(sid, sizeof(buffer), buffer, &rlen)) {
                errno_msg("FAILURE: can't query the original session ID %d",
			sid);
                exit(1);
        }
	fprintf(stdout, "Initial session ID: %d\n", sid);
	fprintf(stdout, "Initial session message length: '%zd'\n", rlen);
	if (rlen > 0) {
		fprintf(stdout, "Initial session message: '%s'\n", buffer);
	}

	/* Now try to assume the session. */

	if (dm_create_session(sid, "this is a new message", &newsid))  {
		fprintf(stderr, "FAILURE: can't assume session %d\n", sid);
		exit(1);
	}

	/* Now query the new session. */

        if (dm_query_session(newsid, sizeof(buffer), buffer, &rlen)) {
                errno_msg("FAILURE: can't query the assumed session ID %d",
			newsid);
                exit(1);
        }
	fprintf(stdout, "Assumed session ID: %d\n", newsid);
	fprintf(stdout, "Assumed session message length: '%zd'\n", rlen);
	if (rlen > 0) {
		fprintf(stdout, "Assumed session message: '%s'\n", buffer);
	}

	/* Get rid of the new session as we are done with it. */

	if (dm_destroy_session(newsid))  {
		fprintf(stderr, "FAILURE: Can't shut down assumed session %d\n",
			newsid);
		exit(1);
	}

	/* Now verify that DM_NO_TOKEN will not work with the old session ID,
	   which is now invalid.
	*/

	DMEV_ZERO(eventlist);

	if (dm_get_eventlist(sid, hanp, hlen, DM_NO_TOKEN, DM_EVENT_MAX,
			&eventlist, &nelem) == 0) {
		fprintf(stderr, "FAILURE: dm_get_eventlist() worked when it "
			"should have failed!\n");
	}
#ifdef	VERITAS_21
	if (errno != ESRCH) {
#else
	if (errno != EINVAL) {
#endif
		errno_msg("FAILURE: unexpected errno");
		exit(1);
	}
	fprintf(stdout, "SUCCESS!\n");
	exit(0);
}
