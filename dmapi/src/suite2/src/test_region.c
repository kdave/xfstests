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

#include <sys/types.h>

#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------

Automated test of the DMAPI functions dm_set_region() and dm_get_region().

The command line is:

        test_region [-s sid] ls_path directory 

where 
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


u_int reg_flags[8] = {DM_REGION_NOEVENT,
		      DM_REGION_READ,
		      DM_REGION_WRITE,
		      DM_REGION_TRUNCATE,
		      DM_REGION_READ | DM_REGION_WRITE,
		      DM_REGION_READ | DM_REGION_TRUNCATE,
		      DM_REGION_WRITE | DM_REGION_TRUNCATE,
		      DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE};



static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-s sid] ls_path pathname\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_region_t	region = { 0, 0, 0 };
	dm_region_t	checkregion = { 0, 0, 0 };
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*dir_name = NULL;
	char		*ls_path = NULL;
	char		object[128];
	char		command[128];
	u_int		exactflag;
	u_int		nelem_read = 0;
	void		*hanp;
	size_t	 	 hlen;
	char		*name;
	int		opt;
	int		i;
	int             Vflag = 0;
	dm_token_t	test_token = DM_NO_TOKEN;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "vo:l:s:")) != EOF) {
		switch (opt) {
		case 'v':
			Vflag++;
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
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/***********************************************\
	|* Beginning the testing of set/get_region...  *|
	\***********************************************/

	printf("Region test beginning...\n");
	sprintf(object, "%s/VeryLongUnlikelyFilename.REGIONTEST", dir_name);
	sprintf(command, "cp %s %s \n", ls_path, object); 
	system(command);
	
	/* Get the test file's handle. */
	if (dm_path_to_handle(object, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for file %s\n", object);
		exit(1);
	}
	/* Loop over all possible region flag combinations, 
	 * setting and getting. See what works!  
	 */
	for (i = 0; i < 8; i++) {
		region.rg_flags = reg_flags[i];
	       	if (dm_set_region(sid, hanp, hlen, DM_NO_TOKEN, NELEM, 
				  &region, &exactflag)) {
		  fprintf(stderr, "dm_set_region failed, %s\n",
			  ERR_NAME);
		  continue;
		}
		if (exactflag != DM_TRUE){
		  fprintf(stdout, "oops...exactflag was false!\n");
		}
		if (dm_get_region(sid, hanp, hlen, DM_NO_TOKEN, NELEM,
				  &checkregion, &nelem_read)) {
		  fprintf(stderr, "dm_get_region failed, %s\n",
			  ERR_NAME);
		  continue;
		}
		if (region.rg_flags != checkregion.rg_flags) {
		  fprintf(stdout, "set region flags %d, but found %d\n", 
			  region.rg_flags, checkregion.rg_flags);
		}
		else if (Vflag) {
		  fprintf(stdout, "Test #%d okay\n", i);
		}
	}

	/*************************************\
	|* Correct-input testing complete.   *|
	|* Beginning improper-input testing. *|
	\*************************************/
	printf("\t(errno subtests beginning...)\n");
	region.rg_flags = 7;
	
	/**** SET tests ****/
	/*---------------------------------------------------------*/
	ERRTEST(E2BIG,
		"set", 
		dm_set_region(sid, hanp, hlen, DM_NO_TOKEN, 
			      2, &region, &exactflag))
	ERRTEST(E2BIG,
		"set", 
		dm_set_region(sid, hanp, hlen, DM_NO_TOKEN, 
			      -1, &region, &exactflag))
	/*---------------------------------------------------------*/
	EXCLTEST("set", hanp, hlen, test_token, 
		 dm_set_region(sid, hanp, hlen, test_token,
			       NELEM, &region, &exactflag)) 
	/*---------------------------------------------------------*/
	ERRTEST(EFAULT,
		"set",
		dm_set_region(sid, hanp, hlen, DM_NO_TOKEN,
			     NELEM, (dm_region_t*)(-1000), &exactflag))
	ERRTEST(EFAULT,
		"set",
		dm_set_region(sid, hanp, hlen, DM_NO_TOKEN,
			     NELEM, &region, (dm_boolean_t*)(-1000)))
       	/*---------------------------------------------------------*/
	ERRTEST(EINVAL, 
		"set (bad session id)",
		dm_set_region(-100, hanp, hlen, DM_NO_TOKEN, 
			      NELEM, &region, &exactflag))
       	/*---------------------------------------------------------*/
	
	/**** GET tests ****/
       	/*---------------------------------------------------------*/
	  ERRTEST (E2BIG,
		 "get", 
		 dm_get_region(sid, hanp, hlen, DM_NO_TOKEN,
			       0, &checkregion, &nelem_read))
	/*---------------------------------------------------------*/
	ERRTEST(EFAULT,
		"get (bad handle)",
		dm_get_region(sid, NULL, hlen, DM_NO_TOKEN,
			      NELEM, &checkregion, &nelem_read)) 
	/*---------------------------------------------------------*/
	ERRTEST(EFAULT,
		"get (bad regbufp)",
		dm_get_region(sid, hanp, hlen, DM_NO_TOKEN,
			      NELEM, (dm_region_t *)(-1000), &nelem_read)) 
	/*---------------------------------------------------------*/
	ERRTEST(EFAULT,
		"get (bad nelemp)",
		dm_get_region(sid, hanp, hlen, DM_NO_TOKEN,
			      NELEM, &checkregion, (u_int *)(-1000))) 
	/*---------------------------------------------------------*/
	SHAREDTEST("get", hanp, hlen, test_token, 
		   dm_get_region(sid, hanp, hlen, test_token,
				 NELEM, &checkregion, &nelem_read))
	/*---------------------------------------------------------*/
	ERRTEST(EINVAL, 
		"get", 
		dm_get_region(-100, hanp, hlen, DM_NO_TOKEN,
			      NELEM, &checkregion, &nelem_read))
	/*---------------------------------------------------------*/
	  printf("\t(errno subtests complete)\n");
	/**********************************\
	|* End of improper-input testing. *|
	\**********************************/

	sprintf(command, "rm %s \n", object); 
	system(command);
	printf("Region test complete.\n");
	
	/***********************************\
	|* End of set/get_region testing.  *|
	\***********************************/

	dm_handle_free(hanp, hlen);
	exit(0);
}
