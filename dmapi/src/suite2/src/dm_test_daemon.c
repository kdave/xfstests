

/*
 * dm_test_daemon.c
 *
 * Joseph Jackson
 * 25-Jun-1996
 *
 * Additions:
 * Jay Woodward
 * 6-Aug-1997
 * 
 * Monitor all events for a file system.
 * When one arrives, print a message with all the details.
 * If the message is synchronous, always reply with DM_RESP_CONTINUE
 * (This program doesn't perform any real file system or HSM work.)
 *
 * This is a simplification of the "migin.c" example program.
 * The original code was by Peter Lawthers:
 *   This code was written by Peter Lawthers, and placed in the public
 *   domain for use by DMAPI implementors and app writers.
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <lib/dmport.h>
#include <lib/hsm.h>

  /*
   * Define some standard formats for the printf statements below.
   */

#define HDR  "%s\ntoken          :%d\nsequence       :%d\n"
#define VALS "%-15s:%s\n"
#define VALD "%-15s:%d\n"
#define VALLLD "%-15s:%lld\n"

extern int	 optind;
extern int	 errno;

void    	 usage		(char *);
int		 main		(int, char **);
static	void	 event_loop	(dm_sessid_t, int);
int		 handle_message	(dm_sessid_t, dm_eventmsg_t *);
static	int	format_mode(mode_t mode, char **ptr);
static	int	get_fs_handle	(char *, void **, size_t *);
static	int	set_disposition(dm_sessid_t, void *, size_t);
static	int	set_events	(dm_sessid_t, void *, size_t);
static	int	clear_events	(dm_sessid_t, void *, size_t);
int		 finish_responding(dm_sessid_t);
int		 establish_handler(void);
void		 exit_handler	(int);

/*
 * Keep these global so the exit_handler and err_msg routines can get to them
 */
char		*Progname;
int		 Sleep = 0;
int		 Verbose;
dm_sessid_t	 sid = 0;
dm_sessid_t	 oldsid = 0;
char		 *fsname;
int              friendly=1;
int              unfriendly_errno=EBADMSG;
int              unfriendly_count=0;
int              pending_count=0;
int          	 token_arr[10];
int              arr_top=0;

void
usage(
      char *prog)
{
  fprintf(stderr, "Usage: %s ", prog);
  fprintf(stderr, "<-s sleeptime> <-S oldsid> <-v verbose> ");
  fprintf(stderr, "filesystem \n");
}


int
main(
     int	argc,
     char	*argv[])
{

  int	 	 c;
  int	 	 error;
  void		*fs_hanp;
  size_t		 fs_hlen;
  char           buf[BUFSIZ + 8];

  Progname  = argv[0];
  fsname  = NULL;

  while ((c = getopt(argc, argv, "vs:S:")) != EOF) {
    switch (c) {
    case 's':
      Sleep = atoi(optarg);
      break;
    case 'S':
      oldsid = atoi(optarg);
      break;
    case 'v':
      Verbose = 1;
      break;
    case '?':
    default:
      usage(Progname);
      exit(1);
    }
  }
  if (optind >= argc) {
    usage(Progname);
    exit(1);
  }
  fsname = argv[optind];
  if (fsname == NULL) {
    usage(Progname);
    exit(1);
  }

  /*
   * Establish an exit handler
   */
  error = establish_handler();
  if (error)
    exit(1);

  /*
   * Init the dmapi, and get a filesystem handle so
   * we can set up our events
   */

  if (oldsid) {
	sid = oldsid;
  } else {
  	error = setup_dmapi(&sid);
  	if (error)
    		exit(1);
  }

  error = get_fs_handle(fsname, &fs_hanp, &fs_hlen);
  if (error)
    goto cleanup;

  /*
   * Set the event disposition so that our session will receive
   * all the events for the given file system
   */
  error = set_disposition(sid, fs_hanp, fs_hlen);
  if (error)
    goto cleanup;

  /*
   * Enable monitoring for all events in the given file system
   */
  error = set_events(sid, fs_hanp, fs_hlen);
  if (error)
    goto cleanup;

  /*
   * Set line buffering!!
   */
  error = setvbuf(stdout, buf, _IOLBF, BUFSIZ);
  if (error)
    goto cleanup;

  /*
   * Now sit in an infinite loop, reporting on any events that occur.
   * The program is exited after a signal through exit_handler().
   */
  printf("\n");
  event_loop(sid, 1 /*waitflag*/);

  /*
   * If we get here, cleanup after the event_loop failure
   */
 cleanup:
  exit_handler(0);
  return(1);
}


/*
 * Main event loop processing
 *
 * The waitflag argument is set to 1 when we call this from main().
 *  In this case, continuously process incoming events,
 *  blocking if there are none available.
 * In the exit_handler(), call this routine with waitflag=0.
 *  Just try to read the events once in this case with no blocking.
 */

