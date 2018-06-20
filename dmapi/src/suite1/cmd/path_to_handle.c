// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

/* Given a file object's pathname, print the object's handle. */

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test dm_path_to_handle().  The command line is:

        path_to_handle pathname

where pathname is the name of a file, directory, or symlink.

----------------------------------------------------------------------------*/


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s pathname\n", Progname);
	exit(1);
}


int
main(
	int		argc,
	char		**argv)
{
	char		*pathname;
	char		*name;
	void		*hanp;
	size_t		hlen;
	char		buffer[100];

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 2)
		usage();
	pathname = argv[1];

	(void)dm_init_service(&name);

	if (dm_path_to_handle(pathname, &hanp, &hlen) != 0) {
		fprintf(stderr, "dm_path_to_handle failed, %s\n",
			strerror(errno));
		return(1);
	}
	hantoa(hanp, hlen, buffer);
	fprintf(stdout, "%s\n", buffer);

	dm_handle_free(hanp, hlen);
	return(0);
}
