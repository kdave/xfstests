// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

/* Given a file pathname, print the filesystem handle and the uuid_t for
   the filesystem that contains the file.
*/

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <lib/dmport.h>

char *Progname;

static void
hantoa(
        void    *hanp,
        size_t   hlen,
        char    *handle_str)
{
        u_char  *cp= (u_char *)hanp;
        int     i;

        for (i = 0;i < hlen; i++, handle_str += 2)
                sprintf(handle_str, "%.2x", *cp++);
        *handle_str = '\0';
}

int
main(
	int		argc,
	char		**argv)
{
	char		*name;
	int		fd;
	void		*fshanp;
	size_t		fshlen;
	char		buffer[100];

	if (argc != 2) {
		fprintf(stderr, "usage:	%s path\n", argv[0]);
		exit(1);
	}
	Progname = argv[0];
	(void)dm_init_service(&name);

	if (dm_path_to_fshandle(argv[1], &fshanp, &fshlen) != 0) {
		fprintf(stderr, "dm_path_to_fshandle failed, %s\n",
			strerror(errno));
	}
	hantoa(fshanp, fshlen, buffer);

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "Open of %s failed, %s\n", argv[1],
			strerror(errno));
		exit(1);
	}
	fprintf(stdout, "fshandle %s, %s\n",
		buffer, argv[1]);
	exit(0);
}
