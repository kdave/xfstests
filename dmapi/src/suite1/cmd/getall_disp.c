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
#include <getopt.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_getall_disp().  The
command line is:

	getall_disp [-b buflen] sid

where buflen is the size of the buffer to use, and sid is the session ID
whose dispositions you you are interested in.

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
	fprintf(stderr, "usage:\t%s [-b buflen] sid\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_dispinfo_t	*disp;
	dm_sessid_t	sid;
	void		*bufp = NULL;
	size_t		buflen = 10000;
	void		*hanp;
	size_t		hlen;
	char		hans1[HANDLE_STR];
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
	if (optind + 1 != argc)
		usage();
	sid = atol(argv[optind]);

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}

	if (buflen > 0) {
		if ((bufp = malloc(buflen)) == NULL) {
			fprintf(stderr, "malloc failed, %s\n", strerror(errno));
			exit(1);
		}
	}

	if (dm_getall_disp(sid, buflen, bufp, &rlenp)) {
		if (errno == E2BIG) {
			fprintf(stderr, "dm_getall_disp buffer too small, "
				"should be %zd bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_getall_disp failed, %s\n",
				strerror(errno));
		}
		exit(1);
	}
	fprintf(stdout, "rlenp is %zd\n", rlenp);
	if (rlenp == 0)
		return(0);

	disp = bufp;
	while (disp != NULL) {
		hanp = DM_GET_VALUE(disp, di_fshandle, void *);
		hlen = DM_GET_LEN(disp, di_fshandle);
		if (hanp && hlen) {
			hantoa(hanp, hlen, hans1);
		} else {
			sprintf(hans1, "<BAD HANDLE, hlen %zd>", hlen);
		}
		printf("%-15s %s dm_eventset_t 0%llo\n",
			"fshandle", hans1,
			(unsigned long long) disp->di_eventset);

		disp = DM_STEP_TO_NEXT(disp, dm_dispinfo_t *);
	}
	return(0);
}
