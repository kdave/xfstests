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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>

#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------
Automated test of the DMAPI functions: 
     dm_set_dmattr()
     dm_get_dmattr()
     dm_remove_dmattr()

The command line is:

	test_dmattr [-v] [-n num] [-l length] [-s sid] directory 

where 
   ls_path 
     is the path to a specific copy of ls, important only for its size
   directory 
     is the pathname to a DMAPI filesystem
   num
     is the number of files to create for the test.
   length
     is the length of the attribute value for the test.
   sid 
     is the session ID whose attributes you are interested in.

----------------------------------------------------------------------------*/

#define VALUE_LENGTH 22
#define NUM_ITERATIONS 50
#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;

static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-v] [-n number] [-l length] "
		"[-s sid] ls_path pathname\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*dir_name;
	char		*ls_path;
	dm_attrname_t	*attrnamep;
	size_t		buflen=VALUE_LENGTH;
      	char		*bufp;
	int		setdtime = 0;
	size_t		rlenp;
	void		*hanp;
	size_t	 	hlen;
	char		*name;
	int		opt;
	int		i=0;
	int		j=0;
	int             Vflag=0;
	int             num_iter = NUM_ITERATIONS;
        char            test_file[128];
	char            command[128];
        char            **test_array;
	dm_size_t       config_retval;
	dm_token_t      test_token;
	struct stat    *statbuf;
	struct stat    *checkbuf;

        Progname = strrchr(argv[0], '/');
        if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "vn:l:s:")) != EOF) {
		switch (opt) {
		case 'v':
			Vflag++;
			break;
		case 'n':
			num_iter = atoi(optarg);
			break;
		case 'l':
			buflen = atoi(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 2 != argc)
		usage();
	ls_path = argv[optind];
	dir_name = argv[optind+1];

     	bufp =
	  (char *)(malloc (buflen * sizeof(char)));  
	statbuf = 
	  (struct stat *)(malloc (num_iter * sizeof(struct stat)));
     	checkbuf = 
	  (struct stat *)(malloc (num_iter * sizeof(struct stat)));
	test_array = 
	  (char **)(malloc (num_iter * sizeof(char *)));
	if (bufp==NULL || test_array==NULL || 
	    statbuf==NULL || checkbuf==NULL) {
	  printf("Malloc failed\n");
	  exit(1);
	}
	for (i=0; i<num_iter; i++) {
	  test_array[i] =
	    (char*)(malloc (buflen * sizeof(char)));
	  if (test_array[i] == NULL) {
	    printf("Malloc failed\n");
	    exit(1);
	  }
	}

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);
	
	printf("Attribute tests beginning...\n");
	
	attrnamep = (dm_attrname_t *)("DMATTR");

	/* File creation loop*/
	for (i=0; i < num_iter; i++) {
	  sprintf(test_file, "%s/DMAPI_attribute_test_file.%d", 
		  dir_name, i);
	  sprintf(command, "cp %s %s \n", ls_path, test_file); 
	  system(command);
	}
	sleep(1);
	/* SET loop */
	for (i=0; i < num_iter; i++) {
	  sprintf(test_file, "%s/DMAPI_attribute_test_file.%d", 
		  dir_name, i);

	  if (stat(test_file, &(statbuf[i]))){
	    fprintf(stdout, 
		    "Error: unable to stat the test file; %s (before set)\n", 
		    test_file);
	  }
	  if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	    fprintf(stderr, "can't get handle for %s; bypassing test\n",
		    test_file);
	  }
	  else {
	    for (j=0; j < VALUE_LENGTH; j++) {
	      test_array[i][j]=(char)(rand()/128);;
	    } 
	    /* buflen is already set (to VALUE_LENGTH) */
	    if (dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN, attrnamep, 
			      (i<num_iter/2)?0:1, buflen, test_array[i])) {
	      fprintf(stderr, "dm_set_dmattr failed on test %d, %s\n",
		      i, ERR_NAME);
	    }
	    else { 
	      if (Vflag){
		printf("Report: success with set #%d.\n", i);
	      }
	    }
	  }
	}
		
	/* GET loop */
	for (i=0; i < num_iter; i++) {
	  sprintf(test_file, "%s/DMAPI_attribute_test_file.%d", 
		  dir_name, i);

	  if (stat(test_file, &(checkbuf[i]))){
	    fprintf(stdout, 
		    "Error: unable to stat the test file; %s (before get)\n", 
		    test_file);
	  }
	  if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	    fprintf(stderr, "can't get handle for %s; bypassing test\n",
		    test_file);
	  }
	  else {
	    if (dm_get_dmattr(sid, hanp, hlen, DM_NO_TOKEN, attrnamep, buflen,
			      bufp, &rlenp)) {
	      if (errno == E2BIG) {
		fprintf(stderr, "dm_get_dmattr buffer too small, "
			"should be %zd bytes\n", rlenp);
	      } else {
		fprintf(stderr, "dm_get_dmattr failed (%s) for test file %d\n",
			ERR_NAME, i);
	      }
	    }
	    else {
	      /* Compare bufp with test_array[i]: */
	      if (strncmp(test_array[i], bufp, buflen)){
		printf("ERROR: failure on get test #%d.\n", i);
	      }
	      else if (Vflag) {
		printf("Report: success with get #%d. "
		       "(output matches expectation)\n",i);
	      }
	    }
	  }
	}
	
	/* It's time for timestamp checking! */
	for (i=0; i < num_iter; i++) {
#ifdef linux
	  if ((statbuf[i].st_atime == checkbuf[i].st_atime) &&
	      (statbuf[i].st_mtime == checkbuf[i].st_mtime) &&
	      (statbuf[i].st_ctime == checkbuf[i].st_ctime))
#else
	  if ((statbuf[i].st_atim.tv_sec == checkbuf[i].st_atim.tv_sec) &&
	      (statbuf[i].st_atim.tv_nsec == checkbuf[i].st_atim.tv_nsec) &&
	      (statbuf[i].st_mtim.tv_sec == checkbuf[i].st_mtim.tv_sec) &&
	      (statbuf[i].st_mtim.tv_nsec == checkbuf[i].st_mtim.tv_nsec) &&
	      (statbuf[i].st_ctim.tv_sec == checkbuf[i].st_ctim.tv_sec) &&
	      (statbuf[i].st_ctim.tv_nsec == checkbuf[i].st_ctim.tv_nsec))
#endif
	  {
	    if (i < num_iter/2) {
	      /* Time stamp did not change, correctly */
		if (Vflag) {
		fprintf(stdout, "Report: Time stamp was correctly "
			"unchanged by test %d.\n", i);
		}
	    }
	    else {
	      /* Time stamp did not change, but should have */
	      fprintf(stdout, "Error: the time stamp should have "
		      "changed in test file %d\n", i);
	    }
	  }
	  else {
	    /* Time stamp changed, but should not have. */
	    if (i < num_iter/2) {
	      fprintf(stdout, "Error: the time stamp should not"
		      "change in test file %d\n", i);
	    }
	    else {
	    /* Time stamp changed, and should  have. */
	      if (Vflag) {
		fprintf(stdout, "Report: Time stamp was correctly "
			"changed by test %d.\n", i);
	      }
	    }
	  }
	}

	
	/* REMOVE loop */
	for (i=0; i < num_iter; i++) {
	  sprintf(test_file, "%s/DMAPI_attribute_test_file.%d", 
		  dir_name, i);

	  if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	    fprintf(stderr, "can't get handle for %s; bypassing test\n",
		    test_file);
	  }
	  else {
	    if (dm_remove_dmattr(sid, hanp, hlen, DM_NO_TOKEN, setdtime,
				 attrnamep)) {
	      fprintf(stderr, "dm_remove_dmattr failed (%s) on test #%d\n",
		      ERR_NAME, i);
	    }
	    else {
	      if (Vflag) {
		printf("Report: success with remove test #%d.\n",i);
	      }
	    }
	  }
	}

	for (i=0; i < num_iter; i++) {
	  sprintf(test_file, "%s/DMAPI_attribute_test_file.%d", 
		  dir_name, i);
	  sprintf(command, "rm %s \n", test_file); 
	  system(command);
	}

	/*************************************\
	|* Correct-input testing complete.   *|
	|* Beginning improper-input testing. *|
	\*************************************/
	sprintf(test_file, "%s/DMAPI_attribute_test_file.ERRNO", 
		dir_name);
	sprintf(command, "cp %s %s\n", ls_path, test_file); 
	system(command);
	
	if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	  fprintf(stderr, "can't get handle for %s; bypassing errno tests\n",
		  test_file);
	}
	else {
	  
	  printf("\t(errno subtests beginning...)\n");
	  /**** SET tests ****/
	  /*---------------------------------------------------------*/
	  dm_get_config(hanp, hlen, DM_CONFIG_MAX_ATTRIBUTE_SIZE, 
			&config_retval);
	  
	  ERRTEST(E2BIG,
		  "set", 
		  dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				 attrnamep, setdtime, (config_retval+1), 
				"foofoofoo"))
	  /*---------------------------------------------------------*/
	  EXCLTEST("set", hanp, hlen, test_token, 
		   dm_set_dmattr(sid, hanp, hlen, test_token,
				 attrnamep, 0, buflen, "no right"))
	  /*---------------------------------------------------------*/
	  { void* test_vp;
	    if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	      fprintf(stderr, "Cannot create a test handle (%s); "
		      "skipping EBADF test\n", ERR_NAME);
	    }
	    else {
	      ((char *) test_vp)[hlen/2]++;
	      ERRTEST(EBADF,
		      "set",
		      dm_set_dmattr(sid, test_vp, hlen, DM_NO_TOKEN,
				    attrnamep, 0, buflen, "EBADF"))
		dm_handle_free(test_vp, hlen);
	    }
	  }
	  /*---------------------------------------------------------*/
	  ERRTEST(EBADF,
		  "set",
		  dm_set_dmattr(sid, hanp, hlen-1, DM_NO_TOKEN,
				attrnamep, 0, buflen, "EBADF"))
	  /*---------------------------------------------------------*/
	  ERRTEST(EFAULT,
		  "set",
		  dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				(dm_attrname_t*)(-1000), 0,
				buflen, "EFAULT_test" ))
	  ERRTEST(EFAULT,
		  "set",
		  dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				attrnamep, 0, buflen, (void*)(-1000)))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "set (bad token)",
		  dm_set_dmattr(sid, hanp, hlen, (dm_token_t)(-1000), 
				attrnamep, 0, buflen, 
				"EINVAL_bad_token"))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "set (bad session id)",
		  dm_set_dmattr((dm_sessid_t)(-1000), hanp, hlen, 
				DM_NO_TOKEN, attrnamep, 0, buflen, 
				"EINVAL_bad_session_id"))
	    
	  /**** GET tests ****/
	  /*---------------------------------------------------------*/
	  dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
			attrnamep, 0, buflen,
			"ERRNO for GET_DMATTR");
	  /*---------------------------------------------------------*/
	  ERRTEST(E2BIG,
		  "get",
		  dm_get_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				attrnamep, 0, bufp, &rlenp))
	  /*---------------------------------------------------------*/
	  SHAREDTEST("get", hanp, hlen, test_token, 
		     dm_get_dmattr(sid, hanp, hlen, test_token,
				   attrnamep, buflen, bufp, &rlenp))
	  /*---------------------------------------------------------*/
	  { void* test_vp;
	    if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	      fprintf(stderr, "Cannot create a test handle (%s); "
		      "skipping EBADF test\n", ERR_NAME);
	    }
	    else {
	      ((char *) test_vp)[hlen/2]++;
	      ERRTEST(EBADF,
		      "get",
		      dm_get_dmattr(sid, test_vp, hlen, DM_NO_TOKEN,
				    attrnamep, buflen, bufp, &rlenp))
		dm_handle_free(test_vp, hlen);
	    }
	  }
	  /*---------------------------------------------------------*/
	  ERRTEST(EBADF,
		  "get",
		  dm_get_dmattr(sid, hanp, hlen-1, DM_NO_TOKEN,
				attrnamep, buflen, bufp, &rlenp))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "get (invalid session)",
		  dm_get_dmattr((dm_sessid_t)(-1000), hanp, hlen, DM_NO_TOKEN,
				attrnamep, buflen, bufp, &rlenp))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "get (invalid token)",
		  dm_get_dmattr(sid, hanp, hlen, (dm_token_t)(-1000),
				attrnamep, buflen, bufp, &rlenp))
	  /*---------------------------------------------------------*/
	  ERRTEST(ENOENT,
		  "get",
		  dm_get_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				(dm_attrname_t *)("NO_SUCH_ENTRY"),
				buflen, bufp, &rlenp))
	  /*---------------------------------------------------------*/
	
	  /**** REMOVE tests ****/
	  /*---------------------------------------------------------*/
	  dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
			attrnamep, 0, buflen,
			"ERRNO for DMATTR");
	  EXCLTEST("remove", hanp, hlen, test_token,
		   dm_remove_dmattr(sid, hanp, hlen, test_token, 
				    0, attrnamep))
	  /*---------------------------------------------------------*/
	  { void* test_vp;
	    if (dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN, attrnamep, 
			      0, buflen, "ERRNO for DMATTR")) {
	      printf("ERROR in setting dmattr for remove_dmattr test. (%s)\n",
		     ERR_NAME);
	    } 
	    else if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	      fprintf(stderr, "Cannot create a test handle (%s); "
		      "skipping EBADF test\n", ERR_NAME);
	    }
	    else {
	      ((char *) test_vp)[hlen/2]++;
	      ERRTEST(EBADF,
		      "remove",
		      dm_remove_dmattr(sid, test_vp, hlen, DM_NO_TOKEN,
				       0, attrnamep))
		dm_handle_free(test_vp, hlen);
	    }
	  }
	  /*---------------------------------------------------------*/
	  if (dm_set_dmattr(sid, hanp, hlen, DM_NO_TOKEN, attrnamep, 0, 
			    buflen, "ERRNO for DMATTR")) {
	    printf("ERROR in setting dmattr for remove_dmattr test. (%s)\n",
		   ERR_NAME);
	  } 
	  else {
	    ERRTEST(EBADF,
		    "remove",
		    dm_remove_dmattr(sid, hanp, hlen-1, DM_NO_TOKEN,
				     0, attrnamep))
	  }
	  /*---------------------------------------------------------*/
	  ERRTEST(EFAULT,
		  "remove",
		  dm_remove_dmattr(sid, hanp, hlen, DM_NO_TOKEN,
				 0, (void*)(-1000)))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "remove (bad token)",
		  dm_remove_dmattr(sid, hanp, hlen, (dm_token_t)(-1000),
				 0, attrnamep))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "remove (bad session)",
		  dm_remove_dmattr(-1, hanp, hlen, DM_NO_TOKEN,
				 0, attrnamep))
	  /*---------------------------------------------------------*/


	 sprintf(test_file, "%s/DMAPI_attribute_test_file.ERRNO", 
		 dir_name);
	 sprintf(command, "rm %s\n", test_file); 
	 system(command);
	 printf("\t(errno subtests complete)\n");
	}
	/**********************************\
	|* End of improper-input testing. *|
	\**********************************/


	printf("Attribute tests complete!\n");

	dm_handle_free(hanp, hlen);
	exit(0);
}
