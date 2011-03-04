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

#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_set_eventlist().  The
command line is:

	test_eventlist [-v] [-s sid] [-t token] directory

where:
ls_path
        is the path to a specific copy of ls, important only for its size
directory
	is the pathname to a DMAPI filesystem.
sid
	is the dm_sessid_t value to use.
token
	is the dm_token_t value to use (DM_NO_TOKEN is the default).
----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;

int check_one_event (dm_sessid_t, void*, size_t, dm_token_t, 
		     dm_eventtype_t, int);

static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-v] [-s sid] [-t token]"
		"ls_path directory \n", Progname);
	/* fprintf(stderr, "possible events are:\n");
	   for (i = 0; i < ev_namecnt; i++) {
		fprintf(stderr, "%s (%d)\n", ev_names[i].name,
			ev_names[i].value);
        }
        */
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	token = DM_NO_TOKEN;
	char		object[128];
	void		*hanp;
	size_t	 	hlen;
	int		Vflag = 0;
	char		*name;
	int		opt;
	int             error;
        void           *fshanp;
        size_t          fshlen;
	int             i;
	char            command[128];
	char           *ls_path;
	char           *dir_name;
	dm_token_t	test_token = DM_NO_TOKEN;
	dm_eventset_t   eventset;
	void           *test_vp;
	u_int           nelemp;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "s:t:uv")) != EOF) {
		switch (opt) {
		case 's':
			sid = atol(optarg);
			break;
		case 't':
			token = atol(optarg);
			break;
		case 'u':
		        Vflag=2;
			break;
		case 'v':
		        if (Vflag==0) Vflag=1;
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
	
	/* Get the directory handle */
	if (dm_path_to_handle(dir_name, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n", dir_name);
		exit(1);
	}

	/***********************************************\
	|* Test to run on a FILE...                    *|
	\***********************************************/
	
	printf("File test beginning...\n");
	sprintf(object, "%s/VeryLongUnlikelyFilename.DMAPIFOO", dir_name);
	sprintf(command, "cp %s %s \n", ls_path, object); 
	system(command);

	if (dm_path_to_handle(object, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n; aborting test",
			object);
	}
	else {
	  for (i = 0; i < ev_namecnt; i++) {
	    error = check_one_event(sid, hanp, hlen, token,
				    ev_names[i].value, Vflag);  
	    switch (ev_names[i].value){
	    case DM_EVENT_ATTRIBUTE: case DM_EVENT_DESTROY:
	      if (error) {
		fprintf(stderr, "ERROR: %s failed on a file!\n",
			ev_names[i].name);  
	      }
	      break;
	    default: 
	      if (!error) {
		fprintf(stderr, "ERROR: %s succeeded on a file!\n",
			ev_names[i].name);  
	      }
	    }
	  }
	  /*------------------------*\
	  |*  ## Errno subtests ##  *|
	  \*------------------------*/
	  printf("\t(errno subtests beginning...)\n");
	  DMEV_ZERO(eventset);
	  DMEV_SET(DM_EVENT_ATTRIBUTE, eventset);
	  /*---------------------------------------------------------*/
	  EXCLTEST("set", hanp, hlen, test_token, 
		   dm_set_eventlist(sid, hanp, hlen, test_token, 
				    &eventset, DM_EVENT_MAX))
	  /*---------------------------------------------------------*/
	  if ((test_vp = handle_clone(hanp, hlen)) == NULL) {
	    fprintf(stderr, 
		    "Cannot create a test handle (%s); skipping EBADF test\n",
		    ERR_NAME);
	  }
	  else {
	    /* Alter the handle copy to make it (presumably) invalid */
	    ((char *) test_vp)[hlen/2]++;
	    ERRTEST(EBADF,
		    "set",
		    dm_set_eventlist(sid, test_vp, hlen, token,
				     &eventset, DM_EVENT_MAX))
	    dm_handle_free(test_vp, hlen);
	  }
	  /*---------------------------------------------------------*/
#ifdef	VERITAS_21
	/* Veritas gets a segmentation fault if hanp is NULL or if the
	   &eventset is out of range.
	*/
	fprintf(stderr, "\tERROR testing for EFAULT in set (bad hanp): "
		"Veritas gets a segmentation fault.\n");
	fprintf(stderr, "\tERROR testing for EFAULT in set (bad eventset): "
		"Veritas gets a segmentation fault.\n");
#else
	  ERRTEST(EFAULT, 
		  "set",
		  dm_set_eventlist(sid, NULL, hlen, token, 
				&eventset, DM_EVENT_MAX))
	  ERRTEST(EFAULT, 
		  "set",
		  dm_set_eventlist(sid, hanp, hlen, token, 
				(dm_eventset_t*)(-1000), DM_EVENT_MAX))
#endif
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "set (bad session)",
		  dm_set_eventlist(-100, hanp, hlen, token,
				   &eventset, DM_EVENT_MAX))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "set (bad token)",
		  dm_set_eventlist(sid, hanp, hlen, 0, 
				&eventset, DM_EVENT_MAX))
	  /*---------------------------------------------------------*/
#if 0
	  PROBLEM: too-small buffer does not produce E2BIG 
	  { 
	    dm_eventset_t  *small_evsp = malloc(0);
	    if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen)) {
	      fprintf(stderr, 
		      "can't get filesystem handle from %s; aborting test\n",
		      dir_name);
	    } 
	    else {
	      check_one_event(sid, fshanp, fshlen, token, 
			      DM_EVENT_CREATE, Vflag);
	      ERRTEST(E2BIG,
		      "(broken) get",
		      dm_get_eventlist(sid, fshanp, fshlen, token, 
				       DM_EVENT_MAX, small_evsp, &nelemp))
	      check_one_event(sid, fshanp, fshlen, token, 
			      DM_EVENT_INVALID, Vflag);
	    }
	  }
