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
