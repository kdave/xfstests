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

Test program used to test the DMAPI function dm_respond_event().  The
command line is:

	respond_event sid token response reterror

where sid is the session ID whose event you are responding to.

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
	fprintf(stderr, "usage:\t%s sid token response reterror\n",
		Progname);
	fprintf(stderr, "  Response values:\n");
	fprintf(stderr, "  %d = DM_RESP_INVALID\n", DM_RESP_INVALID);
	fprintf(stderr, "  %d = DM_RESP_CONTINUE\n", DM_RESP_CONTINUE );
	fprintf(stderr, "  %d = DM_RESP_ABORT\n", DM_RESP_ABORT);
	fprintf(stderr, "  %d = DM_RESP_DONTCARE\n", DM_RESP_DONTCARE);
	exit(1);
}


int
main(
	int		argc, 
	char		**argv)
{
	dm_sessid_t	sid;
	char		*name;
	dm_token_t	token;
	dm_response_t	response;
	int		reterror;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 5)
		usage();

	sid = atol(argv[1]);
	token = atol(argv[2]);
	response = (dm_response_t)atoi(argv[3]);
	reterror = atol(argv[4]);

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}

	if (dm_respond_event(sid, token, response, reterror, 0, NULL)) {
		fprintf(stderr, "dm_respond_event failed, %d/%s\n",
			errno, strerror(errno));
		exit(1);
	}
	exit(0);
}
