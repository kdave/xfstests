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

#include <unistd.h>

#include <time.h>
#include <string.h>


/*---------------------------------------------------------------------------
Automated test of the DMAPI functions:
   dm_set_fileattr()
   dm_get_fileattr()
   dm_get_bulkattr()
   dm_get_dirattrs()

The command line is:
   test_fileattr [-s sid] [-n num_files] [-v] ls_path pathname

where:
   sid
      is the session ID whose events you you are interested in.
   num_files
      is the number of test files to create.
   ls_path
      is the path to a copy of ls, which will be copied as a test file.
   pathname
      is the filesystem to use for the test.
----------------------------------------------------------------------------*/

#define SET_MASK  DM_AT_ATIME|DM_AT_MTIME|DM_AT_CTIME|DM_AT_DTIME|\
                  DM_AT_UID|DM_AT_GID|DM_AT_MODE|DM_AT_SIZE 

#define GET_MASK DM_AT_EMASK|DM_AT_PMANR|DM_AT_PATTR|\
		  DM_AT_DTIME|DM_AT_CFLAG|DM_AT_STAT

extern	int	optind;
extern	int	opterr;
extern	char	*optarg;

char	*Progname;
int     Vflag=0;


int
comp_stat ( dm_stat_t expected,
	    dm_stat_t found,
	    int       i)
{
   int good=0;
   if (found.dt_mode != expected.dt_mode) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected mode %ld, but found %ld\n",
	     i, (long)expected.dt_mode, (long)found.dt_mode);
   }
   else good++;
   if (found.dt_uid != expected.dt_uid) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected uid %ld, but found %ld\n",
	     i, (long)expected.dt_uid, (long)found.dt_uid);
   }
   else good++;
   if (found.dt_gid != expected.dt_gid) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected gid %ld, but found %ld\n",
	     i, (long)expected.dt_gid, (long)found.dt_gid);
   }
   else good++;
   if (found.dt_atime != expected.dt_atime) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected atime %ld, but found %ld\n",
	     i, expected.dt_atime, found.dt_atime);
   }
   else good++;
   if (found.dt_mtime != expected.dt_mtime) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected mtime %ld, but found %ld\n",
	     i, expected.dt_mtime, found.dt_mtime);
   }
   else good++;
   if (found.dt_ctime != expected.dt_ctime) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected ctime %ld, but found %ld\n",
	     i, expected.dt_ctime, found.dt_ctime);
   }
   else good++;
   
   /* NOTE: dtime gets set to ctime */
   
   if (found.dt_dtime != expected.dt_ctime) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected dtime %ld, but found %ld\n",
	     i, expected.dt_ctime, found.dt_dtime);
   }
   else good++;
   if (found.dt_size != expected.dt_size) {
     fprintf(stderr, 
	     "ERROR: get #%d, expected size %lld, but found %lld\n",
	     i, (long long) expected.dt_size, (long long) found.dt_size);
   }
   else good++;
   if (Vflag){
     if (good==8) {
       fprintf(stderr, "report: get #%d had no errors.\n",i);
     } else {
       fprintf(stderr, "report: %d tests correct for get #%d.\n", 
	       good, i);
     }
   }
   return(good);
}


static void
usage(void)
{
	fprintf(stderr, 
		"Usage: %s [-v] [-s sid] [-n num_files] ls_path pathname\n",
		Progname);
	exit(1);
}


