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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/dmport.h>
#include <lib/hsm.h>

#include <getopt.h>
#ifdef linux
#include <xfs/xfs_fs.h>
#include <xfs/handle.h>
#endif

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_handle_to_path().  The
command line is:

        handle_to_path [-b buflen] {dirpath|dirhandle} {objpath|objhandle}

There are two parameters.  The first is the pathname of a directory,
and the second is the pathname of a file, directory, or symbolic link
within that directory.  The second parameter can also be the same as
the first if you want to specify "." (this is how EMASS uses it).
Pathnames can either be relative or full.

buflen is the size of the buffer to use in the call.

This program will return the full pathname of the object which is the
second parameter using the dm_handle_to_path() function.

The program should work successfully for files, directories, and
symbolic links, and does not have to work for any other type of
object.  It doesn't have to work across mount points.  There shouldn't
be any "/." crud on the end of the returned pathname either.

----------------------------------------------------------------------------*/

extern	int	optind;
extern	char	*optarg;


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-b buflen] {dirpath|dirhandle} {objpath|objhandle}\n", Progname);
	exit(1);
}


int
main(
	int		argc,
	char		**argv)
{
	char		*dirpath;
	char		*objpath;
	void		*hanp1, *hanp2;
	size_t		hlen1, hlen2;
	void		*pathbufp;
	size_t		buflen = 1024;
	size_t		rlenp;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "b:")) != EOF) {
		switch (opt) {
		case 'b':
			buflen = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 2 != argc)
		usage();
	dirpath = argv[optind++];
	objpath = argv[optind];

	if (dm_init_service(&name)) {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		return(1);
	}

	if (opaque_to_handle(dirpath, &hanp1, &hlen1)) {
		fprintf(stderr, "can't get handle for dir %s\n", dirpath);
		exit(1);
	}

	if (opaque_to_handle(objpath, &hanp2, &hlen2)) {
		fprintf(stderr, "can't get handle for obj %s\n", objpath);
		exit(1);
	}

	if ((pathbufp = malloc(buflen == 0 ? 1 : buflen)) == NULL) {
		fprintf(stderr, "malloc failed\n");
		return(1);
	}

	if (dm_handle_to_path(hanp1, hlen1, hanp2, hlen2,
	    buflen, pathbufp, &rlenp)) {
		if (errno == E2BIG) {
			fprintf(stderr, "dm_handle_to_path buffer too small, "
				"should be %zd bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_handle_to_path failed, (%d) %s\n",
				errno, strerror(errno));
		}
		return(1);
	}
	fprintf(stderr, "rlenp is %zd, pathbufp is %s\n", rlenp, (char*)pathbufp);
	if (strlen(pathbufp) + 1 != rlenp) {
		fprintf(stderr, "rlenp is %zd, should be %zd\n", rlenp,
			strlen(pathbufp) + 1);
		return(1);
	}
	exit(0);
}
