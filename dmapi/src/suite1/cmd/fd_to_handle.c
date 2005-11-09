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

/* Given a file object's pathname, print the object's handle. */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <lib/dmport.h>

#include <string.h>

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
	void		*hanp;
	size_t		hlen;
	char		buffer[100];
	int		fd;

	if (argc != 2) {
		fprintf(stderr, "usage:	%s path\n", argv[0]);
		exit(1);
	}
	Progname = argv[0];

	(void)dm_init_service(&name);

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "open of %s failed, %s\n", argv[1],
			strerror(errno));
		exit(1);
	}
	if (dm_fd_to_handle(fd, &hanp, &hlen) != 0) {
		fprintf(stderr, "dm_fd_to_handle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	hantoa(hanp, hlen, buffer);

	fprintf(stdout, "handle %s, path %s\n", buffer, argv[1]);
	exit(0);
}
