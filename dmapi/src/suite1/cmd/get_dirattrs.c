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
#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_get_dirattrs().  The
command line is:

	get_dirattrs [-b buflen] [-l loc] [-s sid] dirpath

where dirpath is the name of a directory, buflen is the size of the buffer
to use in the call, loc is a starting location, and sid is the session ID
whose attributes you are interested in.

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
	fprintf(stderr, "usage:\t%s [-b buflen] [-l loc] [-s sid] [-1] [-q] dirpath\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_attrloc_t	loc = 0;
	char		*dirpath;
	char		buffer[100];
	void		*bufp;
	size_t		buflen = 10000;
	u_int		mask;
	size_t		rlenp;
	void		*hanp;
	size_t		hlen;
	char		*name;
	int		opt;
	int		ret;
	int		oneline = 0;
	int		quiet = 0;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "b:l:s:1q")) != EOF) {
		switch (opt) {
		case 'b':
			buflen = atol(optarg);
			break;
		case 'l':
			loc = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '1':
			oneline = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	dirpath = argv[optind++];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the directory's handle. */

	if (dm_path_to_handle(dirpath, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for file %s, %s\n",
			dirpath, strerror(errno));
		exit(1);
	}

	if ((bufp = malloc(buflen == 0 ? 1 : buflen)) == NULL) {
		fprintf(stderr, "malloc failed, %s\n", strerror(errno));
		exit(1);
	}

	mask = DM_AT_HANDLE|DM_AT_EMASK|DM_AT_PMANR|DM_AT_PATTR|DM_AT_DTIME|DM_AT_CFLAG|DM_AT_STAT;

	do {
		memset(bufp, 0, buflen);
		if ((ret = dm_get_dirattrs(sid, hanp, hlen, DM_NO_TOKEN, mask,
				&loc, buflen, bufp, &rlenp)) < 0) {
			fprintf(stderr, "dm_get_dirattrs failed, %s\n",
				strerror(errno));
			exit(1);
		}
		if (!quiet) {
			fprintf(stdout, "ret = %d, rlenp is %zd, loc is %lld\n",
				ret, rlenp, (long long) loc);
		}
		if (rlenp > 0) {
			dm_stat_t	*statp;

			statp = (dm_stat_t *)bufp;
			while (statp != NULL) {

				hantoa((char *)statp + statp->dt_handle.vd_offset,
					statp->dt_handle.vd_length, buffer);
				if (oneline) {
					fprintf(stdout, "%s %s\n",
						(char *)statp + statp->dt_compname.vd_offset,
						buffer);
				}
				else {
					fprintf(stdout, "handle %s\n", buffer);
					fprintf(stdout, "name %s\n",
						(char *)statp + statp->dt_compname.vd_offset);
					print_line(statp);
				}

				statp = DM_STEP_TO_NEXT(statp, dm_stat_t *);
			}
		}
		else if ((ret == 1) && (rlenp == 0) && (!quiet)) {
			fprintf(stderr, "buflen is too short to hold anything\n");
			exit(1);
		}
	} while (ret != 0);

	dm_handle_free(hanp, hlen);
	exit(0);
}