static void
event_loop(
	dm_sessid_t	sid,
	int		waitflag)
{
	void		*msgbuf;
	size_t		bufsize;
	int		error;
	dm_eventmsg_t	*msg;
	int		count;

	/*
	 * We take a swag at a buffer size. If it's wrong, we can
	 * always resize it
	 */

	bufsize = sizeof(dm_eventmsg_t) + sizeof(dm_data_event_t) + HANDLE_LEN;
	bufsize *= 50;
	msgbuf  = (void *)malloc(bufsize);
	if (msgbuf == NULL) {
		err_msg("Can't allocate memory for buffer");
		return;
	}

	for (;;) {
		error = dm_get_events(sid, ALL_AVAIL_MSGS,
			waitflag ? DM_EV_WAIT:0, bufsize, msgbuf, &bufsize);
		if (error) {
			if (errno == EAGAIN) {
				if (waitflag)
					continue;
				break;
			}
			if (errno == E2BIG) {
				free(msgbuf);
				msgbuf = (void *)malloc(bufsize);
				if (msgbuf == NULL) {
					err_msg("Can't resize msg buffer");
					return;
				}
				continue;
			}
			errno_msg("Error getting events from DMAPI");
			break;
		}

		/*
		 * Walk through the message buffer, pull out each individual
		 * message, and dispatch the messages to handle_message(),
		 * which will respond to the events.
		 */

		count = 0;
		msg = (dm_eventmsg_t *)msgbuf;
		while (msg != NULL ) {
			count++;
			error = handle_message(sid, msg);
			if (error) {
				free(msgbuf);
				return;
			}
			printf("end_of_message\n");
			msg = DM_STEP_TO_NEXT(msg, dm_eventmsg_t *);
		}
		if (count != 1 && Verbose) {
			err_msg("Found %d events in one call to "
				"dm_get_events\n", count);
		}
	}
	if (msgbuf != NULL)
		free(msgbuf);
}


void
print_one_mount_event(
	void		*msg)
{
	void		*hanp1, *hanp2, *hanp3;
	size_t		hlen1, hlen2, hlen3;
	char		hans1[HANDLE_STR], hans2[HANDLE_STR], hans3[HANDLE_STR];
	void		*namp1, *namp2;
	size_t		nlen1, nlen2;
	char		nams1[NAME_MAX + 1], nams2[NAME_MAX + 1];
	mode_t		mode;

#if	VERITAS
	dm_namesp_event_t  *msg_ne = (dm_namesp_event_t *)msg;

	msg_ne = DM_GET_VALUE(msg, ev_data, dm_namesp_event_t *);
	hanp1  = DM_GET_VALUE(msg_ne, ne_handle1, void *);
	hlen1  = DM_GET_LEN  (msg_ne, ne_handle1);
	hanp2  = DM_GET_VALUE(msg_ne, ne_handle2, void *);
	hlen2  = DM_GET_LEN  (msg_ne, ne_handle2);
	namp1  = DM_GET_VALUE(msg_ne, ne_name1, void *);
	nlen1  = DM_GET_LEN  (msg_ne, ne_name1);
	namp2  = DM_GET_VALUE(msg_ne, ne_name2, void *);
	nlen2  = DM_GET_LEN  (msg_ne, ne_name2);
	rootp  = NULL;
	rlen   = 0;
	mode   = msg_ne->ne_mode;
#else
	dm_mount_event_t  *msg_me = (dm_mount_event_t *)msg;

	hanp1 = DM_GET_VALUE(msg_me, me_handle1, void *);
	hlen1 = DM_GET_LEN(msg_me, me_handle1);
	hanp2 = DM_GET_VALUE(msg_me, me_handle2, void *);
	hlen2 = DM_GET_LEN(msg_me, me_handle2);
	namp1  = DM_GET_VALUE(msg_me, me_name1, void *);
	nlen1 = DM_GET_LEN(msg_me, me_name1);
	namp2 = DM_GET_VALUE(msg_me, me_name2, void *);
	nlen2 = DM_GET_LEN(msg_me, me_name2);
	hanp3 = DM_GET_VALUE(msg_me, me_roothandle, void *);
	hlen3 = DM_GET_LEN(msg_me, me_roothandle);
	mode  = msg_me->me_mode;
#endif	/* VERITAS */

	if (hanp1 && hlen1) {
		hantoa(hanp1, hlen1, hans1);
	} else {
		sprintf(hans1, "<BAD_HANDLE_hlen_%zd>", hlen1);
	}
	if (hanp2 && hlen2) {
		hantoa(hanp2, hlen2, hans2);
	} else {
		sprintf(hans2, "<BAD_HANDLE_hlen_%zd>", hlen2);
	}
	if (hanp3 && hlen3) {
		hantoa(hanp3, hlen3, hans3);
	} else {
		sprintf(hans3, "<BAD_HANDLE_hlen_%zd>", hlen3);
	}
	if (namp1 && nlen1) {
		strncpy(nams1, namp1, nlen1);
		if (nlen1 != sizeof(nams1))
			nams1[nlen1] = '\0';
	} else {
		sprintf(nams1, "<BAD STRING_nlen_%zd>", nlen1);
	}
	if (namp2 && nlen2) {
		strncpy(nams2, namp2, nlen2);
		if (nlen2 != sizeof(nams2))
			nams2[nlen2] = '\0';
	} else {
		sprintf(nams2, "<BAD_STRING_nlen_%zd>", nlen2);
	}

	printf(VALS VALS VALS VALS VALS VALD,
	     "fs handle",	hans1,
	     "mtpt handle",	hans2,
	     "mtpt path",	nams1,
	     "media desig",	nams2,
	     "root handle",	hans3,
    	     "mode",		mode);
}


