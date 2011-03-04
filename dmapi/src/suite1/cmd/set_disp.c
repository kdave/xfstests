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

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_set_eventlist().  The
command line is:

	set_disp [-m max] [-s sid] [-t token] {pathname|handle} [event...]
	set_disp [-m max] [-s sid] -G [DM_EVENT_MOUNT]

where
{pathname|handle}
	is the name of a file or handle whose filesystem is of interest.
-G
	if the global handle should be used
max
	is the value to use for the maxevent parameter of dm_set_disp(),
sid
	is the dm_sessid_t to use rather than the default test session.
token
	is the dm_token_t to use in place of DM_NO_TOKEN.
event
	is zero or more events to set.

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
	int	i;

	fprintf(stderr, "usage:\t%s [-m max] [-s sid] [-t token] "
		"{pathname|handle} [event...]\n", Progname);
	fprintf(stderr, "usage:\t%s [-m max] [-s sid] -G [DM_EVENT_MOUNT]\n",
		Progname);
	fprintf(stderr, "possible events are:\n");
	for (i = 0; i < ev_namecnt; i++) {
		fprintf(stderr, "%s (%d)\n", ev_names[i].name,
			ev_names[i].value);
	}
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	token = DM_NO_TOKEN;
	u_int		maxevent = DM_EVENT_MAX;
	dm_eventset_t	eventset;
	void		*fshanp;
	size_t	 	fshlen;
	int		Gflag = 0;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "Gm:s:t:")) != EOF) {
		switch (opt) {
		case 'G':
			Gflag++;
			break;
		case 'm':
			maxevent = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 't':
			token = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (Gflag && token != DM_NO_TOKEN)
		usage();
	if (optind + (Gflag ? 0 : 1) > argc)
		usage();

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	DMEV_ZERO(eventset);

	/* Get the file or filesystem's handle. */

	if (Gflag) {
		fshanp = DM_GLOBAL_HANP;
		fshlen = DM_GLOBAL_HLEN;
	} else {
		char	*object;
		void	*hanp;
		size_t	hlen;

		object = argv[optind++];

		if (opaque_to_handle(object, &hanp, &hlen)) {
			fprintf(stderr, "can't get handle from %s\n", object);
			exit(1);
		}
		if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen)) {
			fprintf(stderr, "can't get filesystem handle from %s\n",
				object);
			exit(1);
		}
		dm_handle_free(hanp, hlen);
	}

	for (; optind < argc; optind++) {
		dm_eventtype_t	event;

		event = ev_name_to_value(argv[optind]);
		if (event == DM_EVENT_INVALID) {
			fprintf(stderr, "invalid event %s\n", argv[optind]);
			usage();
		}
		DMEV_SET(event, eventset);
	}

	if (dm_set_disp(sid, fshanp, fshlen, token, &eventset, maxevent)) {
		fprintf(stderr, "dm_set_disp failed, %s\n",
			strerror(errno));
		exit(1);
	}

	if (!Gflag)
		dm_handle_free(fshanp, fshlen);
	exit(0);
}
