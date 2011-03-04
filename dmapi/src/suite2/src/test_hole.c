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

#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_punch_hole().  The
command line is:

	test_hole [-v] [-s sid] pathname

where 
   ls_path
      is the path to a specific copy of ls, important only for its size
   pathname 
      is the path to the test filesystem
   sid
      is the session ID whose events you you are interested in.

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
	fprintf(stderr, "usage:\t%s [-v] [-s sid] ls_path directoryname\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*pathname = NULL;
	char		*ls_path = NULL;
	dm_off_t	offset = 0, end;
	dm_off_t	ex_off = 0;
	dm_extent_t     extent[20];
	u_int           nelem;
	dm_size_t	length = 0;
	void		*hanp;
	size_t	 	hlen;
	dm_token_t      test_token;
	char		*name;
	int		opt;
	int             Vflag = 0;
	char		filename[128];
	char		command[128];
	dm_off_t	roff;
	dm_size_t	rlen;
	dm_off_t        blocksize = 5; 
	void		*test_vp;
	struct stat    buf;
	struct stat    checkbuf;


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
	pathname = argv[optind+1];

	if (dm_init_service(&name) == -1)  {
		fprintf(stdout, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the directory handle. */

	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
		fprintf(stdout, 
		     "ERROR: can't get handle for directory %s\n", pathname);
		exit(1);
	}
	
	printf("Hole test beginning...\n");
	sprintf(filename, "%s/VeryLongUnlikelyFilename.HOLETEST", pathname);
	sprintf(command, "cp %s %s \n", ls_path, filename); 
	system(command);

	if (dm_path_to_handle(filename, &hanp, &hlen)) {
	  fprintf(stdout, "can't get handle for %s\n; aborting test",
		  filename);
	  
	  sprintf(command, "rm %s \n", filename); 
	  system(command);
	  fprintf(stdout, "\tHole test aborted.\n");
	  
	  dm_handle_free(hanp, hlen);
	  exit(1);
	}
   
	/*   ## Get the block size using a length-1 probe. ##   */
	dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, 1, 0,
		      &blocksize, &rlen);
	
	if (blocksize==0) {
	  fprintf(stdout, "Error: block size appears to be 0!\n");
	  
	  sprintf(command, "rm %s \n", filename); 
	  system(command);
	  fprintf(stdout, "\tHole test aborted.\n");
	  
	  dm_handle_free(hanp, hlen);
	  exit(1);
	}

	/*
	 * The kernel always rounds the offset up to the next block
	 * size, so we can only probes up to the previous to last block.
	 */
	end = (29604 / blocksize) * blocksize;

	/* Check that dm_probe_hole returns an extent from the next
	 * highest multiple of the block size, to the end of the file
	 */
	for (offset = 0; offset < end; offset++) { 
	  if (dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, offset, length,
			    &roff, &rlen)) {
	    fprintf(stdout, "dm_probe_hole failed on pass %lld (%s)\n",
		    (long long)offset, ERR_NAME);
	  }
	  else {
	    if (rlen != 0) {
		fprintf(stdout, 
			"Error: hole did not extend to end of file!\n");
	    }
	    if (blocksize*(roff/blocksize) != roff) {
		fprintf(stdout,
			"Error: offset not a multiple of block size!\n");
	    }
	  }
	}
	
	/* Be sure dm_punch_hole doesn't change the time stamp, 
	 * and verify that dm_get_allocinfo shows a hole
	 * followed by an extent to the end of the file.
	 */
	for(offset = 28672; offset > 0; offset-=blocksize) {
	  if (stat(filename, &buf)){
	    fprintf(stdout, 
		    "Error: unable to stat the test file; %s (1st)\n", 
		    filename);
	    continue;
	  }
	  if (dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, offset, length,
			    &roff, &rlen)) {
	    fprintf(stdout, "dm_probe_hole failed, %s\n",
		    ERR_NAME);
	    continue;
	  }
	  if (roff != offset) {
	    fprintf(stdout, 
		    "Error: presumed offset was not %lld.\n",
		    (long long)(roff));
	  }
	  if (dm_punch_hole(sid, hanp, hlen, DM_NO_TOKEN,
			    roff, length)) {
	    fprintf(stdout, "dm_punch_hole failed, %s\n",
		    ERR_NAME);
	    continue;
	  }
	  if (stat(filename, &checkbuf)){
	    fprintf(stdout, 
		    "Error: unable to stat the test file. (2nd)\n");
	    continue;
	  }
	  else {
	    /* COMPARE BUF AND CHECKBUF! */
#ifdef linux
	    if ((buf.st_atime == checkbuf.st_atime) &&
		(buf.st_mtime == checkbuf.st_mtime) &&
		(buf.st_ctime == checkbuf.st_ctime))
#else
	    if ((buf.st_atim.tv_sec == checkbuf.st_atim.tv_sec) &&
		(buf.st_atim.tv_nsec == checkbuf.st_atim.tv_nsec) &&
		(buf.st_mtim.tv_sec == checkbuf.st_mtim.tv_sec) &&
		(buf.st_mtim.tv_nsec == checkbuf.st_mtim.tv_nsec) &&
		(buf.st_ctim.tv_sec == checkbuf.st_ctim.tv_sec) &&
		(buf.st_ctim.tv_nsec == checkbuf.st_ctim.tv_nsec))
#endif
	   {
	      if (Vflag) {
		fprintf(stdout, 
			"\tTime stamp unchanged by hole from offset %lld.\n",
			(long long)(offset));
	      }
	    }
	    else {
	      fprintf(stdout,
		      "Error: punch_hole changed file's time stamp.\n");
	    }
	    ex_off=0;
	    if ((dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN, 
				  &ex_off, 1, extent, &nelem) == 1) &&
		(extent->ex_type == DM_EXTENT_RES) &&
		(dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN,
				  &ex_off, 1, extent, &nelem) == 0) &&
		(extent->ex_type == DM_EXTENT_HOLE)) {
	      if (extent->ex_offset == roff){
		if (Vflag) {
		  fprintf(stdout, "\tVerified hole at %lld\n", 
			  (long long)(extent->ex_offset));
		}
	      }
	      else {
		fprintf(stdout, "\tError: get_allocinfo found hole at %lld\n",
			(long long)(extent->ex_offset));
	      }
	    }
	    else {
		fprintf(stdout, "\tError: get_allocinfo did not find an "
			"extent followed by a hole!\n");
	    }
	  }
	}
	/*------------------------*\
	|*  ## Errno subtests ##  *|
	\*------------------------*/
	fprintf(stdout, "\t(beginning errno subtests...)\n");
	/*---------------------------------------------------------*/
	ERRTEST(E2BIG,
		"probe (from past EOF)",
		dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, 30000, length,
			      &roff, &rlen))
	/*---------------------------------------------------------*/
	ERRTEST(E2BIG,
		"probe (to past EOF)",
		dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, 15000, 150000,
			      &roff, &rlen))
        /*---------------------------------------------------------*/
	SHAREDTEST("probe", hanp, hlen, test_token, 
		 dm_probe_hole(sid, hanp, hlen, test_token, 
			       0, 0, &roff, &rlen)) 
	/*---------------------------------------------------------*/
	EXCLTEST("punch", hanp, hlen, test_token, 
		   dm_punch_hole(sid, hanp, hlen, test_token, 0, 0)) 
	/*---------------------------------------------------------*/
	/*
	 * No idea where that EAGAIN should come from, it's never
	 * returned from the kernel.
	 *
	 * 		-- hch
	 */
