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
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>

#include <lib/hsm.h>

#include <string.h>

  /*
   * Define some standard formats for the printf statements below.
   */

#define HDR  "type=%s token=%d sequence=%d\n"
#define VALS "\t%s=%s\n"
#define VALD "\t%s=%d\n"
#define VALLLD "\t%s=%lld\n"


/*
	Convert a mode_t field into a printable string.

	Returns non-zero if the mode_t is invalid.  The string is
 	returned in *ptr, whether there is an error or not.
*/

int
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

	sprintf(modestr, "mode %06o: perm %c%c%c %c%c%c %c%c%c %c%c%c, type %s",
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

#if	VERITAS_21
	dm_namesp_event_t  *msg_ne = (dm_namesp_event_t *)msg;

/*
	msg_ne = DM_GET_VALUE(msg, ev_data, dm_namesp_event_t *);
*/
	hanp1  = DM_GET_VALUE(msg_ne, ne_handle1, void *);
	hlen1  = DM_GET_LEN  (msg_ne, ne_handle1);
	hanp2  = DM_GET_VALUE(msg_ne, ne_handle2, void *);
	hlen2  = DM_GET_LEN  (msg_ne, ne_handle2);
	namp1  = DM_GET_VALUE(msg_ne, ne_name1, void *);
	nlen1  = DM_GET_LEN  (msg_ne, ne_name1);
	namp2  = DM_GET_VALUE(msg_ne, ne_name2, void *);
	nlen2  = DM_GET_LEN  (msg_ne, ne_name2);
	hanp3  = NULL;
	hlen3  = 0;
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
#endif	/* VERITAS_21 */

	if (hanp1 && hlen1) {
		hantoa(hanp1, hlen1, hans1);
	} else {
		sprintf(hans1, "<BAD HANDLE, hlen %zd>", hlen1);
	}
	if (hanp2 && hlen2) {
		hantoa(hanp2, hlen2, hans2);
	} else {
		sprintf(hans2, "<BAD HANDLE, hlen %zd>", hlen2);
	}
	if (hanp3 && hlen3) {
		hantoa(hanp3, hlen3, hans3);
	} else {
		sprintf(hans3, "<BAD HANDLE, hlen %zd>", hlen3);
	}
	if (namp1 && nlen1) {
		strncpy(nams1, namp1, nlen1);
		if (nlen1 != sizeof(nams1))
			nams1[nlen1] = '\0';
	} else {
		sprintf(nams1, "<BAD STRING, nlen %zd>", nlen1);
	}
	if (namp2 && nlen2) {
		strncpy(nams2, namp2, nlen2);
		if (nlen2 != sizeof(nams2))
			nams2[nlen2] = '\0';
	} else {
		sprintf(nams2, "<BAD STRING, nlen %zd>", nlen2);
	}

	printf(VALS VALS VALS VALS VALS VALD,
	     "fs handle",	hans1,
	     "mtpt handle",	hans2,
	     "mtpt path",	nams1,
	     "media desig",	nams2,
	     "root handle",	hans3,
    	     "mode",		mode);
}


static int
print_one_data_event(
	dm_data_event_t	*msg_de)
{
	char		handle[HANDLE_STR];
	void		*hanp;
	size_t		hlen;

	hanp  = DM_GET_VALUE(msg_de, de_handle, void *);
	hlen  = DM_GET_LEN  (msg_de, de_handle);

	if (hanp && hlen) {
		hantoa(hanp, hlen, handle);
	} else {
		sprintf(handle, "<BAD HANDLE, hlen %zd>", hlen);
	}

	printf(VALS VALLLD VALLLD,
		"file_handle",	handle,
		"offset", (long long) msg_de->de_offset,
		"length", (long long) msg_de->de_length);

	return(0);
}


