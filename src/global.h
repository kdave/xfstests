/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
 
#ifndef GLOBAL_H
#define GLOBAL_H

#include <config.h>

#ifdef HAVE_XFS_LIBXFS_H
#include <xfs/libxfs.h>
#endif

#ifdef HAVE_XFS_JDM_H
#include <xfs/jdm.h>
#endif

#ifdef HAVE_ATTR_ATTRIBUTES_H
#include <attr/attributes.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_ATTRIBUTES_H
#include <sys/attributes.h>
#endif

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif

#ifdef STDC_HEADERS
#include <signal.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYS_SYSSGI_H
#include <sys/syssgi.h>
#endif

#ifdef HAVE_SYS_UUID_H
#include <sys/uuid.h>
#endif

#ifdef HAVE_SYS_FS_XFS_FSOPS_H
#include <sys/fs/xfs_fsops.h>
#endif

#ifdef HAVE_SYS_FS_XFS_ITABLE_H
#include <sys/fs/xfs_itable.h>
#endif

#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#endif

#ifdef sgi
#define xfs_flock64 flock64
#define xfs_flock64_t struct flock64

#define XFS_IOC_DIOINFO                F_DIOINFO                       
#define XFS_IOC_FSGETXATTR             F_FSGETXATTR                   
#define XFS_IOC_FSSETXATTR             F_FSSETXATTR                    
#define XFS_IOC_ALLOCSP64              F_ALLOCSP64                    
#define XFS_IOC_FREESP64               F_FREESP64                 
#define XFS_IOC_GETBMAP                F_GETBMAP                 
#define XFS_IOC_FSSETDM                F_FSSETDM                 
#define XFS_IOC_RESVSP                 F_RESVSP                 
#define XFS_IOC_UNRESVSP               F_UNRESVSP                 
#define XFS_IOC_RESVSP64               F_RESVSP64                 
#define XFS_IOC_UNRESVSP64             F_UNRESVSP64                 
#define	XFS_IOC_GETBMAPA               F_GETBMAPA                 
#define	XFS_IOC_FSGETXATTRA            F_FSGETXATTRA                         
#define	XFS_IOC_GETBMAPX               F_GETBMAPX
                   
#define XFS_IOC_FSGEOMETRY_V1          XFS_FS_GEOMETRY_V1          
#define XFS_IOC_FSBULKSTAT             SGI_FS_BULKSTAT     
#define XFS_IOC_FSBULKSTAT_SINGLE      SGI_FS_BULKSTAT_SINGLE
#define XFS_IOC_FSINUMBERS             /* TODO */
#define XFS_IOC_PATH_TO_FSHANDLE       /* TODO */		         
#define XFS_IOC_PATH_TO_HANDLE         /* TODO */		       
#define XFS_IOC_FD_TO_HANDLE           /* TODO */
#define XFS_IOC_OPEN_BY_HANDLE         /* TODO */
#define XFS_IOC_READLINK_BY_HANDLE     /* TODO */
#define XFS_IOC_SWAPEXT                /* TODO */
#define XFS_IOC_FSGROWFSDATA           XFS_GROWFS_DATA      
#define XFS_IOC_FSGROWFSLOG            XFS_GROWFS_LOG      
#define XFS_IOC_FSGROWFSRT             XFS_GROWFS_RT      
#define XFS_IOC_FSCOUNTS               XFS_FS_COUNTS      
#define XFS_IOC_SET_RESBLKS            XFS_SET_RESBLKS      
#define XFS_IOC_GET_RESBLKS            XFS_GET_RESBLKS      
#define XFS_IOC_ERROR_INJECTION        SGI_XFS_INJECT_ERROR
#define XFS_IOC_ERROR_CLEARALL         SGI_XFS_CLEARALL_ERROR
#define XFS_IOC_FREEZE                 XFS_FS_FREEZE      
#define XFS_IOC_THAW                   XFS_FS_THAW      
#define XFS_IOC_FSSETDM_BY_HANDLE      /* TODO */    
#define XFS_IOC_ATTRLIST_BY_HANDLE     /* TODO */      
#define XFS_IOC_ATTRMULTI_BY_HANDLE    /* TODO */      
#define XFS_IOC_FSGEOMETRY             XFS_FS_GEOMETRY      
#define XFS_IOC_GOINGDOWN              XFS_FS_GOINGDOWN

typedef struct xfs_error_injection {
        __int32_t           fd;
        __int32_t           errtag;
} xfs_error_injection_t;


typedef struct xfs_fsop_bulkreq {
        ino64_t             *lastip;      
        __int32_t           icount;   
        xfs_bstat_t         *ubuffer;      
        __int32_t           *ocount;       
} xfs_fsop_bulkreq_t;

static __inline__ int 
xfsctl(char* path, int fd, int cmd, void* arg) {
  if (cmd >= 0 && cmd < XFS_FSOPS_COUNT) {
    /*
     * We have a problem in that xfsctl takes 1 arg but
     * some sgi xfs ops take an input arg and/or an output arg
     * So have to special case the ops to decide if xfsctl arg
     * is an input or an output argument.
     */
    if (cmd == XFS_FS_GOINGDOWN) {
      /* in arg */
      return syssgi(SGI_XFS_FSOPERATIONS, fd, cmd, arg, 0);
    } else {
      /* out arg */
      return syssgi(SGI_XFS_FSOPERATIONS, fd, cmd, 0, arg);
    }
  } else if (cmd == SGI_FS_BULKSTAT)
    return syssgi(SGI_FS_BULKSTAT, fd, 
		  ((xfs_fsop_bulkreq_t*)arg)->lastip,
		  ((xfs_fsop_bulkreq_t*)arg)->icount,
		  ((xfs_fsop_bulkreq_t*)arg)->ubuffer);
  else if (cmd == SGI_FS_BULKSTAT_SINGLE)
    return syssgi(SGI_FS_BULKSTAT_SINGLE, fd, 
		  ((xfs_fsop_bulkreq_t*)arg)->lastip,
		  ((xfs_fsop_bulkreq_t*)arg)->ubuffer);
  else if (cmd == SGI_XFS_INJECT_ERROR)
    return syssgi(SGI_XFS_INJECT_ERROR,
		  ((xfs_error_injection_t*)arg)->errtag, fd);
  else if (cmd == SGI_XFS_CLEARALL_ERROR)
    return syssgi(SGI_XFS_CLEARALL_ERROR, fd);
  else
    return fcntl(fd, cmd, arg);
}

#endif /* sgi */                           