/*
 * First, weed out the events which return interesting structures.
 * If it's not one of those, unpack the dm_namesp_event structure
 * and display the contents.
 */
int
handle_message(
	       dm_sessid_t	sid,
	       dm_eventmsg_t	*msg)
{
  int			pkt_error = 0;
  int			error;
  dm_response_t		response = DM_RESP_INVALID;
  int			respond, respcode = 0;
  dm_namesp_event_t	*msg_ne;
#if	!VERITAS
    dm_mount_event_t	*msg_me;
#endif
  void			*hanp1, *hanp2, *namp1, *namp2;
  u_int			hlen1, hlen2, nlen1, nlen2;
  char			hans1[HANDLE_STR], hans2[HANDLE_STR];
  char			nams1[NAME_MAX + 1], nams2[NAME_MAX + 1];
  void		        *fs_hanp;
  size_t		fs_hlen;
  dm_timestruct_t       *pending_time;

  /*
   * Set the defaults for responding to events
   */

  /*****************************************************
   *     If the daemon is feeling unfriendly, it will
   *  respond (when necessary) with DM_RESP_ABORT, rather
   *  than the standard DM_RESP_CONTINUE.
   *
   *     While unfriendly, the daemon normally returns
   *  a respcode of "unfriendly_errno".  This defaults to 
   *  EBADMSG but can be set when unfriendly mode is
   *  activated.
   *****************************************************/

  respond = 1;
  if (unfriendly_count==0) {
    response = friendly ? DM_RESP_CONTINUE : DM_RESP_ABORT;
    respcode = friendly ? 0 : unfriendly_errno;
  }
  else if (unfriendly_count > 0) {
    if (unfriendly_count-- == 0) {
      response = DM_RESP_CONTINUE;
      respcode = 0;
    }
    else {
      response = DM_RESP_ABORT;
      respcode = unfriendly_errno;
    }    
  }
  
  if (pending_count >= 0) {
    if (msg->ev_type != DM_EVENT_USER) {
      if (pending_count-- == 0) {
	int i;
	for (i=arr_top; i>=0; --i) {
	  dm_respond_event(sid, token_arr[i], 
			   DM_RESP_CONTINUE, 0, 0, 0);
	}
	response = DM_RESP_CONTINUE;
	respcode = 0;
      }
      else {
        if (pending_count<10) {
	  token_arr[pending_count]=msg->ev_token;
	}
	pending_time = malloc(sizeof(dm_timestruct_t));
	pending_time->dm_tv_sec=0;
	pending_time->dm_tv_nsec=0;
	dm_pending(sid, msg->ev_token, pending_time);
	printf("pending\ntries left\t:%d\n",pending_count);
	return 0;
      }
    } 
  }

  /***** USER EVENTS *****/

  if (msg->ev_type == DM_EVENT_USER) {
    char	*privp;
    u_int	plen, i;

    printf(HDR,
		"user", msg->ev_token, msg->ev_sequence);

    /* print private data as ascii or hex if it exists 
       DM_CONFIG_MAX_MESSAGE_DATA */

    privp = DM_GET_VALUE(msg, ev_data, char *);
    plen  = DM_GET_LEN  (msg, ev_data);
    if (plen) {
	for (i = 0; i < plen; i++) {
		if (!isprint(privp[i]) && !isspace(privp[i]))
			break;
	}
	if (i == plen - 1 && privp[i] == '\0') {
	  /*****************************************************
	   *  Here, we check the messages from send_message.
	   *  Some of them have special meanings.
	   *****************************************************/
	  if (strncmp(privp, "over", 4)==0) {
	    response = DM_RESP_CONTINUE;
	    respcode = 0;
	  }
	  else if (strncmp(privp, "pending", 7)==0){
	    if (strlen(privp)>8) {
	      sscanf(privp, "pending%*c%d", &pending_count);
	    }	  
	    else {
	      pending_count=1;
	    }
	    arr_top=pending_count-1;
	  }
	  else if (strncmp(privp, "reset_fs", 8)==0){
	    if (get_fs_handle(fsname, &fs_hanp, &fs_hlen)){
	      strcpy(privp, "error");
	    }
	    else if (set_disposition(sid, fs_hanp, fs_hlen)){
	      strcpy(privp, "error");
	    }
	    else if (set_events(sid, fs_hanp, fs_hlen)){
	      strcpy(privp, "error");
	    }
	  }
	  else if (strncmp(privp, "friendly", 8)==0) {
	    friendly = 1;
	    response = DM_RESP_CONTINUE;
	    respcode = 0;
	  }
	  else if (strncmp(privp, "unfriendly", 10)==0) {
	    friendly = 0;
	    response = DM_RESP_CONTINUE;
	    respcode = 0;
	    if (strlen(privp)>11) {
	      sscanf(privp, "unfriendly%*c%d", &unfriendly_errno);
	    }
	    else {
	      unfriendly_errno=EBADMSG;
	    }
	  }
	  else if (strncmp(privp, "countdown", 9)==0) {
	    response = DM_RESP_CONTINUE;
	    respcode = 0;
	    
	    if (strlen(privp)>10) {
	      sscanf(privp, "countdown%*c%d%*c%d",
		     &unfriendly_count, &unfriendly_errno); 
	    }
	    else {
	      unfriendly_count=5;
	      unfriendly_errno=EAGAIN;
	    }
	  }


	  printf(VALS,
			"privdata", privp);

	} else {
          printf("privdata      :");
          for (i = 0; i < plen; i++) {
	    printf("%.2x", privp[i]);
          }
          printf("\n");
	}
    } else {
	printf(VALS,
		"privdata", "<NONE>");
    }

    if (msg->ev_token == DM_INVALID_TOKEN)	/* async dm_send_msg event */
      respond = 0;
  }

  /***** CANCEL EVENT *****/

/* Not implemented on SGI or Veritas */

  else if (msg->ev_type == DM_EVENT_CANCEL) {
    dm_cancel_event_t	*msg_ce;

    msg_ce = DM_GET_VALUE(msg, ev_data, dm_cancel_event_t *);
    printf(HDR VALD VALD,
	     "cancel", msg->ev_token, msg->ev_sequence,
	     "sequence",	msg_ce->ce_sequence,
	     "token",		msg_ce->ce_token);
    respond = 0;
  }

  /***** DATA EVENTS *****/

  else if (msg->ev_type == DM_EVENT_READ ||
	   msg->ev_type == DM_EVENT_WRITE ||
	   msg->ev_type == DM_EVENT_TRUNCATE) {
    dm_data_event_t	*msg_de;

    msg_de = DM_GET_VALUE(msg, ev_data, dm_data_event_t *);
    hanp1  = DM_GET_VALUE(msg_de, de_handle, void *);
    hlen1  = DM_GET_LEN  (msg_de, de_handle);
    if (hanp1 && hlen1) {
      hantoa(hanp1, hlen1, hans1);
    } else {
      sprintf(hans1, "<BAD HANDLE, hlen %d>", hlen1);
    }

    switch(msg->ev_type) {

    case DM_EVENT_READ:
      printf(HDR VALS VALLLD VALLLD,
	     "read", msg->ev_token, msg->ev_sequence,
	     "file handle",	hans1,
	     "offset",		(long long) msg_de->de_offset,
	     "length",		(long long) msg_de->de_length);
      break;

    case DM_EVENT_WRITE:
      printf(HDR VALS VALLLD VALLLD,
	     "write", msg->ev_token, msg->ev_sequence,
	     "file handle",	hans1,
	     "offset",		(long long) msg_de->de_offset,
	     "length",		(long long) msg_de->de_length);
      break;

    case DM_EVENT_TRUNCATE:
      printf(HDR VALS VALLLD VALLLD,
	     "truncate", msg->ev_token, msg->ev_sequence,
	     "file handle",	hans1,
	     "offset",		(long long) msg_de->de_offset,
	     "length",		(long long) msg_de->de_length);
      break;
    default: break;
    }
  }

  /***** DESTROY EVENT *****/

  else if (msg->ev_type == DM_EVENT_DESTROY) {
    dm_destroy_event_t	*msg_ds;
    char		attrname[DM_ATTR_NAME_SIZE + 1];
    u_char		*copy;
    u_int		clen;
    u_int		i;

    msg_ds= DM_GET_VALUE(msg, ev_data, dm_destroy_event_t *);
    hanp1  = DM_GET_VALUE(msg_ds, ds_handle, void *);
    hlen1  = DM_GET_LEN  (msg_ds, ds_handle);
    if (hanp1 && hlen1) {
      hantoa(hanp1, hlen1, hans1);
    } else {
      sprintf(hans1, "<BAD HANDLE, hlen %d>", hlen1);
    }
    if (msg_ds->ds_attrname.an_chars[0] != '\0') {
      strncpy(attrname, (char *)msg_ds->ds_attrname.an_chars, sizeof(attrname));
    } else {
      strcpy(attrname, "<NONE>");
    }
    printf(HDR VALS VALS,
	     "destroy", msg->ev_token, msg->ev_sequence,
	     "handle",		hans1,
	     "attrname",	attrname);
    copy  = DM_GET_VALUE(msg_ds, ds_attrcopy, u_char *);
    clen  = DM_GET_LEN  (msg_ds, ds_attrcopy);
    if (copy && clen) {
      printf("attrcopy :");
      for (i = 0; i < clen; i++) {
	/* Old version: printf("%.2x", copy[i]); */
	printf("%c", copy[i]);
      }
      printf("\n");
    } else {
      printf(VALS, "attrcopy", "<NONE>");
    }
    respond = 0;
  }

  /***** MOUNT EVENT *****/

	else if (msg->ev_type == DM_EVENT_MOUNT) {
		printf(HDR, "mount", msg->ev_token, msg->ev_sequence);
#if	!VERITAS
		msg_me = DM_GET_VALUE(msg, ev_data, dm_mount_event_t *);
		print_one_mount_event(msg_me);
#else	/* VERITAS */
		msg_ne = DM_GET_VALUE(msg, ev_data, dm_namesp_event_t *);
		print_one_mount_event(msg_ne);
#endif	/* VERITAS */
  }

  /***** NAMESPACE EVENTS *****/

  else {
    char	*type = NULL;

    msg_ne = DM_GET_VALUE(msg, ev_data, dm_namesp_event_t *);
    hanp1  = DM_GET_VALUE(msg_ne, ne_handle1, void *);
    hlen1  = DM_GET_LEN  (msg_ne, ne_handle1);
    hanp2  = DM_GET_VALUE(msg_ne, ne_handle2, void *);
    hlen2  = DM_GET_LEN  (msg_ne, ne_handle2);
    namp1  = DM_GET_VALUE(msg_ne, ne_name1, void *);
    nlen1  = DM_GET_LEN  (msg_ne, ne_name1);
    namp2  = DM_GET_VALUE(msg_ne, ne_name2, void *);
    nlen2  = DM_GET_LEN  (msg_ne, ne_name2);

    if (hanp1 && hlen1) {
      hantoa(hanp1, hlen1, hans1);
    }
    if (hanp2 && hlen2) {
      hantoa(hanp2, hlen2, hans2);
    }
    if (namp1 && nlen1) {
      strncpy(nams1, namp1, nlen1);
      if (nlen1 != sizeof(nams1))
	nams1[nlen1] = '\0';
    }
    if (namp2 && nlen2) {
      strncpy(nams2, namp2, nlen2);
      if (nlen2 != sizeof(nams2))
	nams2[nlen2] = '\0';
    }

    if (msg->ev_type == DM_EVENT_PREUNMOUNT ||
	msg->ev_type == DM_EVENT_UNMOUNT) {
      if (msg_ne->ne_mode == 0) {
	type = "NOFORCE";
#if	!VERITAS
      } else if (msg_ne->ne_mode == DM_UNMOUNT_FORCE) {
#else
      } else if (msg_ne->ne_mode > 0) {
#endif
	type = "FORCE";
      } else {
	type = "UNKNOWN";
	pkt_error++;
      }
    } else if (msg->ev_type == DM_EVENT_CREATE ||
               msg->ev_type == DM_EVENT_POSTCREATE ||
	       msg->ev_type == DM_EVENT_REMOVE ||
               msg->ev_type == DM_EVENT_POSTREMOVE) {
	if (format_mode(msg_ne->ne_mode, &type)) {
	  pkt_error++;
        }
    }

    switch(msg->ev_type) {

    case DM_EVENT_PREUNMOUNT:
      printf(HDR VALS VALS VALS,
	     "preunmount", msg->ev_token, msg->ev_sequence,
	     "fs handle",	hans1,
	     "root dir",	hans2,
	     "unmount mode",	type);
      break;

    case DM_EVENT_UNMOUNT:
      printf(HDR VALS VALS VALD,
	     "unmount", msg->ev_token, msg->ev_sequence,
	     "fs handle",	hans1,
	     "unmount mode",	type,
	     "retcode",		msg_ne->ne_retcode);
      break;

    case DM_EVENT_NOSPACE:
      printf(HDR VALS,
	     "nospace", msg->ev_token, msg->ev_sequence,
	     "fs handle",	hans1);
      response = DM_RESP_ABORT;
      respcode = ENOSPC;
      break;

    case DM_EVENT_DEBUT:
      printf(HDR VALS,
	     "debut", msg->ev_token, msg->ev_sequence,
	     "object",		hans1);
      break;

    case DM_EVENT_CREATE:
      printf(HDR VALS VALS VALS,
	     "create", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "name",		nams1,
	     "mode bits",	type);
      break;

    case DM_EVENT_POSTCREATE:
      printf(HDR VALS VALS VALS VALS VALD,
	     "postcreate", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "new object",	hans2,
	     "name",		nams1,
	     "mode bits",	type,
	     "retcode",		msg_ne->ne_retcode);
      respond = 0;
      break;

    case DM_EVENT_REMOVE:
      printf(HDR VALS VALS VALS,
	     "remove", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "name",		nams1,
	     "mode bits",	type);
      break;

    case DM_EVENT_POSTREMOVE:
      printf(HDR VALS VALS VALS VALD,
	     "postremove", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "name",		nams1,
	     "mode bits",	type,
	     "retcode",		msg_ne->ne_retcode);
      respond = 0;
      break;

    case DM_EVENT_RENAME:
      printf(HDR VALS VALS VALS VALS,
	     "rename", msg->ev_token, msg->ev_sequence,
	     "old parent",	hans1,
	     "new parent",	hans2,
	     "old name",	nams1,
	     "new name",	nams2);
      break;

    case DM_EVENT_POSTRENAME:
      printf(HDR VALS VALS VALS VALS VALD,
	     "postrename", msg->ev_token, msg->ev_sequence,
	     "old parent",	hans1,
	     "new parent",	hans2,
	     "old name",	nams1,
	     "new name",	nams2,
	     "retcode",		msg_ne->ne_retcode);
      respond = 0;
      break;

    case DM_EVENT_SYMLINK:
      printf(HDR VALS VALS VALS,
	     "symlink", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "name",		nams1,
	     "contents",	nams2);
      break;

    case DM_EVENT_POSTSYMLINK:
      printf(HDR VALS VALS VALS VALS VALD,
	     "postsymlink", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "new object",	hans2,
	     "name",		nams1,
	     "contents",	nams2,
	     "retcode",		msg_ne->ne_retcode);
      respond = 0;
      break;

    case DM_EVENT_LINK:
      printf(HDR VALS VALS VALS,
	     "link", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "source",		hans2,
	     "name",		nams1);
      break;

    case DM_EVENT_POSTLINK:
      printf(HDR VALS VALS VALS VALD,
	     "postlink", msg->ev_token, msg->ev_sequence,
	     "parent dir",	hans1,
	     "source",		hans2,
	     "name",		nams1,
	     "retcode",		msg_ne->ne_retcode);
      respond = 0;
      break;

    case DM_EVENT_ATTRIBUTE:
      printf(HDR VALS,
	     "attribute", msg->ev_token, msg->ev_sequence,
	     "object",		hans1);
      respond = 0;
      break;

    case DM_EVENT_CLOSE:
      printf(HDR VALS,
	     "close", msg->ev_token, msg->ev_sequence,
	     "object",		hans1);
      respond = 0;
      break;

    default:
      pkt_error++;
      printf(HDR VALD,
	     "<UNKNOWN>", msg->ev_token, msg->ev_sequence,
	     "ev_type",		msg->ev_type);
      if (msg->ev_token == DM_INVALID_TOKEN)
	respond = 0;
      break;
    }
  }

  /*
   * Now respond to those messages which require a response
   */
  if (respond) {
    if (Sleep) sleep(Sleep); /* Slow things down here */

    error = dm_respond_event(sid, msg->ev_token, response, respcode, 0, 0);
    if (error) {
      errno_msg("Can't respond to event");
      return error;
    }
  }

  return 0;
}