int
print_one_message(
	dm_eventmsg_t	*msg)
{
	int		pkt_error = 0;
	dm_namesp_event_t  *msg_ne;
	void		*hanp1, *hanp2, *namp1, *namp2;
	u_int		hlen1, hlen2, nlen1, nlen2;
	char		hans1[HANDLE_STR], hans2[HANDLE_STR];
	char		nams1[NAME_MAX + 1], nams2[NAME_MAX + 1];

	/***** USER EVENTS *****/

	if (msg->ev_type == DM_EVENT_USER) {
    char	*privp;
    u_int	plen, i;

    printf(HDR,
		"user", msg->ev_token, msg->ev_sequence);

    /* print private data as ascii or hex if it exists DM_CONFIG_MAX_MESSAGE_DATA */

    privp = DM_GET_VALUE(msg, ev_data, char *);
    plen  = DM_GET_LEN  (msg, ev_data);
    if (plen) {
	for (i = 0; i < plen; i++) {
		if (!isprint(privp[i]) && !isspace(privp[i]))
			break;
	}
	if (i == plen - 1 && privp[i] == '\0') {
	  printf(VALS,
			"privdata", privp);
	} else {
          printf("\t%-15s ", "privdata");
          for (i = 0; i < plen; i++) {
	    printf("%.2x", privp[i]);
          }
          printf("\n");
	}
    } else {
	printf(VALS,
		"privdata", "<NONE>");
    }

	/***** CANCEL EVENT *****/

/* Not implemented on SGI or Veritas */

	} else if (msg->ev_type == DM_EVENT_CANCEL) {
		dm_cancel_event_t	*msg_ce;

		msg_ce = DM_GET_VALUE(msg, ev_data, dm_cancel_event_t *);
		printf(HDR VALD VALD,
			"cancel", msg->ev_token, msg->ev_sequence,
			"sequence", msg_ce->ce_sequence,
			"token", msg_ce->ce_token);

	/***** DATA EVENTS *****/

	} else if (msg->ev_type == DM_EVENT_READ ||
		   msg->ev_type == DM_EVENT_WRITE ||
		   msg->ev_type == DM_EVENT_TRUNCATE) {
		dm_data_event_t	*msg_de;

		msg_de = DM_GET_VALUE(msg, ev_data, dm_data_event_t *);

		switch (msg->ev_type) {
		case DM_EVENT_READ:
			printf(HDR, "read", msg->ev_token, msg->ev_sequence);
			break;

		case DM_EVENT_WRITE:
			printf(HDR, "write", msg->ev_token, msg->ev_sequence);
			break;

		case DM_EVENT_TRUNCATE:
			printf(HDR, "truncate", msg->ev_token,
				msg->ev_sequence);
			break;
		default: break;
		}
		print_one_data_event(msg_de);

  /***** DESTROY EVENT *****/

	} else if (msg->ev_type == DM_EVENT_DESTROY) {
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
      printf("\t%-15s ", "attrcopy");
      for (i = 0; i < clen; i++) {
	printf("%.2x", copy[i]);
      }
      printf("\n");
    } else {
      printf(VALS, "attrcopy", "<NONE>");
    }

  /***** MOUNT EVENT *****/

	} else if (msg->ev_type == DM_EVENT_MOUNT) {
		void 	*msg_body;

		printf(HDR, "mount", msg->ev_token, msg->ev_sequence);
#if	!VERITAS_21
		msg_body = DM_GET_VALUE(msg, ev_data, dm_mount_event_t *);
#else	/* VERITAS_21 */
		msg_body = DM_GET_VALUE(msg, ev_data, dm_namesp_event_t *);
#endif	/* VERITAS_21 */
		print_one_mount_event(msg_body);

  /***** NAMESPACE EVENTS *****/

	} else {
    char	*type;

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
#if	!VERITAS_21
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
      break;

    case DM_EVENT_DEBUT:		/* not supported on SGI */
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
      break;

    case DM_EVENT_ATTRIBUTE:
      printf(HDR VALS,
	     "attribute", msg->ev_token, msg->ev_sequence,
	     "object",		hans1);
      break;

    case DM_EVENT_CLOSE:	/* not supported on SGI */
      printf(HDR VALS,
	     "close", msg->ev_token, msg->ev_sequence,
	     "object",		hans1);
      break;

    default:
      pkt_error++;
      printf(HDR VALD,
	     "<UNKNOWN>", msg->ev_token, msg->ev_sequence,
	     "ev_type",		msg->ev_type);
      break;
    }
	}
	return(pkt_error);
}


int
handle_message(
	dm_sessid_t	sid,
	dm_eventmsg_t	*msg)
{
	dm_response_t	response;
	int		respond, respcode;
	int		error = 0;

	if (print_one_message(msg))
		error++;

	/* Set the defaults for responding to events. */

	respond = 1;
	response = DM_RESP_CONTINUE;
	respcode = 0;

	/***** USER EVENTS *****/

	switch (msg->ev_type) {
	case DM_EVENT_USER:
		if (msg->ev_token == DM_INVALID_TOKEN)
			respond = 0;
		break;

	case DM_EVENT_CANCEL:
	case DM_EVENT_DESTROY:
	case DM_EVENT_POSTCREATE:
	case DM_EVENT_POSTREMOVE:
	case DM_EVENT_POSTRENAME:
	case DM_EVENT_POSTSYMLINK:
	case DM_EVENT_POSTLINK:
	case DM_EVENT_ATTRIBUTE:
	case DM_EVENT_CLOSE:
		respond = 0;
		break;

	case DM_EVENT_MOUNT:
	case DM_EVENT_READ:
	case DM_EVENT_WRITE:
	case DM_EVENT_TRUNCATE:
	case DM_EVENT_PREUNMOUNT:
	case DM_EVENT_UNMOUNT:
	case DM_EVENT_DEBUT:
	case DM_EVENT_CREATE:
	case DM_EVENT_REMOVE:
	case DM_EVENT_RENAME:
	case DM_EVENT_SYMLINK:
	case DM_EVENT_LINK:
		break;

	case DM_EVENT_NOSPACE:
		response = DM_RESP_ABORT;
		respcode = ENOSPC;
		break;

	default:
		if (msg->ev_token == DM_INVALID_TOKEN)
			respond = 0;
		break;
	}

	/* Respond to those messages which require a response. */

	if (respond) {
		if (dm_respond_event(sid, msg->ev_token, response, respcode, 0, 0)) {
			errno_msg("Can't respond to event");
		}
	}
	return(error);
}
