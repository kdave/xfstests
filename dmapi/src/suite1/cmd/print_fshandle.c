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

/* Given a file pathname, print the filesystem handle and the uuid_t for
   the filesystem that contains the file.
*/

#include <sys/types.h>
#include <string.h>
#ifdef __sgi
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>
#endif

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
#ifdef __sgi
	xfs_fsop_geom_t	geom;
	char		*uuid_str;
	u_int		status;
#endif
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

#ifdef __sgi
	syssgi(SGI_XFS_FSOPERATIONS, fd, XFS_FS_GEOMETRY, NULL, &geom);

	uuid_to_string(&geom.uuid, &uuid_str, &status);

	fprintf(stdout, "fshandle %s, uuid %s, %s\n",
		buffer, uuid_str, argv[1]);
#endif
	fprintf(stdout, "fshandle %s, %s\n",
		buffer, argv[1]);
	exit(0);
}
