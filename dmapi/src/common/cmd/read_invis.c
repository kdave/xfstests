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

#include <ctype.h>

#include <lib/hsm.h>

#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_read_invis().  The
command line is:

	read_invis [-o offset] [-l length] [-s sid] [-c char] \
		[-S storefile] {pathname|handle}

where:
'offset' is the offset of the start of the write (0 is the default),
'length' is the length of the write in bytes (1 is the default),
'sid' is the session ID whose events you you are interested in.
'pathname' is the name of the file to be written.
'char' is ignored--it just allows read_invis and write_invis to have
    interchangeable commandlines without having to fuss with the params.

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
	fprintf(stderr, "usage:\t%s [-o offset] [-l length] "
		"[-s sid] [-c char] "
		"[-S storefile] {pathname|handle}\n", Progname);
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
	char		*bufp = NULL;
	void		*hanp;
	size_t		hlen;
	dm_ssize_t	rc;
	char		*name;
	int		opt;
	int		i;
	char		*storefile = NULL;
	int		storefd = 0;
	int		exit_status = 0;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "o:l:s:c:S:")) != EOF) {
		switch (opt) {
		case 'o':
			sscanf(optarg, "%lld", &lltemp);
			offset = (dm_off_t) lltemp;
			break;
		case 'l':
			sscanf(optarg, "%llu", &ulltemp);
			length = (dm_size_t) ulltemp;
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 'c':
			/* This is a no-op, it just allows read_invis
			 * and write_invis to have interchangeable
			 * commandlines, without having to fuss with
			 * the params.
			 */
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
		memset(bufp, '\0', length);
	}

	if (storefile) {
		off_t	lret;

		if ((storefd = open(storefile, O_WRONLY|O_CREAT, 0777)) == -1) {
			fprintf(stderr, "unable to open store file for write (%s), errno = %d\n", storefile, errno);
			exit(1);
		}
		lret = lseek(storefd, offset, SEEK_SET);
		if (lret < 0) {
			fprintf(stderr, "unable to lseek(%s) to offset %lld, errno = %d\n",
				storefile, (long long)lret, errno);
			exit(1);
		}
	}

	rc = dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN, offset, length, bufp);

	if (rc < 0) {
		fprintf(stderr, "dm_read_invis failed, %s\n", strerror(errno));
		exit_status++;
	} else if (rc != length) {
		fprintf(stderr, "dm_read_invis expected to read %llu bytes, actually "
			"read %lld\n", (unsigned long long) length,
			(long long) rc);
		exit_status++;
	}

	if (storefile) {
		ssize_t sret;
		sret = write(storefd, bufp, rc);
		if (sret < 0) {
			fprintf(stderr, "unable to write to store file (%s), errno = %d\n", storefile, errno);
			exit_status++;
		}
		else if (sret != rc) {
			fprintf(stderr, "write(%s) returned %lld, expected %lld\n",
				storefile, (long long)sret, (long long)rc);
			exit_status++;
		}
		close(storefd);
	}
	else {
		for (i = 0; i < rc; i++) {
			if (isprint(bufp[i])) {
				fprintf(stdout, "%c", bufp[i]);
			} else {
				fprintf(stdout, "\\%03d", bufp[i]);
			}
		}
	}
	dm_handle_free(hanp, hlen);
	exit(exit_status);
}
