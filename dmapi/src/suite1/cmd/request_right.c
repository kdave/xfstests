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

#include <lib/hsm.h>

#include <getopt.h>
#include <string.h>



/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_request_right().  The
command line is:

	request_right [-F] [-w] [-s sid] token {pathname|handle} right

where
-F
	when a pathname is specified, -F indicates that its filesystem handle
	should be used rather than its file object handle.
-w
	if specified, the DM_RR_WAIT flag will be used.
sid
	is the dm_sessid_t to use rather than the default test session.
token
	is the dm_token_t to use.
{pathname|handle}
	is either a handle, or is the pathname of a file whose handle is
	to be used.
right
	is the dm_right_t to use.

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;


static void
usage(void)
{
	int	i;

	fprintf(stderr, "usage:\t%s [-F] [-w] [-s sid] token "
		"{pathname|handle} right\n", Progname);
	fprintf(stderr, "possible rights are:\n");
	for (i = 0; i < rt_namecnt; i++) {
		fprintf(stderr, "%s (%d)\n", rt_names[i].name,
			rt_names[i].value);
	}
	exit(1);
}


int
main(
	int	argc,
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	token;
	dm_right_t	right;
	char		*object;
	char		*rightstr;
	void		*hanp;
	size_t	 	hlen;
	int		Fflag = 0;
	int		wflag = 0;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "Fws:")) != EOF) {
		switch (opt) {
		case 'F':
			Fflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 3 != argc)
		usage();
	token = atol(argv[optind++]);
	object = argv[optind++];
	rightstr = argv[optind];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file or filesystem's handle. */

	if (opaque_to_handle(object, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle from %s\n", object);
		exit(1);
	}
	if (Fflag) {
		void	*fshanp;
		size_t	fshlen;

		if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen)) {
			fprintf(stderr, "can't get filesystem handle from %s\n",
				object);
			exit(1);
		}
		dm_handle_free(hanp, hlen);
		hanp = fshanp;
		hlen = fshlen;
	}

	if (rt_name_to_value(rightstr, &right)) {
		fprintf(stderr, "invalid right %s\n", rightstr);
		usage();
	}

	if (dm_request_right(sid, hanp, hlen, token,
	    (wflag ? DM_RR_WAIT : 0), right)) {
		fprintf(stderr, "dm_request_right failed, %s\n",
			strerror(errno));
		return(1);
	}

	dm_handle_free(hanp, hlen);
	exit(0);
}