#if 0
	ERRTEST(EAGAIN,
		"punch",
		dm_punch_hole(sid, hanp, hlen, DM_NO_TOKEN,
			      1, length))	
#endif
	/*---------------------------------------------------------*/
	if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	  fprintf(stderr, 
		  "Cannot create a test handle (%s); skipping EBADF test\n",
		  ERR_NAME);
	}
	else {
	 ((char *) test_vp)[hlen/2]++;
	  ERRTEST(EBADF,
		  "probe",
		  dm_probe_hole(sid, test_vp, hlen, DM_NO_TOKEN,
				offset, length,
				&roff, &rlen))
	  ERRTEST(EBADF,
		  "punch",
		  dm_punch_hole(sid, test_vp, hlen, DM_NO_TOKEN,
				offset, length))
	}
	dm_handle_free(test_vp, hlen);
        /*---------------------------------------------------------*/
        ERRTEST(EFAULT,
		"probe (null handle)",
		dm_probe_hole(sid, 0, hlen, DM_NO_TOKEN, offset, length,
				&roff, &rlen))
        ERRTEST(EFAULT,
		"probe (bad rlen)",
		dm_probe_hole(sid, 0, hlen, DM_NO_TOKEN, offset, length,
				&roff, (dm_size_t*)(-1000)))
        ERRTEST(EFAULT,
		"probe (bad roff)",
		dm_probe_hole(sid, hanp, hlen, DM_NO_TOKEN, offset, length,
				(dm_off_t*)(-1000), &rlen))
        /*---------------------------------------------------------*/
        ERRTEST(EFAULT,
		"punch",
		dm_punch_hole(sid, 0, hlen, DM_NO_TOKEN, offset, length))
        /*---------------------------------------------------------*/
        ERRTEST(EINVAL,
		"probe (bad session)",
		dm_probe_hole(-100, hanp, hlen, DM_NO_TOKEN, offset, length,
				&roff, &rlen))
        /*---------------------------------------------------------*/
        ERRTEST(EINVAL, 
		"probe (bad token)",
		dm_probe_hole(sid, hanp, hlen, 0, offset, length,
				&roff, &rlen))
        /*---------------------------------------------------------*/
        ERRTEST(EINVAL, 
		"probe (bad token 2)",
		dm_probe_hole(sid, hanp, hlen, 0, offset, length,
				&roff, &rlen))
        /*---------------------------------------------------------*/
	fprintf(stdout, "\t(errno subtests complete)\n");
		

	sprintf(command, "rm %s \n", filename); 
	system(command);
	printf("Hole test complete.\n");

	dm_handle_free(hanp, hlen);
	exit(0);
}