/*
	Convert a mode_t field into a printable string.

	Returns non-zero if the mode_t is invalid.  The string is
 	returned in *ptr, whether there is an error or not.
*/

static int
format_mode(
	mode_t	mode,
	char	**ptr)
{
static	char	modestr[100];
	char	*typestr;
	int	error = 0;

	if     (S_ISFIFO(mode)) typestr = "FIFO";
	else if(S_ISCHR (mode)) typestr = "Character Device";
	else if(S_ISBLK (mode)) typestr = "Block Device";
	else if(S_ISDIR (mode)) typestr = "Directory";
	else if(S_ISREG (mode)) typestr = "Regular File";
	else if(S_ISLNK (mode)) typestr = "Symbolic Link";
	else if(S_ISSOCK(mode)) typestr = "Socket";
	else {
		typestr = "<unknown type>"; 
		error++;
	}

	sprintf(modestr, "mode %06o (perm %c%c%c %c%c%c %c%c%c %c%c%c) "
		"type %s",
		mode,
		mode & S_ISUID ? 's':' ',
		mode & S_ISGID ? 'g':' ',
		mode & S_ISVTX ? 't':' ',
		mode & S_IRUSR ? 'r':'-',
		mode & S_IWUSR ? 'w':'-',
		mode & S_IXUSR ? 'x':'-',
		mode & S_IRGRP ? 'r':'-',
		mode & S_IWGRP ? 'w':'-',
		mode & S_IXGRP ? 'x':'-',
		mode & S_IROTH ? 'r':'-',
		mode & S_IWOTH ? 'w':'-',
		mode & S_IXOTH ? 'x':'-',
		typestr);
	*ptr = modestr;
	return(error);
}


