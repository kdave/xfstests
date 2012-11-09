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

#include <limits.h>

#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>
#include <time.h>

/*---------------------------------------------------------------------------

Automated test of the DMAPI functions dm_write_invis() and dm_read_invis()

The command line is:

	test_invis [-s sid] [-v] ls_path pathname

where:
   sid
      is the session ID whose events you you are interested in.
   ls_path
      is the path to a specific copy of ls, important only for its size
   pathname
      is the filesystem to use for the test.

DM_WRITE_SYNC is is not supported.
----------------------------------------------------------------------------*/

#define OFF_MAX  50
#define OFF_STEP 5
#define LEN_MAX  50
#define LEN_STEP 5

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-v] [-s sid] ls_path pathname\n", 
		Progname);
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
	dm_off_t	offset = 0;
	dm_size_t	length = 1;
	dm_size_t	curlength = 0;
	u_char		ch;
	void		*bufp = NULL;
	void		*hanp;
	size_t		hlen;
	dm_ssize_t	rc;
	char		*name;
	char            test_file[128];
	char            command[128];
	int		opt;
	int		i;
	int		j;
	int             k;
	int             Vflag=0;
	struct stat     statbuf;
	struct stat     checkbuf;
	dm_token_t      test_token;
	void*           test_vp;
	int		cont;
	int		error_reported;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "vs:")) != EOF) {
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
	if (optind + 2 != argc)
		usage();
	ls_path = argv[optind];
	dir_name = argv[optind+1];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);
	
	/* Get a random character for read/write tests */
	srand((unsigned int)time(NULL));
	ch = (u_char)rand(); 

	printf("Invisible read/write tests beginning...\n");
	
	/* File creation loop*/
	for(i=0; i<LEN_MAX; i+=LEN_STEP) {
	  for (j=0; j<OFF_MAX; j+=OFF_STEP) {
	    sprintf(test_file, "%s/DMAPI_invis_test_file.%02d%02d", 
		    dir_name, i, j);
	    sprintf(command, "cp %s %s\n", ls_path, test_file); 
	    system(command);
	  }
	}
	
	/* Write to files, then read them to check for correct results. 
	   Do timestamp checking along the way. */

	for(i=0; i<LEN_MAX; i+=LEN_STEP) {
	  for (j=0; j<OFF_MAX; j+=OFF_STEP) {

#define max(a,b) ((a) > (b) ? (a) : (b))
	    length = max((dm_size_t)(i), length);
	    offset = (dm_off_t)(j);

	    sprintf(test_file, "%s/DMAPI_invis_test_file.%02d%02d", 
		    dir_name, i, j);
	    
	    if (stat(test_file, &statbuf)){
	      fprintf(stdout, 
		      "Error: unable to stat test file; %s (before test)\n", 
		      test_file);
	      continue;
	    }
	    
	    if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	      fprintf(stderr, "can't get handle for %s; bypassing test\n",
		      test_file);
	      continue;
	    }
	    
	    if (length > curlength) {
	      if(curlength>0)
		free(bufp);
	      if ((bufp = malloc(length)) == NULL) {
		fprintf(stderr, "malloc of %llu bytes failed\n",
			(unsigned long long) length);
		continue;
	      }
	      curlength = length;
	      memset(bufp, ch, length);
	    }

	    rc = dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN,
				0, offset, length, bufp);
	    cont = 0;
	    if (rc < 0) {
	      fprintf(stderr, "dm_write_invis failed, %s\n", ERR_NAME);
	      cont=1;
	    } else if (rc != length) {
	      fprintf(stderr, "expected to write %lld bytes, actually "
		      "wrote %lld\n", (long long) length, (long long) rc);
	      cont=1;
	    }
	    if(cont)
		continue;
	    
	    /* Timestamp checking, part 1 */
	    if (stat(test_file, &checkbuf)){
	      fprintf(stdout, 
		      "Error: unable to stat the test file; %s (after write)\n", 
		      test_file);
	    }
	    else {
	      if ((statbuf.st_atime == checkbuf.st_atime) &&
	      (statbuf.st_mtime == checkbuf.st_mtime) &&
	      (statbuf.st_ctime == checkbuf.st_ctime))
		{
		if (Vflag) {
		  printf("Report: time stamp unchanged by write\n");
		}
	      }
	      else {
		printf("Error: time stamp changed by write\n");
	      }
	    }
	    
	    rc = dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN,
				offset, length, bufp);
	    if (rc < 0) {
	      fprintf(stderr, "dm_read_invis failed, %s\n", ERR_NAME);
	      continue;
	    } 
	    else if (rc != length) {
	      fprintf(stderr, "expected to read %lld bytes, actually "
		      "wrote %lld\n", (long long) length, (long long) rc);
	      continue;
	    }
	    else {
	      /* Be sure the buffer is filled with the test char */
	      error_reported = 0;
	      for (k=0; k<i; k++){
		if (((u_char *)bufp)[k] == ch) {
		  if (Vflag) printf(".");
		}
		else {
		  if(!error_reported){
			printf("Error!(line=%d)\n", __LINE__);
			error_reported++;
		  }
		}
	      }
	      if (Vflag) printf("\n");
	    }

	    /* Timestamp checking, part 2 */
	    if (stat(test_file, &statbuf)){
	      fprintf(stdout, 
		      "Error: unable to stat the test file; %s (after write)\n", 
		      test_file);
	    }
	    else {
	      if ((statbuf.st_atime == checkbuf.st_atime) &&
	      (statbuf.st_mtime == checkbuf.st_mtime) &&
	      (statbuf.st_ctime == checkbuf.st_ctime))
		{
		if (Vflag) {
		  printf("Report: time stamp unchanged by read\n");
		}
	      }
	      else {
		printf("Error: time stamp changed by read\n");
	      }
	    }
	  } /* for (j=0; j<OFF_MAX; j+=OFF_STEP) */
	} /* for(i=0; i<LEN_MAX; i+=LEN_STEP) */
	
	/* File deletion loop*/
	for(i=0; i<LEN_MAX; i+=LEN_STEP) {
	  for(j=0; j<OFF_MAX; j+=OFF_STEP) {
	    sprintf(test_file, "%s/DMAPI_invis_test_file.%02d%02d", 
		    dir_name, i, j);
	    sprintf(command, "rm %s\n", test_file); 
	    system(command);
	  }
	}
	
	/*************************************\
	|* Correct-input testing complete.   *|
	|* Beginning improper-input testing. *|
	\*************************************/
	sprintf(test_file, "%s/DMAPI_invis_test_file.ERRNO", 
		dir_name);
	sprintf(command, "cp %s %s\n", ls_path, test_file); 
	system(command);

	if (dm_path_to_handle(test_file, &hanp, &hlen)) {
	  fprintf(stderr, "can't get handle for %s; bypassing errno tests\n",
		  test_file);
	}
	else {
	  dm_off_t offset;

	  /* Try writing a character waaaaaay up in the millions range */
	  sprintf(bufp, "%c", ch);
          if (stat(test_file, &statbuf)){
           fprintf(stdout, 
                    "Error: unable to stat the test file; %s \n", 
                    test_file);
          }
          offset = ((1000000*(dm_off_t)(ch)) > statbuf.st_size) ? 
                            statbuf.st_size : (1000000*(dm_off_t)(ch));
	  if (dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN, 0, 
			     offset, 1, bufp)==-1){
	    printf("Error invis-writing 0x%x at byte 0x%x million: %s\n", 
		   *(u_char *)bufp, (unsigned int)ch, ERR_NAME);
	  }
	  else if (dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN,
				 offset, 1, bufp)==-1){
	    printf("Error invis-reading at byte %u million: %s\n",
		   (unsigned int)ch, ERR_NAME);
	  }
	  else if (((u_char *)bufp)[0]!=ch) {
	    printf("Error: wanted to read %c and instead got %s.\n",
		   ch, (u_char *)bufp);
	  }
	  else if (Vflag) {
	    printf("Report: \"0x%x\" was written and \"0x%x\" was read "
		   "at byte %d million.\n", ch, *(u_char *)bufp, ch);
	  }
	  printf("\t(errno subtests beginning...)\n");
	  /**** WRITE tests ****/
	  /*---------------------------------------------------------*/
	  EXCLTEST("write", hanp, hlen, test_token, 
		   dm_write_invis(sid, hanp, hlen, test_token,
				  0, 0, 13, "write test 1"))
	  /*---------------------------------------------------------*/
            if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	    fprintf(stderr, 
		    "Cannot create a test handle (%s); skipping EBADF test\n",
		    ERR_NAME);
	  }
	  else {
	    ((char *) test_vp)[hlen/2]++;
	    ERRTEST(EBADF,
		    "write",
		    dm_write_invis(sid, test_vp, hlen, DM_NO_TOKEN,
				   0, 0, 0, bufp))
	    ERRTEST(EBADF,
		    "read",
		    dm_read_invis(sid, test_vp, hlen, DM_NO_TOKEN,
				  0, 0, bufp))
	    dm_handle_free(test_vp, hlen);
	  }

	  /*---------------------------------------------------------*/
	  ERRTEST(EBADF,
		  "write",
		  dm_write_invis(sid, hanp, hlen-1, DM_NO_TOKEN,
				 0, 0, 0, NULL))
	  /*---------------------------------------------------------*/
	  ERRTEST(EFAULT,
		  "write",
		  dm_write_invis(sid, NULL, hlen, DM_NO_TOKEN,
				 0, 0, 0, NULL))
	  /*---------------------------------------------------------*/
