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
#include <sys/param.h>

#include <string.h>
#include <getopt.h>

#include <lib/hsm.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_get_mountinfo().  The
command line is:

	get_mountinfo [-b buflen] [-s sid] pathname

where pathname is the name of a file, buflen is the size of the buffer to use
in the call, and sid is the session ID whose attributes you are interested in.

----------------------------------------------------------------------------*/

  /*
   * Define some standard formats for the printf statements below.
   */

#define HDR  "%s: token %d sequence %d\n"
#define VALS "\t%-15s %s\n"
#define VALD "\t%-15s %d\n"
#define VALLLD "\t%-15s %ld\n"

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;



static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-b buflen] [-s sid] pathname\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*pathname;
	void		*bufp = NULL;
	size_t		buflen = 10000;
	size_t		rlenp;
	void		*fshanp;
	size_t	 	 fshlen;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "b:s:")) != EOF) {
		switch (opt) {
		case 'b':
			buflen = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	pathname = argv[optind++];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file's handle. */

	if (dm_path_to_fshandle(pathname, &fshanp, &fshlen)) {
		fprintf(stderr, "can't get fshandle for file %s, %s\n",
			pathname, strerror(errno));
		exit(1);
	}

	if (buflen > 0) {
		if ((bufp = malloc(buflen)) == NULL) {
			fprintf(stderr, "malloc failed, %s\n", strerror(errno));
			exit(1);
		}
	}

	if (dm_get_mountinfo(sid, fshanp, fshlen, DM_NO_TOKEN, buflen,
	    bufp, &rlenp)) {
		if (errno == E2BIG) {
			fprintf(stderr, "dm_get_mountinfo buffer too small, "
				"should be %zd bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_get_mountinfo failed, %s\n",
				strerror(errno));
		}
		exit(1);
	}
	fprintf(stdout, "rlenp is %zd\n", rlenp);
	print_one_mount_event(bufp);

	dm_handle_free(fshanp, fshlen);
	exit(0);
}