int
main(
	int	argc,
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	test_token = DM_NO_TOKEN;
	void		*hanp;
	size_t		hlen;
	char		*ls_path;
	char		*pathname;
	char            test_file[100];
	char            command[200];
	int		num_files=50;
	dm_stat_t	*stat_arr;
	dm_stat_t	dmstat;
	dm_fileattr_t   fileattr;
	char		*name;
	int		opt;
	int             oops=1;
	int             i=0;
	dm_attrloc_t    loc;
	size_t          buflen = 16*sizeof(dm_stat_t);
	size_t          rlen;
	void            *bufp;
	dm_stat_t       *statbuf;
	int             loops=0;
	void            *fs_hanp;
	size_t          fs_hlen;
	void            *targhanp;
	size_t          targhlen;
	char            check_name[100];
	char            *chk_name_p;
	int             chk_num;

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
		case 'n':
		         num_files = atoi(optarg);
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

	/* Seed the random number generator */
	srand((unsigned int)time(NULL)); 

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);
	
	/* Dynamically allocate stat_arr; */
	stat_arr = 
	  (dm_stat_t *)malloc(num_files * sizeof(dm_stat_t));
	
	printf("Beginning file attribute tests...\n");

	/* Fill in the dm_stat blocks with lots of junk...
	 */
	for (i=0; i<num_files; i++) {
	  /*
	   * XFS inode timestamp t_sec is a 32 bit signed value. rand() only
	   * returns 15 bits of random number, and so the rand()*0x10000 is
	   * really a rand() << 16 to populate the upper 16 bits of
	   * the timestamp.IOWs, linux does not need this but irix does.
	   */
	  stat_arr[i].dt_atime=((time_t)(rand()+rand()*0x10000) & 0x7FFFFFFF);
	  stat_arr[i].dt_mtime=((time_t)(rand()+rand()*0x10000) & 0x7FFFFFFF);
	  stat_arr[i].dt_ctime=((time_t)(rand()+rand()*0x10000) & 0x7FFFFFFF);
	  stat_arr[i].dt_dtime=((time_t)(rand()+rand()*0x10000) & 0x7FFFFFFF);

	  stat_arr[i].dt_uid=(uid_t)(rand()+rand()*0x10000);
	  stat_arr[i].dt_gid=(gid_t)(rand()+rand()*0x10000);
	  stat_arr[i].dt_mode=(mode_t)((rand()%4096)+32768);
          stat_arr[i].dt_size=((dm_off_t)(rand()+rand()*0x10000)) & 0x1FFFFFFFFFFULL; /* 1 TB max */
	}	

	/*-----------------------------------------------------*\
	|* File creation and set_fileattr loop                 *|
	\*-----------------------------------------------------*/
	if (Vflag) fprintf(stderr, "\nCreating/setting up test files.\n");
	for (i=0; i < num_files; i++) {
	  sprintf(test_file, "%s/DMAPI_fileattr_test.%d", 
		  pathname, i);
	  sprintf(command, "cp %s %s \n", ls_path, test_file); 
	  system(command);

	  if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	    fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		    test_file, ERR_NAME);
	  }
	  else {
	    fileattr.fa_mode  = stat_arr[i].dt_mode;
	    fileattr.fa_uid   = stat_arr[i].dt_uid;
	    fileattr.fa_gid   = stat_arr[i].dt_gid;
	    fileattr.FA_ATIME = stat_arr[i].dt_atime;
	    fileattr.FA_MTIME = stat_arr[i].dt_mtime;
	    fileattr.FA_CTIME = stat_arr[i].dt_ctime;
	    fileattr.FA_DTIME = stat_arr[i].dt_dtime;
	    fileattr.fa_size  = stat_arr[i].dt_size;
	    if (dm_set_fileattr(sid, hanp, hlen, DM_NO_TOKEN,
				SET_MASK, &fileattr)) {
	      fprintf(stderr, "ERROR: set_fileattr failed on pass #%d; %s.\n",
		      i, ERR_NAME);
	    }
	    else if (Vflag) {
	      fprintf(stderr, "report: set #%d was successful.\n", i);
	    }
	  }
	}
	sync();
	/*-----------------------------------------------------*\
	|* Get_fileattr loop                                   *|
	\*-----------------------------------------------------*/
	if (Vflag) fprintf(stderr, "\nRunning get_fileattr test\n");
	for (i=0; i < num_files; i++) {
	  sprintf(test_file, "%s/DMAPI_fileattr_test.%d", 
		  pathname, i);
	  if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	    fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		    test_file, ERR_NAME);
	  }
	  if (dm_get_fileattr(sid, hanp, hlen, DM_NO_TOKEN,
			      GET_MASK, &dmstat)) {
	    fprintf(stderr,
		    "ERROR: dm_get_fileattr failed on pass #%d, %s\n",
		    i, ERR_NAME);
	  }
	  else {
	    comp_stat(stat_arr[i], dmstat, i); 
	  }
	}
	  
      	/*-----------------------------------------------------*\
	|* Get_dirattrs loop                                   *|
	\*-----------------------------------------------------*/
	if (Vflag) fprintf(stderr, "\nRunning get_dirattrs test\n");
	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  pathname, ERR_NAME);
	  goto abort_test;
	}
	if ((bufp = (void*)malloc(buflen)) == NULL) {
	  fprintf(stderr, "Can't allocate memory for buffer.\n");
	    goto abort_test;
	}
	if (dm_init_attrloc(sid, hanp, hlen, DM_NO_TOKEN, &loc)){
	  fprintf(stderr, 
		  "ERROR: dm_init_attrloc failed with %s.\n",
		  ERR_NAME);
	  goto abort_test;
	}
	i=0; 
	loops=0;
	do {
	  /* printf("About to call get_dirattrs;\tloops=%d\n", loops);
	   * fflush(stdout);
	   * sleep(1);
	   */
	  oops=dm_get_dirattrs(sid, hanp, hlen, DM_NO_TOKEN,
			       GET_MASK, &loc, buflen,
			       bufp, &rlen);
	  if (rlen==0) break;
	  for (statbuf = bufp; statbuf != NULL; 
	       statbuf = DM_STEP_TO_NEXT(statbuf, dm_stat_t *)) {
	    chk_name_p = DM_GET_VALUE(statbuf, dt_compname, void *);
	    if (strncmp(chk_name_p, "DMAPI_fileattr_test.", 20)==0) {
	      sscanf(chk_name_p, "DMAPI_fileattr_test.%d", &chk_num);
	      if (comp_stat(stat_arr[chk_num], *statbuf, chk_num)==8) i++;
	    }
	  }
	  loops++;
	} while (oops==1);
	
	if (oops==-1) {
	  fprintf(stderr, 
		  "ERROR: dm_get_dirattrs failed with %s.\n",
		  ERR_NAME);
	}
	if (i!=num_files) {
	  fprintf(stderr,
		  "ERROR: get_dirattrs found %d matching file%s "
		  "(expected %d).\n", i, (i==1)?"":"s", num_files);
	}
	else if (Vflag) {
	  fprintf(stderr, "report: get_dirattrs successfully "
		  "found %d files in %d loops.\n", i, loops);
	}

	/*-----------------------------------------------------*\
	|* Get_bulkattr loop                                   *|
	\*-----------------------------------------------------*/
	if (Vflag) fprintf(stderr, "\nRunning get_bulkattr test\n");
	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
	  fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		  pathname, ERR_NAME);
	  goto abort_test;
	}
        if (dm_path_to_fshandle(pathname, &fs_hanp, &fs_hlen)) {
	  fprintf(stderr, "ERROR: can't get filesystem handle for %s; %s\n",
		  pathname, ERR_NAME);
	  goto abort_test;
	}
	
	buflen = 16*sizeof(dm_stat_t); /* 100000000; */  
	if ((bufp = (void*)malloc(buflen)) == NULL) {
	  fprintf(stderr, "Can't allocate memory for buffer.\n");
	  goto abort_test;
	}
	if (dm_init_attrloc(sid, fs_hanp, fs_hlen,
			    DM_NO_TOKEN, &loc)){
	  fprintf(stderr, 
		  "ERROR: dm_init_attrloc failed with %s.\n",
		  ERR_NAME);
	  goto abort_test;
	}
	
	i=0;
	loops=0;
	do {
	  oops=dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
			       GET_MASK, &loc, buflen, bufp, &rlen); 
	  if (rlen==0) break;
	  for( statbuf = bufp; statbuf != NULL;  
	       statbuf = DM_STEP_TO_NEXT(statbuf, dm_stat_t *) ) {
	    targhanp = DM_GET_VALUE(statbuf, dt_handle, void *);
	    targhlen = DM_GET_LEN(statbuf, dt_handle);
	    if (dm_handle_to_path(hanp, hlen, targhanp, targhlen,
				  (size_t)100, check_name, &rlen)){
	      fprintf(stderr, 
		      "Warning: Couldn't get name from handle. (%s)\n",
			  ERR_NAME);
	    }
	    else {
	      /* Put JUST name (not path) from check_name into chk_name_p */
	      chk_name_p = strrchr(check_name, '/');
	      if (chk_name_p) chk_name_p++;
	      else chk_name_p = check_name;
	      /* Verify that check_name_p holds a testfile name */
	      if (strncmp(chk_name_p, "DMAPI_fileattr_test.",20)==0) {
		/* Get the test file's number and compare. */
		sscanf(chk_name_p, "DMAPI_fileattr_test.%d", &chk_num);
		if (comp_stat(stat_arr[chk_num], *statbuf, chk_num)==8)i++;
	      }
	    }		 
	  }
	  loops++;
	} while (oops==1);
	
	if (oops) {
	  fprintf(stderr, 
		  "ERROR: dm_get_bulkattr failed with %s.\n",
		  ERR_NAME);
	}
	/* printf("All_file_count: %d.  BUFLEN: %d\n",
	 * all_file_count, buflen);
	 */ 
	if (i!=num_files) {
	  fprintf(stderr,
		  "ERROR: get_bulkattr found %d matching file%s "
		  "(expected %d) in %d loops.\n", i, (i==1)?"":"s",
		  num_files, loops);
	}
	else if (Vflag) {
	  fprintf(stderr, "report: get_bulkattr successfully "
		  "found %d files in %d loops.\n", i, loops);
	}

	  /*------------------------*\
	  |*  ## Errno subtests ##  *|
	  \*------------------------*/
	  printf("\t(errno subtests beginning...)\n");
	  sprintf(test_file, "%s/DMAPI_fileattr_test.ERRNO", 
		  pathname);
	  sprintf(command, "cp %s %s\n", ls_path, test_file); 
	  system(command);

	  if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	    fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		    test_file, ERR_NAME);
	    goto abort_test;
	  }
 
	  /*------------------------------------*\
	  |*  ## dm_set_fileattr() subtests ##  *|
	  \*------------------------------------*/
	  /*---------------------------------------------------------*/
	  EXCLTEST("set", hanp, hlen, test_token, 
		   dm_set_fileattr(sid, hanp, hlen, test_token,
				   SET_MASK, &fileattr))
	  /*---------------------------------------------------------*/
	  { void *test_vp;
	  if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	    fprintf(stderr, 
		    "Cannot create a test handle (%s); skipping EBADF test\n",
		    ERR_NAME);
	  }
	  else {
	    ((char *) test_vp)[hlen/2]++;
	    ERRTEST(EBADF,
		    "set",
		    dm_set_fileattr(sid, test_vp, hlen, DM_NO_TOKEN,
				    SET_MASK, &fileattr))
	      dm_handle_free(test_vp, hlen);
	  }
	  }
	  /*---------------------------------------------------------*/
	  ERRTEST(EFAULT, 
		  "set",
		  dm_set_fileattr(sid, NULL, hlen, DM_NO_TOKEN, 
				  SET_MASK, &fileattr))
	  /*---------------------------------------------------------*/