#endif
	  /*---------------------------------------------------------*/
	  SHAREDTEST("get", hanp, hlen, test_token, 
		     dm_get_eventlist(sid, hanp, hlen, test_token,
				      DM_EVENT_MAX, &eventset, &nelemp))
	  /*---------------------------------------------------------*/
	  ERRTEST(EBADF,
		  "get",
		  dm_get_eventlist(sid, test_vp, hlen, token, DM_EVENT_MAX,
				   &eventset, &nelemp))
	  /*---------------------------------------------------------*/
#ifdef	VERITAS_21
	/* Veritas gets a segmentation fault if hanp is NULL. */

	fprintf(stderr, "\tERROR testing for EFAULT in get (bad hanp): "
		"Veritas gets a segmentation fault.\n");
#else
	  ERRTEST(EFAULT, 
		  "get",
		  dm_get_eventlist(sid, NULL, hlen, token, DM_EVENT_MAX,
				   &eventset, &nelemp ))
#endif
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL,
		  "get (bad session)",
		  dm_get_eventlist(-100, hanp, hlen, token, DM_EVENT_MAX,
				   &eventset, &nelemp))
	  /*---------------------------------------------------------*/
	  ERRTEST(EINVAL, 
		  "get (bad token)",
		  dm_get_eventlist(sid, hanp, hlen, 0, DM_EVENT_MAX,
				&eventset, &nelemp))
	  /*---------------------------------------------------------*/
	  printf("\t(errno subtests complete)\n");
	  /*---------------------*\
	  |* End of errno tests  *|
	  \*---------------------*/

	/* Finally, use DM_EVENT_INVALID to delete events... */
	check_one_event(sid, hanp, hlen, token, DM_EVENT_INVALID, Vflag);
	}
	sprintf(command, "rm %s \n", object); 
	system(command);
	printf("\tFile test complete.\n");
	if (Vflag) printf("\n");
	
	/***********************************************\
	|* Test to run on a DIRECTORY...               *|
	\***********************************************/
	
       	printf("Directory test beginning...\n");
	sprintf(object, "%s/VeryLongUnlikelyDirectoryName.DMAPIFOO",
		dir_name);
	sprintf(command, "mkdir %s \n", object); 
        system(command);	
	
	if (opaque_to_handle(object, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n; aborting test",
			object);
	}
	else {
	  for (i = 0; i < ev_namecnt; i++) {
	    error = check_one_event(sid, hanp, hlen, token,
				    ev_names[i].value, Vflag);
	    switch (ev_names[i].value){
	    case DM_EVENT_CREATE:    case DM_EVENT_POSTCREATE:
	    case DM_EVENT_REMOVE:    case DM_EVENT_POSTREMOVE:
	    case DM_EVENT_RENAME:    case DM_EVENT_POSTRENAME:
	    case DM_EVENT_LINK:      case DM_EVENT_POSTLINK:
	    case DM_EVENT_SYMLINK:   case DM_EVENT_POSTSYMLINK:
	    case DM_EVENT_ATTRIBUTE: case DM_EVENT_DESTROY:
	      if (error) {
		fprintf(stderr, "ERROR: %s failed on a directory!\n",
			ev_names[i].name);  
	      }
	      break;
	    default: 
	      if (!error) {
		fprintf(stderr, "ERROR: %s succeeded on a directory!\n",
			ev_names[i].name);  
	      }
	    }
	  }
	/* Use DM_EVENT_INVALID to delete events... */
	check_one_event(sid, hanp, hlen, token, DM_EVENT_INVALID, Vflag);
	}
	sprintf(object, "%s/VeryLongUnlikelyDirectoryName.DMAPIFOO", dir_name);
	sprintf(command, "rmdir %s\n", object); 
	system(command);
       	printf("\tDirectory test complete.\n");
	if (Vflag) printf("\n");

	/***********************************************\
	|* Test to run on a FILESYSTEM...              *|
	\***********************************************/
	
       	printf("Filesystem test beginning...\n");
	
	if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen)) {
	  fprintf(stderr, 
		  "can't get filesystem handle from %s; aborting test\n",
		  dir_name);
	}
	else {
	  for (i = 0; i < ev_namecnt; i++) {
	    error = check_one_event(sid, fshanp, fshlen, token, 
				    ev_names[i].value, Vflag);
	    switch (ev_names[i].value){
	    case DM_EVENT_PREUNMOUNT: case DM_EVENT_UNMOUNT:
	    case DM_EVENT_NOSPACE:    case DM_EVENT_DEBUT:
	    case DM_EVENT_CREATE:     case DM_EVENT_POSTCREATE:
	    case DM_EVENT_REMOVE:     case DM_EVENT_POSTREMOVE:
	    case DM_EVENT_RENAME:     case DM_EVENT_POSTRENAME:
	    case DM_EVENT_LINK:       case DM_EVENT_POSTLINK:
	    case DM_EVENT_SYMLINK:    case DM_EVENT_POSTSYMLINK:
	    case DM_EVENT_ATTRIBUTE:  case DM_EVENT_DESTROY:
	      if (error) {
		fprintf(stderr, "ERROR: %s failed on a filesystem!\n",
			ev_names[i].name);  
	      }
	      break;
	    default: 
	      if (!error) {
		fprintf(stderr, "ERROR: %s succeeded on a filesystem!\n",
			ev_names[i].name);  
	      }
	    }
	  }
	/* Use DM_EVENT_INVALID to delete events... */
	check_one_event(sid, fshanp, fshlen, token, DM_EVENT_INVALID, Vflag);
	}
       	printf("\tFilesystem test complete.\n");

	/***********************************************\
	|* End of tests!                               *|
	\***********************************************/

	dm_handle_free(fshanp, fshlen);
	dm_handle_free(hanp, hlen);
	exit(0);
}

