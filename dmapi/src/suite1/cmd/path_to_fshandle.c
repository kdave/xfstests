/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

/* Given a file object's pathname, print the filesystem handle. */

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test dm_path_to_fshandle().  The command line is:

        path_to_fshandle pathname

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
	void		*fshanp;
	size_t		fshlen;
	char		buffer[100];

	if (Progname = strrchr(argv[0], '/')) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 2)
		usage();
	pathname = argv[1];

	(void)dm_init_service(&name);

	if (dm_path_to_fshandle(pathname, &fshanp, &fshlen) != 0) {
		fprintf(stderr, "dm_path_to_fshandle failed, %s\n",
			strerror(errno));
		return(1);
	}
	hantoa(fshanp, fshlen, buffer);
	fprintf(stdout, "%s\n", buffer);

	dm_handle_free(fshanp, fshlen);
	return(0);
}