static int
get_fs_handle(
	char	*fsname,
	void	**fs_hanpp,
	size_t	*fs_hlenp)
{
	char	hans[HANDLE_STR];

	if (dm_path_to_fshandle(fsname, fs_hanpp, fs_hlenp) == -1) {
		errno_msg("Can't get filesystem handle");
		return 1;
	}
	if (Verbose) {
		hantoa(*fs_hanpp, *fs_hlenp, hans);
		err_msg("File system handle for %s: %s\n", fsname, hans);
	}
	return 0;
}


/*
	Set the event disposition for this filesystem to include all valid
	DMAPI events so that we receive all events for this filesystem.
	Also set DM_EVENT_MOUNT disposition for the global handle.
	It does not make sense to specify DM_EVENT_USER in the disposition
	mask since a session is always unconditionally registered for these
	events.

	Returns non-zero on error.
*/

static int
set_disposition(
	dm_sessid_t	 sid,
	void		*fs_hanp,
	size_t		 fs_hlen)
{
	dm_eventset_t	eventlist;

	if (Verbose) {
		err_msg("Setting event disposition to send all "
			"events to this session\n");
	}

	/* DM_EVENT_MOUNT must be sent in a separate request using the global
	   handle.  If we ever support more than one filesystem at a time, this
	   request should be moved out of this routine to a place where it is
	   issued just once.
	*/

	DMEV_ZERO(eventlist);
	DMEV_SET(DM_EVENT_MOUNT, eventlist);

	if (dm_set_disp(sid, DM_GLOBAL_HANP, DM_GLOBAL_HLEN, DM_NO_TOKEN,
			&eventlist, DM_EVENT_MAX) == -1) {
		errno_msg("Can't set event disposition for mount");
		return(1);
	}

	DMEV_ZERO(eventlist);

	/* File system administration events. */

	DMEV_SET(DM_EVENT_PREUNMOUNT, eventlist);
	DMEV_SET(DM_EVENT_UNMOUNT, eventlist);
	DMEV_SET(DM_EVENT_NOSPACE, eventlist);

	/* While DM_EVENT_DEBUT is optional, it appears that the spec always
	   lets it be specified in a dm_set_disp call; its just that the
	   event will never be seen on some platforms.
	*/

	DMEV_SET(DM_EVENT_DEBUT, eventlist);


	/* Namespace events. */

	DMEV_SET(DM_EVENT_CREATE, eventlist);
	DMEV_SET(DM_EVENT_POSTCREATE, eventlist);
	DMEV_SET(DM_EVENT_REMOVE, eventlist);
	DMEV_SET(DM_EVENT_POSTREMOVE, eventlist);
	DMEV_SET(DM_EVENT_RENAME, eventlist);
	DMEV_SET(DM_EVENT_POSTRENAME, eventlist);
	DMEV_SET(DM_EVENT_LINK, eventlist);
	DMEV_SET(DM_EVENT_POSTLINK, eventlist);
	DMEV_SET(DM_EVENT_SYMLINK, eventlist);
	DMEV_SET(DM_EVENT_POSTSYMLINK, eventlist);

	/* Managed region data events. */

	DMEV_SET(DM_EVENT_READ, eventlist);
	DMEV_SET(DM_EVENT_WRITE, eventlist);
	DMEV_SET(DM_EVENT_TRUNCATE, eventlist);

	/* Metadata events. */

	DMEV_SET(DM_EVENT_ATTRIBUTE, eventlist);
#if 	!defined(VERITAS) && !defined(linux)
	DMEV_SET(DM_EVENT_CANCEL, eventlist);
#endif
#if !defined(linux)
	DMEV_SET(DM_EVENT_CLOSE, eventlist);
#endif
	DMEV_SET(DM_EVENT_DESTROY, eventlist);

	/* Pseudo-events. */

	/* DM_EVENT_USER - always enabled - causes EINVAL if specified */

	if (dm_set_disp(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
			&eventlist, DM_EVENT_MAX) == -1) {
		errno_msg("Can't set event disposition for filesystem");
		return(1);
	}
	return(0);
}


