/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <lib/hsm.h>

#ifdef linux
#include <string.h>
#endif

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
	int	i;

	fprintf(stderr, "usage:\t%s sid token response reterror\n",
		Progname);
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

	if (Progname = strrchr(argv[0], '/')) {
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
		fprintf(stderr, "Can't inititalize the DMAPI\n");
		exit(1);
	}

	if (dm_respond_event(sid, token, response, reterror, 0, NULL)) {
		fprintf(stderr, "dm_respond_event failed, %d/%s\n",
			errno, strerror(errno));
		exit(1);
	}
	exit(0);
}
