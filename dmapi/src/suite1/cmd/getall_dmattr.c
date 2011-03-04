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

Test program used to test the DMAPI function dm_getall_dmattr().  The
command line is:

	getall_dmattr [-b buflen] [-s sid] pathname

where pathname is the name of a file, buflen is the size of the buffer to use
in the call, and sid is the session ID whose attributes you are interested in.

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
	fprintf(stderr, "usage:\t%s [-b buflen] [-s sid] pathname\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*pathname;
	void		*bufp;
	size_t		buflen = 10000;
	size_t		rlenp;
	void		*hanp;
	size_t		hlen;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "b:s:")) != EOF) {
		switch (opt) {
		case 'b':
			buflen = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	pathname = argv[optind++];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file's handle. */

	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for file %s, %s\n",
			pathname, strerror(errno));
		exit(1);
	}

	if ((bufp = malloc(buflen == 0 ? 1 : buflen)) == NULL) {
		fprintf(stderr, "malloc failed, %s\n", strerror(errno));
		exit(1);
	}

	if (dm_getall_dmattr(sid, hanp, hlen, DM_NO_TOKEN, buflen,
	    bufp, &rlenp)) {
		if (errno == E2BIG) {
			fprintf(stderr, "dm_getall_dmattr buffer too small, "
				"should be %zd bytes\n", rlenp);
		} else {
			fprintf(stderr, "dm_getall_dmattr failed, %s\n",
				strerror(errno));
		}
		exit(1);
	}
	fprintf(stdout, "rlenp is %zd\n", rlenp);
	if (rlenp > 0) {
		dm_attrlist_t	*attrlist;

		fprintf(stdout, "DMAPI attributes are:\n");

		attrlist = (dm_attrlist_t *)bufp;
		while (attrlist != NULL) {
			fprintf(stdout, "Name: %s, length %d, value '%s'\n",
				attrlist->al_name.an_chars,
				DM_GET_LEN(attrlist, al_data),
				DM_GET_VALUE(attrlist, al_data, char *));

			attrlist = DM_STEP_TO_NEXT(attrlist, dm_attrlist_t *);
		}
	}

	dm_handle_free(hanp, hlen);
	exit(0);
}