/*
	Enable event generation on each valid filesystem-based DMAPI event
	within the given file system.

	Returns non-zero on errors.
*/

static int
set_events(
	dm_sessid_t	 sid,
	void		*fs_hanp,
	size_t		 fs_hlen)
{
	dm_eventset_t	eventlist;

	if (Verbose) {
		err_msg("Setting event list to enable all events "
			"for this file system\n");
	}
	DMEV_ZERO(eventlist);

	/* File system administration events. */

	/* DM_EVENT_MOUNT - always enabled on the global handle - causes
	   EINVAL if specified.
	*/
	DMEV_SET(DM_EVENT_PREUNMOUNT, eventlist);
	DMEV_SET(DM_EVENT_UNMOUNT, eventlist);
	DMEV_SET(DM_EVENT_NOSPACE, eventlist);
	/* DM_EVENT_DEBUT - always enabled - causes EINVAL if specified. */

	/* Namespace events. */

	DMEV_SET(DM_EVENT_CREATE, eventlist);
	DMEV_SET(DM_EVENT_POSTCREATE, eventlist);
	DMEV_SET(DM_EVENT_REMOVE, eventlist);
	DMEV_SET(DM_EVENT_POSTREMOVE, eventlist);
	DMEV_SET(DM_EVENT_RENAME, eventlist);
	DMEV_SET(DM_EVENT_POSTRENAME, eventlist);
	DMEV_SET(DM_EVENT_LINK, eventlist);
	DMEV_SET(DM_EVENT_POSTLINK, eventlist);
	DMEV_SET(DM_EVENT_SYMLINK, eventlist);
	DMEV_SET(DM_EVENT_POSTSYMLINK, eventlist);

	 /* Managed region data events.  These are not settable by
	    dm_set_eventlist on a filesystem basis.   They are meant
	    to be set using dm_set_region on regular files only.
	    However, in the SGI implementation, they are filesystem-settable.
	    Since this is useful for testing purposes, do it.
	*/

	/* DM_EVENT_READ */
	/* DM_EVENT_WRITE */
	/* DM_EVENT_TRUNCATE */

	/* Metadata events. */

	DMEV_SET(DM_EVENT_ATTRIBUTE, eventlist);
#if 	!defined(VERITAS) && !defined(linux)
	DMEV_SET(DM_EVENT_CANCEL, eventlist);
#endif
#if !defined(linux)
	DMEV_SET(DM_EVENT_CLOSE, eventlist);
#endif
	DMEV_SET(DM_EVENT_DESTROY, eventlist);

	/* Pseudo-events. */

	/* DM_EVENT_USER - always enabled - causes EINVAL if specified */

	if (dm_set_eventlist(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
			&eventlist, DM_EVENT_MAX) == -1) {
		errno_msg("Can't set event list");
		return(1);
	}
	return(0);
}


