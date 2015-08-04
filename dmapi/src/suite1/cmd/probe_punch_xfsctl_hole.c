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
#include <getopt.h>
#include <string.h>
#include <fcntl.h>



/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_probe_hole().  The
command line is:

	probe_hole [-o offset] [-l length] [-s sid] pathname

where pathname is the name of a file, offset is the offset of the start of
the proposed punch, and length is the length of the punch.  sid is the
session ID whose events you you are interested in.

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;

#define METHOD_DMAPI_PROBE	0
#define METHOD_DMAPI_PUNCH	1
#define METHOD_XFSCTL		2

char *methodmap[] = {"DMAPI probe hole",
		     "DMAPI punch hole",
		     "Punch hole with xfsctl(XFS_IOC_FREESP64)"};

static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-x|-p] [-o offset] [-l length] "
		"[-s sid] pathname\n", Progname);
	exit(1);
}

int
xfsctl_punch_hole(
		  char		*path,
		  dm_off_t	offset,
		  dm_size_t	length)
{
	xfs_flock64_t   flock;
	int		fd;
	
	if ((fd = open(path, O_RDWR|O_CREAT, 0600)) < 0) {
		perror(path);
		exit(errno);
	}

	flock.l_whence = 0;
	flock.l_start = offset;
	flock.l_len = length; 

	if (xfsctl(path, fd, XFS_IOC_FREESP64, &flock)) {
		fprintf(stderr, "can't XFS_IOC_FREESP64 %s: %s\n",
			path, strerror(errno));
		return errno;
	}
	
	close(fd);

	printf("ok.\n");
	return 0;
}

int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*pathname = NULL;
	dm_off_t	offset = 0;
	dm_size_t	length = 0;
	dm_off_t	roffp;
	dm_size_t	rlenp;
	void		*hanp;
	size_t	 	hlen;
	char		*name;
	char            *p;
	int		opt;
	int		method = METHOD_DMAPI_PROBE;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "o:l:s:xp")) != EOF) {
		switch (opt) {
		case 'o':
		  	offset = strtoll(optarg, &p, 10);
			if (p && (*p == 'k' || *p == 'K'))
				offset *= 1024;
			break;
		case 'l':
			length = strtoll(optarg, &p, 10);
			if (p && (*p == 'k' || *p == 'K'))
				length *= 1024;
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 'x':
			method = METHOD_XFSCTL;
			break;
		case 'p':
			method = METHOD_DMAPI_PUNCH;
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	pathname = argv[optind];

	if (!pathname)
		usage();

	printf("Running %s on %s with settings:\n", methodmap[method], pathname);
	printf("  offset = '%lld', length = '%lld', sid = '%lld'\n",
		(long long) offset, (unsigned long long) length, (long long) sid);
	
	if (method ==  METHOD_XFSCTL) 
		return xfsctl_punch_hole(pathname, offset, length);
	
	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);
	
	/* Get the file's handle. */
	
	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for file %s\n", pathname);
		exit(1);
	}
	
	switch(method) {
	case METHOD_DMAPI_PROBE:
		if (dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, offset, length,
				  &roffp, &rlenp)) {
			fprintf(stderr, "dm_probe_hole failed, %s\n",
				strerror(errno));
			exit(1);
		}
		fprintf(stdout, "roffp is %lld, rlenp is %llu\n",
			(long long) roffp, (unsigned long long) rlenp);
		break;
	case METHOD_DMAPI_PUNCH:
		if (dm_punch_hole(sid, hanp, hlen, DM_NO_TOKEN, offset, length)) {
			fprintf(stderr, "dm_punch_hole failed, %s\n",
				strerror(errno));
			exit(1);
		}
		break;			
	}
	dm_handle_free(hanp, hlen);
	
	return 0;
}
