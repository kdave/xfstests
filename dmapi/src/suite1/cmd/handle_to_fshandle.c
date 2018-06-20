// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

/* Given an object's handle, print the filesystem handle. */

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test dm_handle_to_fshandle().  The command line is:

        handle_to_fshandle handle

where handle is an object's handle.

----------------------------------------------------------------------------*/


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s handle\n", Progname);
	exit(1);
}


int
main(
	int		argc,
	char		**argv)
{
	char		*han_str;
	char		*name;
	void		*hanp, *fshanp;
	size_t		hlen, fshlen;
	char		buffer[100];
	int		error;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 2)
		usage();
	han_str = argv[1];

	(void)dm_init_service(&name);

	if ((error = atohan(han_str, &hanp, &hlen)) != 0) {
		fprintf(stderr, "atohan() failed, %s\n", strerror(error));
		return(1);
	}

	if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen) != 0) {
		fprintf(stderr, "dm_handle_to_fshandle failed, %s\n",
			strerror(errno));
		return(1);
	}
	hantoa(fshanp, fshlen, buffer);
	fprintf(stdout, "%s\n", buffer);

	dm_handle_free(hanp, hlen);
	dm_handle_free(fshanp, fshlen);
	return(0);
}
