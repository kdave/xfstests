// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <lib/hsm.h>

#include <string.h>
#include <getopt.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function create_userevent().  The
command line is:

	create_userevent [-s sid] string

where string is the msgdata to be stored in the event.
sid is the session ID to use for the event.

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
	fprintf(stderr, "usage:\t%s [-s sid] string\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*string;
	dm_token_t	token;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "s:")) != EOF) {
		switch (opt) {
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	string = argv[optind++];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	if (dm_create_userevent(sid, strlen(string)+ 1, string, &token)) {
		fprintf(stderr, "dm_create_userevent failed, %s\n",
			strerror(errno));
		exit(1);
	}

	fprintf(stdout, "New token %d\n", token);
	exit(0);
}
