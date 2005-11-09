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
#ifndef __ERRTEST_SEEN
#define __ERRTEST_SEEN

#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_ERRS 100

void* handle_clone(void* src_hanp, u_int hlen );

char	*
errno_names[] = {
	"ERROR_0",
	"EPERM",
	"ENOENT",
	"ESRCH",
	"EINTR",
	"EIO",
	"ENXIO",
	"E2BIG",
	"ENOEXEC",
	"EBADF",
	"ECHILD",
	"EAGAIN",
	"ENOMEM",
	"EACCES",
	"EFAULT",
	"ENOTBLK",
	"EBUSY",
	"EEXIST",
	"EXDEV",
	"ENODEV",
	"ENOTDIR",
	"EISDIR",
	"EINVAL",
	"ENFILE",
	"EMFILE",
	"ENOTTY",
	"ETXTBSY",
	"EFBIG",
	"ENOSPC",
	"ESPIPE",
	"EROFS",
	"EMLINK",
	"EPIPE",
	"EDOM",
	"ERANGE",
	"ENOMSG",
	"EIDRM",
	"ECHRNG",
	"EL2NSYNC",
	"EL3HLT",
	"EL3RST",
	"ELNRNG",
	"EUNATCH",
	"ENOCSI",
	"EL2HLT",
	"EDEADLK",
	"ENOLCK",
	"ERROR_47",
	"ERROR_48",
	"ERROR_49",
	"EBADE",
	"EBADR",
	"EXFULL",
	"ENOANO",
	"EBADRQC",
	"EBADSLT",
	"EDEADLOCK",
	"EBFONT",
	"ERROR_58",
	"ERROR_59",
	"ENOSTR",
	"ENODATA",
	"ETIME",
	"ENOSR",
	"ENONET",
	"ENOPKG",
	"EREMOTE",
	"ENOLINK",
	"EADV",
	"ESRMNT",
	"ECOMM",
	"EPROTO",
	"ERROR_72",
	"ERROR_73",
	"EMULTIHOP",
	"ERROR_75",
	"ERROR_76",
	"EBADMSG",
	"ENAMETOOLONG",
	"EOVERFLOW",
	"ENOTUNIQ",
	"EBADFD",
	"EREMCHG",
	"ELIBACC",
	"ELIBBAD",
	"ELIBSCN",
	"ELIBMAX",
	"ELIBEXEC",
	"EILSEQ",
	"ENOSYS",
	"ELOOP",
	"ERESTART",
	"ESTRPIPE",
	"ENOTEMPTY",
	"EUSERS",
	"ENOTSOCK",
	"EDESTADDRREQ",
	"EMSGSIZE",
	"EPROTOTYPE",
	"ENOPROTOOPT" };

#define ERR_NAME							\
        ((errno<NUM_ERRS)?(errno_names[errno]):("an unknown errno"))

#define ERRTEST(EXPECTED, NAME, FUNCTION)				\
    if ((FUNCTION) < 0) {						\
      if (errno == (EXPECTED)) {			       		\
        if (Vflag)							\
	  fprintf(stdout,"\treport on test for " #EXPECTED		\
                          " in %s: test successful\n", (NAME));		\
      }									\
      else {								\
        fprintf(stdout, "\tERROR testing for " #EXPECTED		\
		  " in %s: found %s.\n", (NAME), ERR_NAME);    		\
      }									\
    }						       			\
    else {								\
      fprintf(stdout, "\tERROR testing for " #EXPECTED	       		\
	      " in %s: no error was produced!\n", (NAME));		\
    }


#define EXCLTEST(NAME, HANP, HLEN, TOKEN, FUNCTION)                      \
if (dm_create_userevent(sid, 10, "DMAPI" NAME, &(TOKEN))) {		\
  fprintf(stdout,							\
	  "\tERROR: can't create token (%s); skipping EACCES test\n",   \
	  ERR_NAME);							\
}									\
else {									\
  ERRTEST(EACCES, "no-right " NAME, (FUNCTION))				\
									\
  if (dm_request_right(sid, (HANP), (HLEN), (TOKEN), 0, DM_RIGHT_SHARED))	\
    fprintf(stdout, "\t"NAME" ERROR: Couldn't upgrade to SHARED. %s\n",	\
	    ERR_NAME);							\
  else ERRTEST(EACCES, "SHARED " NAME, (FUNCTION))			\
									\
  if (dm_request_right(sid, (HANP), (HLEN), (TOKEN), 0, DM_RIGHT_EXCL))	\
    fprintf(stdout, "\t"NAME" ERROR: Couldn't upgrade to EXCL: %s\n",	\
	    ERR_NAME);							\
  else if ((FUNCTION) < 0)						\
    fprintf(stdout, "\t"NAME" ERROR: token with DM_RIGHT_EXCL was "	\
	    "denied access: %s\n", ERR_NAME);				\
  else if (Vflag)							\
    fprintf(stdout, "\treport on test for success in EXCL "NAME": "	\
	    "test successful.\n");					\
									\
  if (dm_respond_event(sid, (TOKEN), DM_RESP_CONTINUE, 0, 0, 0))	\
    fprintf(stdout, "\tERROR in responding to token: %s\n", ERR_NAME);	\
}



#define SHAREDTEST(NAME, HANP, HLEN,  TOKEN, FUNCTION)			\
if (dm_create_userevent(sid, 10, "DMAPI" NAME, &(TOKEN))) {		\
     fprintf(stdout,							\
	     "\tCannot create_userevent (%s); skipping EACCES test\n",	\
	     ERR_NAME);							\
}									\
else {									\
  ERRTEST(EACCES, "no-right " NAME, (FUNCTION))				\
									\
  if (dm_request_right(sid, (HANP), (HLEN), (TOKEN), 			\
		       0, DM_RIGHT_SHARED))				\
    fprintf(stdout, "\t"NAME" ERROR: Couldn't upgrade to EXCL. %s\n",	\
	    ERR_NAME);							\
  else if ((FUNCTION) < 0)						\
    fprintf(stdout, "\t" NAME" ERROR: token with DM_RIGHT_SHARED "	\
	    "was denied access. %s\n", ERR_NAME);			\
  else if (Vflag)							\
    fprintf(stdout, "\treport on test for success in SHARED "NAME": "	\
	    "test successful.\n");					\
									\
  if (dm_request_right(sid, (HANP), (HLEN), (TOKEN), 0, DM_RIGHT_EXCL))	\
    fprintf(stdout, "\t"NAME" ERROR: Couldn't upgrade to EXCL. %s\n",	\
	    ERR_NAME);							\
  else if ((FUNCTION) < 0)						\
      fprintf(stdout, "\t" NAME" ERROR: token with DM_RIGHT_EXCL was "	\
	      "denied access. %s\n", ERR_NAME);				\
  else if (Vflag)							\
    fprintf(stdout, "\treport on test for success in EXCL "NAME": "	\
	    "test successful.\n");					\
									\
  if (dm_respond_event(sid, (TOKEN), DM_RESP_CONTINUE, 0, 0, 0))	\
    fprintf(stderr, "\tERROR in responding to token: %s\n", ERR_NAME);	\
}



void* 
handle_clone(void* src_hanp, u_int hlen )
{
  void* dest_hanp;

  if ((dest_hanp = malloc(hlen)) == NULL) return(NULL);
  bcopy(src_hanp, dest_hanp, hlen);
  return(dest_hanp);
}

#endif