#if 0
	  PROBLEM: 32 ones as a mask does not produce a "bad mask" 
	  EINVAL.  If it does not, I suspect nothing will.

	  ERRTEST(EINVAL, 
		  "set (bad mask) [BROKEN]",
		  dm_set_fileattr(sid, hanp, hlen, DM_NO_TOKEN, 
				  0xFFFFFFFF, &fileattr))
#endif
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "set (bad token)",
		  dm_set_fileattr(sid, hanp, hlen, 0, 
				  SET_MASK, &fileattr))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "set (bad session)",
		  dm_set_fileattr(-100, hanp, hlen, DM_NO_TOKEN, 
				  SET_MASK, &fileattr))
	  /*---------------------------------------------------------*/


	  /*------------------------------------*\
	  |*  ## dm_get_fileattr() subtests ##  *|
	  \*------------------------------------*/
	  /*---------------------------------------------------------*/
	  SHAREDTEST("get", hanp, hlen, test_token,
		     dm_get_fileattr(sid, hanp, hlen, test_token,
				     GET_MASK, &dmstat))
	  /*---------------------------------------------------------*/
	  { void *test_vp;
	  if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	    fprintf(stderr, 
		    "Cannot create a test handle (%s); skipping EBADF test\n",
		    ERR_NAME);
	  }
	  else {
	    ((char *) test_vp)[hlen/2]++;
	    ERRTEST(EBADF,
		    "get",
		    dm_get_fileattr(sid, test_vp, hlen, DM_NO_TOKEN,
				    GET_MASK, &dmstat))
	      dm_handle_free(test_vp, hlen);
	  }
	  }
	  /*---------------------------------------------------------*/
	  ERRTEST(EFAULT, 
		  "get",
		  dm_get_fileattr(sid, NULL, hlen, DM_NO_TOKEN, 
				  GET_MASK, &dmstat))
	  ERRTEST(EFAULT, 
		  "get",
		  dm_get_fileattr(sid, hanp, hlen, DM_NO_TOKEN, 
				  GET_MASK, (dm_stat_t *)(-1000)))
	  /*---------------------------------------------------------*/
