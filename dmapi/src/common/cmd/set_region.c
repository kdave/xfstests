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

Test program used to test the DMAPI function dm_set_region().  The
command line is:

	set_region [-n nelem] [-o offset] [-l length] [-s sid] {pathname|handle} [event...]

where pathname is the name of a file, nelem is the number of regions to pass
in the call, offset is the offset of the start of
the managed region, and length is the length.  sid is the session ID whose
events you you are interested in, and event is zero or more managed region
events to set.

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;

static	struct	{
	char	*name;
	int	value;
} rg_events[3] = {
	{ "DM_REGION_READ", DM_REGION_READ },
	{ "DM_REGION_WRITE", DM_REGION_WRITE },
	{ "DM_REGION_TRUNCATE", DM_REGION_TRUNCATE }
};
static	int	nevents = sizeof(rg_events)/sizeof(rg_events[0]);


static void
usage(void)
{
	int	i;

	fprintf(stderr, "usage:\t%s [-n nelem] [-o offset] [-l length] "
		"[-s sid] {pathname|handle} [event...]\n", Progname);
	fprintf(stderr, "possible events are:\n");
	for (i = 0; i < nevents; i++)
		fprintf(stderr, "%s (0x%x)\n", rg_events[i].name, rg_events[i].value);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_region_t	region = { 0, 0, 0 };
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*object = NULL;
	u_int		exactflag;
	u_int		nelem = 1;
	void		*hanp;
	size_t	 	 hlen;
	char		*name;
	int		opt;
	int		i;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "n:o:l:s:")) != EOF) {
		switch (opt) {
		case 'n':
			nelem = atol(optarg);
			break;
		case 'o':
			region.rg_offset = atol(optarg);
			break;
		case 'l':
			region.rg_size = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 > argc)
		usage();
	object = argv[optind++];

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

	for (; optind < argc; optind++) {
		if (strspn(argv[optind], "0123456789") == strlen(argv[optind])){
			region.rg_flags |= atol(argv[optind]);
			continue;
		}
		for (i = 0; i < nevents; i++) {
			if (!strcmp(argv[optind], rg_events[i].name))
				break;
		}
		if (i == nevents) {
			fprintf(stderr, "invalid event %s\n", argv[optind]);
			exit(1);
		}
		region.rg_flags |= rg_events[i].value;
	}

	if (dm_set_region(sid, hanp, hlen, DM_NO_TOKEN, nelem, &region,
	    &exactflag)) {
		fprintf(stderr, "dm_set_region failed, %s\n",
			strerror(errno));
		exit(1);
	}
	fprintf(stdout, "exactflag is %s\n",
		exactflag == DM_TRUE ? "True" : "False");
	dm_handle_free(hanp, hlen);
	exit(0);
}
