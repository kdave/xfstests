/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
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
#ifdef	__sgi
#define VALLLD "\t%-15s %lld\n"
#else
#define VALLLD "\t%-15s %ld\n"
#endif

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
	void		*bufp;
	size_t		buflen = 10000;
	size_t		rlenp;
	void		*fshanp;
	size_t	 	 fshlen;
	char		*name;
	int		opt;

	if (Progname = strrchr(argv[0], '/')) {
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
		fprintf(stderr, "Can't inititalize the DMAPI\n");
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
				"should be %d bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_get_mountinfo failed, %s\n",
				strerror(errno));
		}
		exit(1);
	}
	fprintf(stdout, "rlenp is %d\n", rlenp);
	print_one_mount_event(bufp);

	dm_handle_free(fshanp, fshlen);
	exit(0);
}
