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
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <lib/dmport.h>
#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------
Automated search for EFAULT in the following DMAPI commands:
   dm_get_allocinfo
   dm_get_config
   dm_get_config_events
   dm_getall_dmattr
   dm_init_attrloc

There are other EFAULT tests, in the programs that test individual 
DMAPI functions.  Those other tests are referenced in comments in this source.

The command line is:
   test_efault [-s sid] [-v] ls_path pathname

where:
   sid
      is the session ID whose events you you are interested in.
   ls_path
      is the path to a copy of ls, which will be copied as a test file.
   pathname
      is the filesystem to use for the test.
----------------------------------------------------------------------------*/

extern	int	optind;
extern	int	opterr;
extern	char	*optarg;

char	*Progname;
int     Vflag=0;

static void
usage(void)
{
	fprintf(stderr, 
		"Usage: %s [-v] [-s sid] ls_path pathname\n",
		Progname);
	exit(1);
}


int
main(int argc, char **argv) {
     
        dm_sessid_t	sid = DM_NO_SESSION;
	void		*hanp;
	size_t		hlen;
	char		*name;
	char		*ls_path;
	char		*pathname;
	char            test_file[100];
	char            command[100];
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	opterr = 0;
	while ((opt = getopt(argc, argv, "vn:s:")) != EOF) {
		switch (opt) {
		case 'v':
		         Vflag++;
			 break;
		case 's': 
		         sid = atol(optarg);
			 break;
		case '?':
		         usage();
		}
	}
	if (optind + 2 != argc) {
		usage();
	}
	ls_path = argv[optind];
	pathname = argv[optind+1];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);
	
	printf("Beginning EFAULT testing...\n");

	/*----------------------------------*\
	|*  ## Traditional errno tests  ##  *|
	\*----------------------------------*/
	sprintf(test_file, "%s/DMAPI_EFAULT_test_file", pathname);
	sprintf(command, "cp %s %s\n", ls_path, test_file); 
	system(command);
	
	if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  test_file, ERR_NAME);
	  goto abort_test;
	}


	/*---------------------------------------------------------
	 * get_allocinfo
	 *---------------------------------------------------------
	 */
	{ dm_off_t     off=0;
	  u_int        nelem=1;
	  dm_extent_t  extent;
	  u_int        nelem_ret;
	  
	  ERRTEST(EFAULT, "get_allocinfo (bad offp)",
		  dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN,
				   (dm_off_t*)(-1000), 
				   nelem, &extent, &nelem_ret))
	  ERRTEST(EFAULT, "get_allocinfo (bad extentp)",
		  dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN,
				   &off, nelem, (dm_extent_t*)(-1000),
				   &nelem_ret))
	  ERRTEST(EFAULT, "get_allocinfo (bad nelemp)",
		  dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN,
				   &off, nelem, &extent,
				   (u_int*)(-1000)))
	}
   
	/*------------------------------------------------------
	 * get_bulkattr: see test_fileattr.c
	 *------------------------------------------------------
	 * get_config
	 *------------------------------------------------------
	 */
	  ERRTEST(EFAULT, "get_config", 
		  dm_get_config(hanp, hlen, DM_CONFIG_BULKALL,
				(dm_size_t*)(-1000)))
	/*---------------------------------------------------------
	 * get_config_events
	 *---------------------------------------------------------
	 */
	{ 
	  u_int         nelem=5;
	  u_int         *nelemp = NULL;
	  dm_eventset_t *eventsetp;
	  eventsetp = (dm_eventset_t *)malloc(nelem*sizeof(dm_eventset_t));
	  if (eventsetp == NULL) {
	    printf("Couldn't malloc for config_events tests: %s\n", ERR_NAME);
	  }
	  else {
	  ERRTEST(EFAULT, "get_config_events (bad eventsetp)",
		dm_get_config_events(hanp, hlen, nelem, 
				     (dm_eventset_t*)(-1000), nelemp))
	  ERRTEST(EFAULT, "get_config_events (bad nelemp)",
		  dm_get_config_events(hanp, hlen, nelem, eventsetp, 
				       (u_int*)(-1000)))
	  }
	}
	/*---------------------------------------------------------
	 * get_dirattrs: see test_fileattr.c
	 *---------------------------------------------------------
	 * get_fileattr: see test_fileattr.c
	 *---------------------------------------------------------
	 * get_region: see test_region.c
	 *---------------------------------------------------------
	 * getall_dmattr
	 *---------------------------------------------------------
	 */
	{ 
	  size_t buflen = 5;
	  void   *bufp  = (void *)malloc(buflen*sizeof(dm_attrlist_t));
	  size_t *rlenp = NULL;
	  ERRTEST(EFAULT, "getall_dmattr (NULL handle)", 
		  dm_getall_dmattr(sid, NULL, hlen, DM_NO_TOKEN,
				   buflen, bufp, rlenp))
	  ERRTEST(EFAULT, "getall_dmattr (incremented  bufp)", 
		  dm_getall_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				   buflen, (void*)((char*)(bufp)+1), rlenp))
	  ERRTEST(EFAULT, "getall_dmattr (bad bufp)", 
		  dm_getall_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				   buflen, (void*)(-1000), rlenp))
	  ERRTEST(EFAULT, "getall_dmattr (bad rlenp)", 
		  dm_getall_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				   buflen, bufp, (size_t*)(-1000)))
	}
	/*---------------------------------------------------------
	 * init_attrloc
	 *---------------------------------------------------------
	 */
	ERRTEST(ENOTSUP, "init_attrloc", 
		dm_init_attrloc(sid, hanp, hlen, DM_NO_TOKEN,
				(dm_attrloc_t*)(-1000)))
	/*---------------------------------------------------------
	 * probe_hole: see test_hole.c
	 *---------------------------------------------------------
	 * remove_dmattr: see test_dmattr.c
	 *---------------------------------------------------------
	 * set_dmattr: see test_dmattr.c
	 *---------------------------------------------------------
	 * set_eventlist: see test_eventlist.c
	 *---------------------------------------------------------
	 * set_region: see test_region.c
	 *---------------------------------------------------------
	 */

abort_test:
	sprintf(command, "rm %s\n", test_file); 
	system(command);

	printf("EFAULT testing complete.\n");

	exit(0);
}

