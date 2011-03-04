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

Test program used to test the DMAPI function dm_get_dmattr().  The
command line is:

	get_dmattr [-b buflen] [-s sid] [-t token] {pathname|handle} attr

where pathname is the name of a file, buflen is the size of the buffer to use
in the call, attr is the name of the DMAPI attribute, and sid is the session ID
whose attributes you you are interested in.

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
	fprintf(stderr, "usage:\t%s [-b buflen] [-s sid] [-t token] "
		"{pathname|handle} attr\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	token = DM_NO_TOKEN;
	char		*object;
	dm_attrname_t	*attrnamep;
	void		*bufp = NULL;
	size_t		buflen = 10000;
	size_t		rlenp;
	void		*hanp;
	size_t	 	 hlen;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "b:s:t:")) != EOF) {
		switch (opt) {
		case 'b':
			buflen = atol(optarg);
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
	if (optind + 2 != argc)
		usage();
	object = argv[optind++];
	attrnamep = (dm_attrname_t *)argv[optind];

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

	if (buflen > 0) {
		if ((bufp = malloc(buflen)) == NULL) {
			fprintf(stderr, "malloc failed, %s\n", strerror(errno));
			exit(1);
		}
	}

	if (dm_get_dmattr(sid, hanp, hlen, token, attrnamep, buflen,
	    bufp, &rlenp)) {
		if (errno == E2BIG) {
			fprintf(stderr, "dm_get_dmattr buffer too small, "
				"should be %zd bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_get_dmattr failed, %s\n",
				strerror(errno));
		}
		exit(1);
	}
	fprintf(stdout, "rlenp is %zd, value is '%s'\n", rlenp, (char*)bufp);

	dm_handle_free(hanp, hlen);
	exit(0);
}
