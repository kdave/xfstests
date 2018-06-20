// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test dm_init_service().  The command line is:

        init_service

----------------------------------------------------------------------------*/


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s\n", Progname);
	exit(1);
}


int
main(
	int		argc,
	char		**argv)
{
	char		*name;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 1)
		usage();

	(void)dm_init_service(&name);
	fprintf(stdout, "%s\n", name);

	return(0);
}
