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

Test program used to test the DMAPI function dm_set_dmattr().  The
command line is:

	set_dmattr [-b buflen] [-s sid] [-u] {pathname|handle} attr value

where pathname is the name of a file, buflen is the size of the buffer to use
in the call, attr is the name of the DMAPI attribute, -u is selected if 
setdtime should be updated, and sid is the session ID whose attributes you
are interested in.

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
	fprintf(stderr, "usage:\t%s [-b buflen] [-s sid] [-u] "
		"{pathname|handle} attr value\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*object;
	dm_attrname_t	*attrnamep;
	char		*bufp;
	size_t		buflen = 0;
	int		bflag = 0;
	int		setdtime = 0;
	void		*hanp;
	size_t	 	hlen;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "b:s:u")) != EOF) {
		switch (opt) {
		case 'b':
			bflag++;
			buflen = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 'u':
			setdtime = 1;
			break;
		case '?':
			usage();
		}
	}
	if (optind + 3 != argc)
		usage();
	object = argv[optind++];
	attrnamep = (dm_attrname_t *)argv[optind++];
	bufp = argv[optind];
	if (!bflag)
		buflen = strlen(bufp) + 1;

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

	if (dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN, attrnamep, setdtime,
	    buflen, bufp)) {
		fprintf(stderr, "dm_set_dmattr failed, %s\n",
			strerror(errno));
		exit(1);
	}

	dm_handle_free(hanp, hlen);
	exit(0);
}