/*
	Disable monitoring for all events in the DMAPI for the given
	file system.  This is done before exiting so that future
	operations won't hang waiting for their events to be handled.

	Returns non-zero on errors.
*/

static int
clear_events(
	dm_sessid_t	 sid,
	void		*fs_hanp,
	size_t		 fs_hlen)
{
	dm_eventset_t	eventlist;

	if (Verbose) {
		err_msg("Clearing event list to disable all events "
			"for this filesystem\n");
	}
	DMEV_ZERO(eventlist);

	if (dm_set_eventlist(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
			&eventlist, DM_EVENT_MAX) == -1) {
		errno_msg("Can't clear event list");
		return(1);
	}
	return(0);
}


/*
 * Respond to any events which haven't been handled yet.
 * dm_getall_tokens provides a list of tokens for the outstanding events.
 * dm_find_eventmsg uses the token to lookup the corresponding message.
 * The message is passed to handle_message() for normal response processing.
 */
int
finish_responding(
		  dm_sessid_t	sid)
{
  int		error = 0;
  u_int		nbytes, ntokens = 0, ret_ntokens, i;
  dm_token_t	*tokenbuf = NULL, *tokenptr;
  size_t	buflen = 0, ret_buflen;
  char		*msgbuf = NULL;
  dm_eventmsg_t	*msg;

  if (Verbose)
    err_msg("Responding to any outstanding delivered event messages\n");

  /*
   * Initial sizes for the token and message buffers
   */
  ret_buflen = 16 * (sizeof(dm_eventmsg_t) + sizeof(dm_data_event_t)
		     + HANDLE_LEN);
  ret_ntokens = 16;

  /*
   * The E2BIG dance...
   * Take a guess at how large to make the buffer, starting with ret_ntokens.
   * If the routine returns E2BIG, use the returned size and try again.
   * If we're already using the returned size, double it and try again.
   */
  do {
    dm_token_t *tmpbuf;
    ntokens = (ntokens != ret_ntokens) ? ret_ntokens : ntokens*2;
    nbytes = ntokens * (sizeof(dm_token_t) + sizeof(dm_vardata_t));
    tmpbuf = realloc(tokenbuf, nbytes);
    if (tmpbuf == NULL) {
      err_msg("Can't malloc %d bytes for tokenbuf\n", nbytes);
      error = 1;
      goto out;
    }
    tokenbuf = tmpbuf;
    error = dm_getall_tokens(sid, ntokens, tokenbuf, &ret_ntokens);
  } while (error && errno == E2BIG);

  if (error) {
    errno_msg("Can't get all outstanding tokens");
    goto out;
  }

  tokenptr = tokenbuf;
  for (i = 0; i < ret_ntokens; i++) {
    if (Verbose)
      err_msg("Responding to outstanding event for token %d\n",(int)*tokenptr);

    /*
     * The E2BIG dance reprise...
     */
    do {
      char *tmpbuf;
      buflen = (buflen != ret_buflen) ? ret_buflen : buflen * 2;
      tmpbuf = realloc(msgbuf, buflen);
      if (tmpbuf == NULL) {
	err_msg("Can't malloc %d bytes for msgbuf\n", buflen);
	error = 1;
	goto out;
      }
      msgbuf = tmpbuf;
      error = dm_find_eventmsg(sid, *tokenptr, buflen, msgbuf, &ret_buflen);
    } while (error && errno == E2BIG);
    if (error) {
      errno_msg("Can't find the event message for token %d", (int)*tokenptr);
      goto out;
    }

    msg = (dm_eventmsg_t *) msgbuf;
    while (msg != NULL) {
      error = handle_message(sid, msg);
      if (error)
	goto out;
      msg = DM_STEP_TO_NEXT(msg, dm_eventmsg_t *);
    }

    tokenptr++;
  }

 out:
  if (tokenbuf)
    free(tokenbuf);
  if (msgbuf)
    free(msgbuf);
  return error;
}


