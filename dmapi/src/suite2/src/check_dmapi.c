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
#ifdef linux
#include <dmapi.h>
#else
#include <sys/dmi.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <lib/hsm.h>
#include <lib/dmport.h>

#ifdef linux
#include <string.h>
#endif

/*---------------------------------------------------------------------------
Automated test of version of DMAPI libraries & kernels 

The command line is: 

     check_dmapi [-v] 

where v is a verbose-output flag
----------------------------------------------------------------------------*/
#ifdef linux
#define CREATE_DESTROY_OPCODE DM_DESTROY_SESSION
#define SET_DISP_OPCODE DM_SET_DISP
#else
#define CREATE_DESTROY_OPCODE 5
#define SET_DISP_OPCODE 46
#endif

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;

char	*Progname;

int     dmi(int, ...);

static void
usage(void)
{
	int	i;

	fprintf(stderr, "usage:\t%s [-v]\n"
		"\t(use the v switch for verbose output)\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	old_sid = -1;
	dm_sessid_t	sid;
	void		*hanp;
	size_t	 	 hlen;
	dm_token_t      token = DM_NO_TOKEN;
	int             Vflag = 0;
	char		*name = "old";
	char		*junk = "test junk";
	int		opt;
	int		i;
	int             kernel_status=-1;
	int             library_status=-1L;
	dm_size_t       retval;
	struct stat     stat_buf;

	if (Progname = strrchr(argv[0], '/')) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "v")) != EOF) {
	  switch (opt) {
	  case 'v':
	    Vflag++;
	    break;
	  case '?':
	    usage();
	  }
	}
	if (optind != argc)
	  usage();
	

	if (geteuid()!=0) {
	  printf("You are running as user #%d.  "
		 "You must be root to run this diagnostic.\n", geteuid());
	  exit(1);
	}

	/*--------------------------
	 * RESOLVE KERNEL PRESENCE:
	 *--------------------------
	 */
	if (dmi(CREATE_DESTROY_OPCODE, old_sid, junk, &sid) >= 0) {
	  printf("ERROR: invalid kernel create/destroy_session call "
		 "succeeded!\n");
	  exit(1);
	}
	else if (errno==ENOPKG) {
	  kernel_status=0;
	}
	else if (errno==EINVAL){
	  if (Vflag) printf("(create/destroy_session call verifies "
			    "that you have DMAPI in kernel)\n");
	}
	else {
	  printf("ERROR: kernel create/destroy_session call produced "
		 "unexpected errno, (%d) %s\n", errno, strerror(errno));
	}
	
	/*----------------------------------
	 * RESOLVE KERNEL STATUS IF PRESENT:
	 *----------------------------------
	 */
	if (kernel_status==-1 &&
	    dmi(SET_DISP_OPCODE, 
		(dm_sessid_t) 0, 
		(void*) 0,
		(size_t) 0,
		(dm_token_t) 0,
		(dm_eventset_t) 0,
		(u_int) 0) >= 0) { 
	  printf("ERROR: invalid kernel set_disp call suceeded!\n");
	}
	else {
	  if (errno==ENOSYS) {
	    if (Vflag) 
	      printf("(kernel set_disp call indicates old kernel)\n");
	    kernel_status=1;
	  }
	  else if (errno==ENOPKG) {
	    if (Vflag) 
	      printf("(kernel set_disp call indicates no kernel)\n");
	    kernel_status=0;
	  }
	  else if (errno==EINVAL) {
	    if (Vflag)
	      printf("(kernel set_disp call indicates new kernel)\n");
	    kernel_status=2;
	  }
	  else {
	    printf("ERROR: kernel set_disp call failed: (%d) %s\n", 
		   errno, strerror(errno));
	    exit(1);
	  }
	}

	/*-------------------------
	 * RESOLVE LIBRARY STATUS:
	 *-------------------------
	 */
	if (dm_init_service(&name) == -1)  {
	  fprintf(stderr, "ERROR: can't initialize the DMAPI (%s).\n",
		  strerror(errno));
	  library_status=0;
	}
	else if (strcmp(name, DM_VER_STR_CONTENTS)) {
	  if (Vflag) 
	    printf("(dm_init_service suggests that "
		   "you have an old library)\n");
	  library_status=1;
	}
	else {
	  if (Vflag) 
	    printf("(dm_init_service suggests that "
		   "you have a new library)\n");
	  library_status=2;
	}
	
	if (Vflag) printf("(dm_init_service returned %s)\n", name);
	
	/*-------------------------
	 * MAKE A DIAGNOSIS:
	 *-------------------------
	 */

	if (library_status==2 && kernel_status==2){
	  printf("DIAGNOSIS: Tests show a current version of "
		 "DMAPI is installed.\n");
	} 
	else if (library_status==1 && kernel_status==1) {
	  printf("DIAGNOSIS: Tests show that you have an outdated "
		 "installation of DMAPI.\nUpgrades to both kernel and "
		 "library routines will be necessary.\n");
	}
	else if (library_status==0 && kernel_status==0) {
	  printf("DIAGNOSIS: Tests show that NO components of the DMAPI "
		 "are installed!\nUpgrades to both kernel and "
		 "library routines will be necessary.\n");
	}
	else {
	  printf("DIAGNOSIS: Tests show that:\n"
		 "Your DMAPI kernel routines are ");
	  switch (kernel_status) {
	  case 0: printf ("missing (not installed).\n");
	     break;
	  case 1: printf ("outdated.\n");
	     break;
	  case 2: printf ("current.\n ");
	     break;
	  default: printf("[ERROR!].\n");
	  }
	  printf("Your DMAPI library is ");
	  switch (library_status) {
	  case 0: printf ("missing (not installed).\n");
	     break;
	  case 1: printf ("outdated.\n");
	     break;
	  case 2: printf ("current.\n");
	     break;
	  default: printf("[ERROR!].\n");
	  }
	}
#ifndef linux
	if (library_status!=2 || kernel_status!=2){
	  printf("Please install XFS patch 1907 (for IRIX 6.2) or "
		 "patch 2287 (for IRIX 6.4).\n");
	}
#endif
}

 

