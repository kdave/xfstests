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

#include <sys/types.h>

#include <lib/hsm.h>
#include <lib/errtest.h>

#ifdef linux
#include <string.h>
#endif

/*---------------------------------------------------------------------------

Manual test for the DMAPI functions dm_set_region() and dm_get_region().

The command line is:

        region_test [-Rv] [-s sid] [-l len] [-o offset] ls_path directory 

where 
   -R
	reuse the existing test file
   pathname
      is the name of a file
   ls_path
      is the path to a specific copy of ls, important only for its size
   sid
      is the session ID whose events you you are interested in.
----------------------------------------------------------------------------*/

#define NELEM   1

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

	fprintf(stderr, "usage:\t%s [-Rv] [-s sid] [-l len] [-o offset] ls_path pathname\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*dir_name = NULL;
	char		*ls_path = NULL;
	char		command[1024];
	char		test_file[128];
	int		opt;
	int             Vflag = 0;
	dm_region_t	region = { 0, 0, 0 };
	char		*name;
	int		reuse_file = 0;
	void		*hanp;
	size_t	 	 hlen;
	dm_boolean_t	exactflag;

	if (Progname = strrchr(argv[0], '/')) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "vo:l:s:R")) != EOF) {
		switch (opt) {
		case 'v':
			Vflag++;
			break;
		case 'R':
			reuse_file++;
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
	if (optind + 2 > argc)
		usage();
	ls_path = argv[optind];
	dir_name = argv[optind+1];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't inititalize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	sprintf(test_file, "%s/DMAPI_test_file", dir_name);
	if( !reuse_file ){
		sprintf(command, "cp %s %s\n", ls_path, test_file); 
		system(command);
	}

	if (dm_path_to_handle(test_file, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for file %s\n", test_file);
		exit(1);
	}

	region.rg_flags = DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE;
	if( dm_set_region( sid, hanp, hlen, DM_NO_TOKEN, NELEM,
			  &region, &exactflag ) ){
		fprintf(stderr, "dm_set_region failed, err=%d\n", errno);
		dm_handle_free(hanp,hlen);
		exit(1);
	}
	if( exactflag == DM_FALSE )
		fprintf(stderr, "exact flag was false\n");

	dm_handle_free(hanp, hlen);
	exit(0);
}