#if 0
	  PROBLEM: write_invis refuses to produce EINVAL for 
	  lengths that will not fit in a dm_size_t.

	  ERRTEST(EINVAL,
		  "(bad length) write",
		  dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN,
				 0, 4096, (long long)0xFFFFFFFFFFFFFFFFLL,
				 "write invalid length test"))
#endif
	  /*---------------------------------------------------------*/
#if 0
	  PROBLEM (somewhat fixed): A signal is sent, rather than EFBIG.
	  Presumably, this signal is needed to comply with...something.
	  If this is uncommented, the program will abort here, with the 
	  error message "exceeded file size limit". 

	  ERRTEST(EFBIG,
		  "write",
		  dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN,
				 0, (long long)0xFFFFFFFFFFLL, 
				 (long long)0xFFFFFFFFFFLL,
				 "foo foo foo"))
#endif
	  /*---------------------------------------------------------*/
#ifdef VERITAS_21
	  ERRTEST(EINVAL,
		  "(bad offset) write",
		  dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN,
				 0, (dm_size_t) ULONG_MAX, 5,
				 "write invalid offset test"))
#else
#ifndef linux
	  ERRTEST(EINVAL,
		  "(bad offset) write",
		  dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN,
				 0, (dm_size_t) ULONGLONG_MAX, 5,
				 "write invalid offset test"))
