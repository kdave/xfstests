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
Automated test of access rights, involving many DMAPI functions

The command line is:
   test_rights [-s sid] [-v] ls_path pathname

where:
   sid
      is the session ID whose events you you are interested in.
   ls_path
      is the path to a copy of ls, which will be copied as a test file.
   pathname
      is the filesystem to use for the test.
----------------------------------------------------------------------------*/

#define NUM_TOKENS 4

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
	dm_token_t	token[NUM_TOKENS];
	dm_token_t	test_token;
	void		*fs_hanp;
	size_t		fs_hlen;
	void		*dir_hanp;
	size_t		dir_hlen;
	void		*ap;
	size_t		alen;
	void		*bp;
	size_t		blen;
	void		*cp;
	size_t		clen;
	char		*name;
	char		*ls_path;
	char		*pathname;
	char            fname_a[100];
	char            fname_b[100];
	char            fname_c[100];
	char            command[150];
	int		opt;
	int             i=0;

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
	
	printf("Beginning access rights testing...\n");

	sprintf(fname_a, "%s/DMAPI_rights_test_file_a", pathname);
	sprintf(command, "cp %s %s\n", ls_path, fname_a); 
	system(command);

	if (dm_path_to_handle(fname_a, &ap, &alen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  fname_a, ERR_NAME);
	  goto abort_test;
	}

	sprintf(fname_b, "%s/DMAPI_rights_test_file_b", pathname);
	sprintf(command, "cp %s %s\n", ls_path, fname_b); 
	system(command);
	
	if (dm_path_to_handle(fname_b, &bp, &blen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  fname_b, ERR_NAME);
	  goto abort_test;
	}

	sprintf(fname_c, "%s/DMAPI_rights_test_file_c", pathname);
	sprintf(command, "cp %s %s\n", ls_path, fname_c); 
	system(command);
	
	if (dm_path_to_handle(fname_c, &cp, &clen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  fname_c, ERR_NAME);
	  goto abort_test;
	}

	if (dm_path_to_fshandle(pathname, &fs_hanp, &fs_hlen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  pathname, ERR_NAME);
	  goto abort_test;
	}

	sprintf(pathname, "%s/DMAPI_rights_test_dir", pathname); 
	sprintf(command, "mkdir %s\n", pathname); 
	system(command);

	if (dm_path_to_handle(pathname, &dir_hanp, &dir_hlen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  pathname, ERR_NAME);
	  goto abort_test;
	}

	/* Test remaining functions for appropriate 
	 * right requirements...
	 *------------------------------------------------------------*/
	{ 
	  dm_off_t off = (dm_off_t)0;
	  dm_extent_t extent;
	  u_int nelem_ret;
	  SHAREDTEST("get_allocinfo", ap, alen, test_token,
		   dm_get_allocinfo(sid, ap, alen, test_token, 
				    &off, 1, &extent, &nelem_ret))
	}
	/*------------------------------------------------------------*/
	{ 
	  void *bufp=(void*)malloc(5*sizeof(dm_attrlist_t));
	  size_t rlen;
	  SHAREDTEST("getall_dmattr", ap, alen, test_token,
		   dm_getall_dmattr(sid, ap, alen, test_token, 
				    5, bufp, &rlen))
	}
	/*------------------------------------------------------------*/
	{ 
	  dm_attrloc_t loc;
	  SHAREDTEST("init_attrloc", dir_hanp, dir_hlen, test_token,
		     dm_init_attrloc(sid, dir_hanp, dir_hlen, test_token,
				     &loc))
	}
	/*------------------------------------------------------------*/
#if 0
	mkdir_by_handle is NOT SUPPORTED in current SGI DMAPI 

 	{ 
	  SHAREDTEST("mkdir_by_handle", fs_hanp, fs_hlen, test_token,
		     dm_mkdir_by_handle(sid, fs_hanp, fs_hlen, test_token,
					dir_hanp, dir_hlen, "FUBAR_DIR"))
	}
#endif
	/*------------------------------------------------------------*/
	{ dm_eventset_t eventset;
	  DMEV_ZERO(eventset);
	  EXCLTEST("set_disp", fs_hanp, fs_hlen, test_token,
		   dm_set_disp(sid, fs_hanp, fs_hlen, test_token,
			       &eventset, DM_EVENT_MAX))
	}
	/*------------------------------------------------------------*/
	{ dm_attrname_t attrname={"TEST"};
	  EXCLTEST("set_return...", fs_hanp, fs_hlen, test_token,
		   dm_set_return_on_destroy(sid, fs_hanp, fs_hlen, test_token,
			       &attrname, DM_TRUE))
	}
	/*------------------------------------------------------------*/

	/* Create the tokens */
	for (i=1; i<NUM_TOKENS; i++){
	  if (dm_create_userevent(sid, 0, 0, &token[i])==-1) {
	    fprintf(stderr, "Couldn't create token %d.\n", i);
	    goto abort_test;
	  }
	}

	ERRTEST(EINVAL,
		"rights-on-NO_TOKEN",
		dm_request_right(sid, ap, alen, DM_NO_TOKEN,
				 0, DM_RIGHT_SHARED))
	ERRTEST(EINVAL,
		"rights-on-NO_TOKEN",
		dm_request_right(sid, ap, alen, DM_NO_TOKEN,
				 0, DM_RIGHT_EXCL))

	if (dm_request_right(sid, ap, alen, token[1], 0, DM_RIGHT_SHARED))
	  printf("ERROR: Request for SHARED failed on handle a, token 1");
	if (dm_request_right(sid, ap, alen, token[2], 0, DM_RIGHT_SHARED))
	  printf("ERROR: Request for SHARED failed on handle a, token 2");
	
	/* ---  These WOULD be correct tests, 
           ---  if rights were fully implemented.

	ERRTEST(EAGAIN, "EXCL request",
		dm_request_right(sid, ap, alen, token[1], 0, DM_RIGHT_EXCL))
	ERRTEST(EAGAIN, "EXCL request",
		dm_request_right(sid, ap, alen, token[2], 0, DM_RIGHT_EXCL))
	ERRTEST(EAGAIN, "upgrade",
		dm_upgrade_right(sid, ap, alen, token[1]))
       */
	
abort_test:	
	
	for (i=1; i<NUM_TOKENS; i++)
	  dm_respond_event(sid, token[i], DM_RESP_CONTINUE, 0, 0, 0); 

	sprintf(command, "rm %s\n", fname_a); 
	system(command);

	sprintf(command, "rm %s\n", fname_b); 
	system(command);
	
	sprintf(command, "rm %s\n", fname_c); 
	system(command);

	sprintf(command, "rmdir %s\n", pathname); 
	system(command);

	dm_handle_free(ap, alen);
	dm_handle_free(bp, blen);
	dm_handle_free(cp, clen);

	printf("Access rights testing complete.\n");

	exit(0);
}