/*-------------------------------------------------------------------
  check_one_event: 
   
  Using dm_set_eventlist, set a single event on the object
  indicated by the handle.

  Using dm_get_eventlist, check to see that the single event
  was set correctly.
  -------------------------------------------------------------------*/

int 
check_one_event (
		 dm_sessid_t       sid,
		 void	           *hanp,
		 size_t	           hlen,
		 dm_token_t	   token,
		 dm_eventtype_t    event,
		 int		   Vflag )
{
	dm_eventset_t	eventset;
	dm_eventset_t	check_eventset;
	u_int		nelemp;
	int             set_count = 0;
	int		i;

	DMEV_ZERO(eventset);
	DMEV_ZERO(check_eventset);
		
	if (event != DM_EVENT_INVALID) {
	  DMEV_SET(event, eventset);
	}

	if (dm_set_eventlist(sid, hanp, hlen, token, 
				      &eventset, DM_EVENT_MAX)) {
		if (Vflag){
		  fprintf(stdout, " note: %s could not be set (%s)\n",
			  ev_value_to_name(event), errno_names[errno]);
		}
		return (-1);
	}

	if (dm_get_eventlist(sid, hanp, hlen, token,
			     DM_EVENT_MAX, &check_eventset, &nelemp)) {
		if (Vflag){
		fprintf(stdout, "dm_get_eventlist failed, %s\n",
			errno_names[errno]);
		}
		return (-2);
	}
	
	/*  For each element, see that get_eventlist agrees 
	 *  with set_eventlist; if not, make noise.
	 */  
	for (i = 0; i < ev_namecnt; i++) {
	  int set = DMEV_ISSET(ev_names[i].value, eventset);
	  int found = DMEV_ISSET(ev_names[i].value, check_eventset);
	  if (set && !found) {
	    fprintf(stderr, "ERROR: event %s was set but not found.\n",
		    ev_names[i].name);
	    return (-3);
	  } 
	  else if (!set && found) {
	    fprintf(stderr, "ERROR: Found unexpected event %s \n",
		    ev_names[i].name);
	    return (-4);
	  }
	  else if ((Vflag == 2) && !set && !found) {
	    fprintf(stderr, "clear: %s\n",
		   ev_names[i].name);
	  }
	  else if (Vflag && set && found) {
	    fprintf(stderr, "  SET: %s\n", ev_names[i].name);
	    set_count++;
	  }
	}
	if (Vflag && set_count == 0) {
	  fprintf(stderr, "\t(All events cleared)\n");
	}
        return 0;
}
