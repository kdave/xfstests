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

#include <lib/hsm.h>

#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_write_invis().  The
command line is:

	write_invis [-c char] [-o offset] [-l length] [-s sid] \
		[-S storefile] {pathname|handle}

where:
'char' is the character to use as a repeated pattern ('X' is the default),
'offset' is the offset of the start of the write (0 is the default),
'length' is the length of the write in bytes (1 is the default),
'sid' is the session ID whose events you you are interested in.
'pathname' is the name of the file to be written.

DM_WRITE_SYNC is is not supported.

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-c char] [-o offset] [-l length] "
		"[-s sid] [-S storefile] {pathname|handle}\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*object = NULL;
	dm_off_t	offset = 0;
	long long	lltemp;
	dm_size_t	length = 1;
	unsigned long long	ulltemp;
	u_char		ch = 'X';
	void		*bufp = NULL;
	void		*hanp;
	size_t		hlen;
	dm_ssize_t	rc;
	char		*name;
	int		opt;
	char		*storefile = NULL;
	int		storefd;
	int		exit_status = 0;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "c:o:l:s:S:")) != EOF) {
		switch (opt) {
		case 'c':
			ch = *optarg;
			break;
		case 'o':
			sscanf(optarg, "%lld", &lltemp);
			offset = (dm_off_t) offset;
			break;
		case 'l':
			sscanf(optarg, "%llu", &ulltemp);
			length = (dm_size_t) ulltemp;
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 'S':
			storefile = optarg;
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	object = argv[optind];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file's handle. */

	if (opaque_to_handle(object, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n", object);
		exit(1);
	}

	if (length > 0) {
		/* In case it is a realtime file, align the buffer on a
		   sufficiently big boundary.
		*/
		if ((bufp = memalign(4096, length)) == NULL) {
			fprintf(stderr, "malloc of %llu bytes failed\n",
				(unsigned long long) length);
			exit(1);
		}
		memset(bufp, ch, length);
	}

	if (storefile) {
		ssize_t sret;
		size_t len;

		if ((storefd = open(storefile, O_RDONLY)) == -1) {
			fprintf(stderr, "unable to open store file for read (%s), errno = %d\n", storefile, errno);
			exit(1);
		}

		len = length;
		sret = read(storefd, bufp, len);
		if (sret < 0) {
			fprintf(stderr, "unable to read store file (%s), errno = %d\n", storefile, errno);
			exit(1);
		}
		else if (sret != length) {
			fprintf(stderr, "read(%s) returned %lld, expected %lld\n",
				storefile, (long long)sret, (long long)length);
			exit(1);
		}
		close(storefd);
	}

	rc = dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN, 0, offset, length, bufp);

	if (rc < 0) {
		fprintf(stderr, "dm_write_invis failed, %s\n", strerror(errno));
		exit_status++;
	} else if (rc != length) {
		fprintf(stderr, "dm_write_invis expected to write %llu bytes, "
			"actually wrote %lld\n",
			(unsigned long long) length, (long long)rc);
		exit_status++;
	}
	dm_handle_free(hanp, hlen);
	exit(exit_status);
}