/*
 * Establish an exit handler since we run in an infinite loop
 */
int
establish_handler(void)
{
  struct sigaction	act;

  /*
   * Set up signals so that we can wait for spawned children
   */
  act.sa_handler = exit_handler;
  act.sa_flags   = 0;
  sigemptyset(&act.sa_mask);

  (void)sigaction(SIGHUP, &act, NULL);
  (void)sigaction(SIGINT, &act, NULL);
  (void)sigaction(SIGQUIT, &act, NULL);
  (void)sigaction(SIGTERM, &act, NULL);
  (void)sigaction(SIGUSR1, &act, NULL);
  (void)sigaction(SIGUSR1, &act, NULL);
  (void)sigaction(SIGUSR2, &act, NULL);

  return(0);
}


/*
 * Exit gracefully
 *
 * Stop events from being generated for the given file system
 * Respond to any events that were delivered but unanswered
 *  (the event loop may have been in the middle of taking care of the event)
 * Try getting any undelivered events but don't block if none are there
 *  (the file system may have generated an event after we killed dm_get_events)
 * Shutdown the session using the global "sid" variable.
 */
void
exit_handler(int x)
{
  int		error;
  void		*fs_hanp;
  size_t	fs_hlen;

  if (Verbose)
    printf("\n"),
    err_msg("Exiting...\n");

  error = get_fs_handle(fsname, &fs_hanp, &fs_hlen);

  if (!error) {
    error = clear_events(sid, fs_hanp, fs_hlen);
    if (error)
      /* just keep going */ ;
  }

  error = finish_responding(sid);
  if (error)
    /* just keep going */ ;

  err_msg("Processing any undelivered event messages\n");
  event_loop(sid, 0 /*waitflag*/);

  err_msg("Shutting down the session\n");
  if (sid != 0) {
    error = dm_destroy_session(sid);
    if (error == -1) {
      errno_msg("Can't shut down session - use 'mrmean -kv' to clean up!");
    }
  }

  exit(0);
}
