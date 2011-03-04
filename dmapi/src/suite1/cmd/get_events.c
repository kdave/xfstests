/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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

Test program used to test the DMAPI function dm_get_events().  The
command line is:

	get_events [-b buflen] [-m maxmsgs] [-f] sid

where buflen is the size of the buffer to use, maxmsgs is the number of messages
to read, -f, if selected, is DM_EV_WAIT, and sid is the session ID
whose dispositions you are interested in.

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
	fprintf(stderr, "usage:\t%s [-b buflen] [-m maxmsgs] [-f] sid\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_eventmsg_t	*msg;
	dm_sessid_t	sid;
	u_int		flags = 0;
	void		*bufp = NULL;
	size_t		buflen = 10000;
	u_int		maxmsgs = 1;
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

	while ((opt = getopt(argc, argv, "b:m:f")) != EOF) {
		switch (opt) {
		case 'b':
			buflen = atol(optarg);
			break;
		case 'm':
			maxmsgs = atol(optarg);
			break;
		case 'f':
			flags = DM_EV_WAIT;
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

	if (dm_get_events(sid, maxmsgs, flags, buflen, bufp, &rlenp)) {
		if (errno == E2BIG) {
			fprintf(stderr, "dm_get_events buffer too small, "
				"should be %zd bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_get_events failed, (%d)%s\n",
				errno, strerror(errno));
		}
		exit(1);
	}
	fprintf(stdout, "rlenp=%zd\n", rlenp);

	if (rlenp == 0)
		return(0);

	msg = bufp;
	while (msg != NULL) {
		print_one_message(msg);
		msg = DM_STEP_TO_NEXT(msg, dm_eventmsg_t *);
	}
	return(0);
}