#if 0
	  PROBLEM: 32 ones as a mask does not produce a "bad mask" 
	  EINVAL.  If it does not, I suspect nothing will.

	  ERRTEST(EINVAL, 
		  "get (bad mask) [BROKEN]",
		  dm_get_fileattr(sid, hanp, hlen, DM_NO_TOKEN, 
				  0xFFFFFFFF, &dmstat))
#endif
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "get (bad token)",
		  dm_get_fileattr(sid, hanp, hlen, 0, 
				  GET_MASK, &dmstat))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "get (bad session)",
		  dm_get_fileattr(-100, hanp, hlen, DM_NO_TOKEN, 
				  GET_MASK, &dmstat))
	  /*---------------------------------------------------------*/
	    

	  dm_handle_free(hanp, hlen);

	  /*------------------------------------*\
	  |*  ## dm_get_dirattrs() subtests ##  *|
	  \*------------------------------------*/
	  if (dm_path_to_handle(pathname, &hanp, &hlen)) {
	    fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		    pathname, ERR_NAME);
	  }
	  else if ((bufp = (void*)malloc(buflen)) == NULL) {
	    fprintf(stderr, "Can't allocate memory for buffer.\n");
	  }
	  else {
	    /*---------------------------------------------------------*/
	    if (dm_init_attrloc(sid, hanp, hlen, DM_NO_TOKEN, &loc)){
	      fprintf(stderr, 
		      "ERROR: dm_init_attrloc failed with %s.\n",
		      ERR_NAME);
	    } else {
	      SHAREDTEST("get_dir", hanp, hlen, test_token, 
			 dm_get_dirattrs(sid, hanp, hlen, test_token,
					 GET_MASK, &loc, buflen, bufp, &rlen))
	    }
	    /*---------------------------------------------------------*/
	    { void* test_vp;
	      if (dm_init_attrloc(sid, hanp, hlen, DM_NO_TOKEN, &loc)){
		fprintf(stderr,  "ERROR: dm_init_attrloc failed with %s.\n",
			ERR_NAME);
	      } 
	      else if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
		fprintf(stderr, "Cannot create a test handle (%s); "
			"skipping EBADF test\n", ERR_NAME);
	      }
	      else {
		((char *) test_vp)[hlen/2]++;
		ERRTEST(EBADF,
			"get",
			dm_get_dirattrs(sid, test_vp, hlen, DM_NO_TOKEN,
					GET_MASK, &loc, buflen, bufp, &rlen))
		dm_handle_free(test_vp, hlen);
	      }
	    }
	    /*---------------------------------------------------------*/
 	    { 
              if (dm_init_attrloc(sid, hanp, hlen, DM_NO_TOKEN, &loc)){
		fprintf(stderr, 
			"ERROR: dm_init_attrloc failed with %s.\n",
			ERR_NAME);
	      } 
	      else {
 		/* PROBLEM:
                   This would test alignment.  Right now, no error occurs
		   when the buffer is "out of sync" with struct size.
		   It makes it tough to read from the buffer, tho!
		  ERRTEST(EFAULT,
			"get_dir (bad bufp)",
			dm_get_dirattrs(sid, hanp, hlen, DM_NO_TOKEN,
					GET_MASK, &loc, buflen, p, &rlen))
       		*/
		ERRTEST(EFAULT,
			"get_dir (bad locp)",
			dm_get_dirattrs(sid, hanp, hlen, DM_NO_TOKEN,
					GET_MASK, (dm_attrloc_t*)(-1000),
					buflen, bufp, &rlen))
		ERRTEST(EFAULT,
			"get_dir (bad bufp)",
			dm_get_dirattrs(sid, hanp, hlen, DM_NO_TOKEN,
					GET_MASK, &loc, buflen, 
					(void*)(-1000), &rlen))
		ERRTEST(EFAULT,
			"get_dir (bad rlenp)",
			dm_get_dirattrs(sid, hanp, hlen, DM_NO_TOKEN,
					GET_MASK, &loc, buflen, bufp,
					(size_t*)(-1000)))
	      }
	    }
	    /*---------------------------------------------------------*/
	    /*---------------------------------------------------------*/
	  }
	    
	 /*------------------------------------*\
	 |*  ## dm_get_bulkattr() subtests ##  *|
	 \*------------------------------------*/
	 if (dm_path_to_fshandle(pathname, &fs_hanp, &fs_hlen)) {
	   fprintf(stderr, "ERROR: can't get handle for %s; %s\n",
		   pathname, ERR_NAME);
	 }
	 else if ((bufp = (void*)malloc(buflen)) == NULL) {
	   fprintf(stderr, "Can't allocate memory for buffer.\n");
	 }
	 else {
	   /*---------------------------------------------------------*/
	   if (dm_init_attrloc(sid, fs_hanp, fs_hlen, DM_NO_TOKEN, &loc)){
	     fprintf(stderr, 
		     "ERROR: dm_init_attrloc failed with %s.\n",
		     ERR_NAME);
	   } 
	   else {
	     SHAREDTEST("get_bulk", fs_hanp, fs_hlen, test_token, 
			dm_get_bulkattr(sid, fs_hanp, fs_hlen, test_token,
					GET_MASK, &loc, buflen, bufp, &rlen))
	   }
	   /*---------------------------------------------------------*/
	   if (dm_init_attrloc(sid, fs_hanp, fs_hlen, DM_NO_TOKEN, &loc)){
	     fprintf(stderr, 
		     "ERROR: dm_init_attrloc failed with %s.\n",
		     ERR_NAME);
	   }
	   else {
	     void *p = (void *)(((char *)bufp)+1);
	     ERRTEST(EFAULT, "get_bulk (bad bufp)",
		     dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
				     GET_MASK, &loc, buflen, 
				     (void *)(-1000), &rlen))
	     ERRTEST(EFAULT, "get_bulk (bad locp)",
		     dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
				     GET_MASK, (dm_attrloc_t *)(-1000),
				     buflen, bufp, &rlen))
	     ERRTEST(EFAULT, "get_bulk (bad rlenp)", 
		     dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
				     GET_MASK, &loc, buflen, bufp,
				     (size_t *) (-1000)))
	     ERRTEST(EFAULT, "get_bulk (bad bufp)",
		     dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
				     GET_MASK, &loc, buflen, p, &rlen))
	   }
	   /*---------------------------------------------------------*/
	 }
	 
	  sprintf(command, "rm %s/DMAPI_fileattr_test.ERRNO\n", pathname); 
	  system(command);
	  printf("\t(errno subtests complete)\n");
	  /*---------------------*\
	  |* End of errno tests  *|
	  \*---------------------*/

abort_test:
	/* File deletion loop */
	if (Vflag) printf("(Deleting test files...)\n"); 
        for (i=0; i < num_files; i++) {
	  sprintf(test_file, "%s/DMAPI_fileattr_test.%d", 
		  pathname, i);
	  sprintf(command, "rm %s\n", test_file); 
	  system(command);
	}
	printf("File attribute tests complete.\n");
	exit(0);
}
