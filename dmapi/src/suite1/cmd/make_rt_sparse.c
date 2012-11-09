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

/*
 *	Create file with XFS_XFLAG_REALTIME set.
 */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>


/* Note: In order to figure out the filesystem geometry, you have to run this
   program as root.  Creating the file itself can be done by anyone.
*/


static	char *	prog;

static void
Usage(void)
{
	fprintf(stderr,"Usage: %s filename\n", prog);
	exit(1);
}


int
main(
	int	argc,
	char	**argv)
{
	xfs_fsop_geom_t	geom;
	struct	fsxattr	fsx;
	struct	dioattr	dio;
	char	*pathname;
	u_int	buflen;
	char	*buf;
	ssize_t	offset;
	ssize_t	count;
	int	fd;
	int	i;

	if (prog = strrchr(argv[0], '/')) {
		*prog++;
	} else {
		prog = argv[0];
	}

	if (argc != 2)
		Usage();
	pathname = argv[1];

	/* Create the file. */

	if ((fd = open(pathname, O_RDWR|O_CREAT|O_EXCL|O_DIRECT, 0600)) < 0) {
		fprintf(stderr,"%s: Cannot open %s, %s\n", prog,
			pathname, strerror(errno));
		exit(1);
	}

	/* Determine the filesystem's realtime partition geometry. */

	if (syssgi(SGI_XFS_FSOPERATIONS, fd, XFS_FS_GEOMETRY, NULL, &geom)) {
		fprintf(stderr,"%s: syssgi(,XFS_FS_GEOMETRY) failed, %s\n",
			prog, strerror(errno));
		exit(1);
	}

	/* Make the file a realtime file. */

	fsx.fsx_xflags = 1; /*XFS_XFLAG_REALTIME*/
	fsx.fsx_extsize = 4 * geom.blocksize * geom.rtextsize;
	if (fcntl(fd, F_FSSETXATTR, &fsx) < 0) {
		fprintf(stderr,"%s: fcntl(,F_FSSETXATTR) failed, %s\n", prog,
			strerror(errno));
		exit(1);
	}

	/* Obtain the direct I/O parameters. */

	if (fcntl(fd, F_DIOINFO, &dio) < 0) {
		fprintf(stderr,"%s: fcntl(,F_DIOINFO) failed,%s\n",
			prog, strerror(errno));
		exit(1);
	}
	fprintf(stdout, "%s: file %s direct io requirements.\n", prog,
		pathname);
	fprintf(stdout, "%7d memory alignment.\n", dio.d_mem);
	fprintf(stdout, "%7d minimum io size.\n", dio.d_miniosz);
	fprintf(stdout, "%7d maximum io size.\n", dio.d_maxiosz);

	if (fcntl(fd, F_FSGETXATTR, &fsx) < 0) {
		fprintf(stderr,"%s: fcntl(,F_FSGETXATTR) failed, %s\n", prog,
			strerror(errno));
		exit(1);
	}
	fprintf(stdout, "%7d realtime extent size.\n", fsx.fsx_extsize);

	/* Malloc and zero a buffer to use for writes. */

	buflen = dio.d_miniosz;
	if ((buf = memalign(dio.d_mem, buflen)) == NULL) {
		fprintf(stderr,"%s: memalign(%d,%d) returned NULL\n",
				prog, dio.d_mem, buflen);
		exit(1);
	}
	memset(buf, '\0', buflen);

	for (i = 0; i < 10; i += 2) {
		offset = i * fsx.fsx_extsize;
		if (lseek(fd, offset, SEEK_SET) < 0) {
			fprintf(stderr, "seek to %d failed, %s\n", offset,
				strerror(errno));
			exit(1);
		}
		if ((count = write(fd, buf, buflen)) < 0) {
			fprintf(stderr, "write of %d bytes failed at offset "
				"%d, , %s\n", buflen, offset, strerror(errno));
			exit(1);
		}
		if (count != buflen) {
			fprintf(stderr, "expected to write %d bytes at offset "
				"%d, actually wrote %d\n", buflen, offset,
				count);
			exit(1);
		}
	}
	exit(0);
}
