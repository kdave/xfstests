/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.
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

#include <string.h>
#include <getopt.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_set_return_on_destroy().  The
command line is:

	set_return_on_destroy [-F] [-s sid] [-t token] {pathname|fshandle} [attr]

where pathname is the name of a file which resides in the filesystem of
interest.  attr is the name of the DMAPI attribute; if none is specified,
then set-return-on-destroy will be disabled for the filesystem.
sid is the session ID whose attribute you are interested in setting.

Use -F if you want the program to find the fshandle based on the pathname.

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
	fprintf(stderr, "usage:\t%s [-F] [-s sid] [-t token] {pathname|fshandle} [attr]\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	token = DM_NO_TOKEN;
	char		*pathname;
	dm_attrname_t	*attrnamep = NULL;
	dm_boolean_t	enable = DM_FALSE;
	void		*hanp;
	size_t	 	hlen;
	char		*name;
	int		opt;
	int		Fflag = 0;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "Fs:t:")) != EOF) {
		switch (opt) {
		case 's':
			sid = atol(optarg);
			break;
		case 't':
			token = atol(optarg);
			break;
		case 'F':
			Fflag++;
			break;
		case '?':
			usage();
		}
	}
	if (optind == argc || optind + 2 < argc)
		usage();
	pathname = argv[optind++];
	if (optind < argc) {
		enable = DM_TRUE;
		attrnamep = (dm_attrname_t *)argv[optind++];
	}

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	if (opaque_to_handle(pathname, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n", pathname);
		exit(1);
	}

	/* Get the file's handle. */

	if (Fflag) {
		void *fshanp;
		size_t fshlen;

		if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen)) {
			fprintf(stderr, "can't get filesystem handle for file %s, %s\n",
				pathname, strerror(errno));
			exit(1);
		}
		dm_handle_free(hanp, hlen);
		hanp = fshanp;
		hlen = fshlen;
	}

	if (dm_set_return_on_destroy(sid, hanp, hlen, token,
	    attrnamep, enable)) {
		fprintf(stderr, "dm_set_return_on_destroy failed, %s\n",
			strerror(errno));
		exit(1);
	}

	dm_handle_free(hanp, hlen);
	exit(0);
}
