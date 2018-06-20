// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_pending().  The
command line is:

	pending sid token

where sid is the session ID whose event you are responding to.

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
	fprintf(stderr, "usage:\t%s sid token\n",
		Progname);
	exit(1);
}


int
main(
	int		argc, 
	char		**argv)
{
	dm_sessid_t	sid;
	char		*name;
	dm_token_t	token;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 3)
		usage();

	sid = atol(argv[1]);
	token = atol(argv[2]);

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}

	if (dm_pending(sid, token, NULL)) {
		fprintf(stderr, "dm_pending failed, %s\n", strerror(errno));
		exit(1);
	}
	exit(0);
}