#endif
#endif

	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "(bad sid) write",
		  dm_write_invis(-100, hanp, hlen, DM_NO_TOKEN,
				 0, 0, 26, "write invalid offset test"))


	  /**** READ tests ****/
	  /*---------------------------------------------------------*/
	  SHAREDTEST("read", hanp, hlen, test_token,
		     dm_read_invis(sid, hanp, hlen, test_token, 
				   0, 13, bufp))
	  /*---------------------------------------------------------*/
	  ERRTEST(EBADF,
		  "read",
		  dm_read_invis(sid, hanp, hlen-1, DM_NO_TOKEN,
				 0, 0, bufp))
	  /*---------------------------------------------------------*/
	  ERRTEST(EFAULT,
		  "read",
		  dm_read_invis(sid, NULL, hlen, DM_NO_TOKEN,
				 0, 0, bufp))
	  /*---------------------------------------------------------*/
#ifdef	VERITAS_21
	  ERRTEST(EINVAL,
		  "(bad offset) read",
		  dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN,
				ULONG_MAX, 5, bufp));
#else
#ifndef linux
	  ERRTEST(EINVAL,
		  "(bad offset) read",
		  dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN,
				ULONGLONG_MAX, 5, bufp));
#endif
#endif

	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "(bad sid) read",
		  dm_read_invis(-100, hanp, hlen, DM_NO_TOKEN,
				0, 5, bufp))
	  /*---------------------------------------------------------*/
	  printf("\t(errno subtests complete!)\n");
	}
	sprintf(test_file, "%s/DMAPI_invis_test_file.ERRNO", 
	        dir_name);
	sprintf(command, "rm %s \n", test_file); 
	system(command);

	printf("Invisible read/write tests complete.\n");

	dm_handle_free(hanp, hlen);
	exit(0);
}
