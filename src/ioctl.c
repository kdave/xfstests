/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <libxfs.h>
#include <jdm.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/* simple test program to try out  a bunch of ioctls:
 *     XFS_IOC_FSCOUNTS
 *     XFS_IOC_GET_RESBLKS
 *     XFS_IOC_SET_RESBLKS
 *     XFS_IOC_PATH_TO_FSHANDLE
 *     XFS_IOC_PATH_TO_HANDLE
 *     XFS_IOC_FD_TO_HANDLE
 *     XFS_IOC_OPEN_BY_HANDLE
 *     XFS_IOC_READLINK_BY_HANDLE
*/

    
void fscounts(int fsfd)
{
    xfs_fsop_counts_t   counts;
    int                 ret;
    
    ret=ioctl(fsfd, XFS_IOC_FSCOUNTS, &counts);
    if (ret) {
        perror("ioctl(XFS_IOC_FSCOUNTS)");
        exit(1);
    }

    printf("XFS_IOC_FSCOUNTS-\n    freedata: %lld freertx: %lld freeino: %lld allocino: %lld\n",
            counts.freedata, counts.freertx, counts.freeino, counts.allocino);
}
    
__u64 getresblks(int fsfd)
{
    xfs_fsop_resblks_t  res;
    int                 ret;
    
    ret=ioctl(fsfd, XFS_IOC_GET_RESBLKS, &res);
    if (ret) {
        perror("ioctl(XFS_IOC_GET_RESBLKS)");
        exit(1);
    }
    
    printf("XFS_IOC_GET_RESBLKS-\n    resblks: %lld blksavail: %lld\n",
            res.resblks, res.resblks_avail);
    
    return res.resblks;
}
    
__u64 setresblks(int fsfd, __u64 blks)
{
    xfs_fsop_resblks_t  res;
    int                 ret;
    
    res.resblks=blks;
    ret=ioctl(fsfd, XFS_IOC_SET_RESBLKS, &res);
    if (ret) {
        perror("ioctl(XFS_IOC_SET_RESBLKS)");
        exit(1);
    }
    
    printf("XFS_IOC_SET_RESBLKS-\n    resblks: %lld blksavail: %lld\n",
            res.resblks, res.resblks_avail);
    
    return res.resblks;
}

void handle_print(void *handle, int handlen)
{
    char *p=handle;
    if (!handle||!handlen) {
        printf("%s",handle?"<zero len>":"<NULL>");
        return;
    };
    
    printf("#");
    while (handlen--)
        printf("%02x", *p++);   
}

void stat_print(int fd)
{
    struct stat buf;
    
    if (fstat(fd, &buf)) {
        perror("stat");
        exit(1);
    }
    printf("dev: %llu ino: %llu mode: %o\n",
            (__u64)buf.st_dev, (__u64)buf.st_ino, buf.st_mode);
}
    

void handle(int fsfd, char *path)
{
    xfs_fsop_handlereq_t    handle;
    char                    buffer[1024];
    char                    link[1024];
    int                     ret;
    __u32                   len;
    __u32                   linklen;
    int                     fd;
    
    handle.path=path;
    handle.ohandle=buffer;
    handle.ohandlen=&len;
    ret=ioctl(fsfd, XFS_IOC_PATH_TO_FSHANDLE, &handle);
    if (ret) {
        perror("ioctl(XFS_IOC_PATH_TO_FSHANDLE)");
        exit(1);
    }
    printf("XFS_IOC_PATH_TO_FSHANDLE-\n    handle: ");
    handle_print(handle.ohandle, *handle.ohandlen);
    printf("\n");
    
    fd=open(path,O_RDONLY);
    if (fd<0) {
        perror("open");
        exit(1);
    }
    handle.path=NULL;
    handle.fd=fd;
    handle.ohandle=buffer;
    handle.ohandlen=&len;
    ret=ioctl(fsfd, XFS_IOC_FD_TO_HANDLE, &handle);
    if (ret) {
        perror("ioctl(XFS_IOC_FD_TO_HANDLE)");
        exit(1);
    }
    
    printf("XFS_IOC_FD_TO_HANDLE-\n    path: %s\n    handle: ", path);
    handle_print(handle.ohandle, *handle.ohandlen);
    printf("\n");
    
    close(fd);
    
    handle.path=NULL;
    handle.fd=-1;
    handle.ihandle=buffer;
    handle.ihandlen=len;
    handle.ohandle=NULL;
    handle.ohandlen=NULL;
    ret=ioctl(fsfd, XFS_IOC_OPEN_BY_HANDLE, &handle);
    if (ret<0) {
        perror("ioctl(XFS_IOC_OPEN_BY_HANDLE)");
        exit(1);
    }
    printf("XFS_IOC_OPEN_BY_HANDLE-\n    handle: ");
    handle_print(handle.ihandle, handle.ihandlen);
    printf("\n    fd: %d\n    stat- ", ret);
    stat_print(ret);
    close(ret);
    
    handle.path=path;
    handle.ohandle=buffer;
    handle.ohandlen=&len;
    ret=ioctl(fsfd, XFS_IOC_PATH_TO_HANDLE, &handle);
    if (ret) {
        perror("ioctl(XFS_IOC_PATH_TO_HANDLE)");
        exit(1);
    }
    printf("XFS_IOC_PATH_TO_HANDLE-\n    path: %s\n    handle: ", path);
    handle_print(handle.ohandle, *handle.ohandlen);
    printf("\n");

    handle.path=NULL;
    handle.fd=-1;
    handle.ihandle=buffer;
    handle.ihandlen=len;
    handle.ohandle=link;
    linklen=sizeof(link);
    handle.ohandlen=&linklen;
    ret=ioctl(fsfd, XFS_IOC_READLINK_BY_HANDLE, &handle);
    if (ret<0) {
        perror("ioctl(XFS_IOC_READLINK_BY_HANDLE)");
        fprintf(stderr,"ERROR IGNORED\n");
    } else {
        printf("XFS_IOC_READLINK_BY_HANDLE-\n    handle: ");
        handle_print(handle.ihandle, handle.ihandlen);
        printf("\n    link=\"%*.*s\"\n", 
                ret, ret, (char*)handle.ohandle);
    }
}

int
main(int argc, char **argv)
{
    int                 fsfd;
    
    if (argc != 3) {
        fprintf(stderr,"usage: %s <filesystem> <file/link in FS>\n",
                argv[0]);
        exit(0);
    }

    fsfd = open(argv[1], O_RDONLY);
    if (fsfd < 0) {
	perror("open");
	exit(1);
    }
    
    /* XFS_IOC_FSCOUNTS */
    fscounts(fsfd);
    /* XFS_IOC_GET_RESBLKS & XFS_IOC_SET_RESBLKS */
    getresblks(fsfd);
    setresblks(fsfd, 1000);
    getresblks(fsfd);
    setresblks(fsfd, 0);
    /* XFS_IOC_FSINUMBERS */
    
        /* NYI in kernel */
    
    /* XFS_IOC_PATH_TO_FSHANDLE */
    /* XFS_IOC_PATH_TO_HANDLE */
    /* XFS_IOC_FD_TO_HANDLE */
    /* XFS_IOC_OPEN_BY_HANDLE */
    /* XFS_IOC_READLINK_BY_HANDLE */
    handle(fsfd, argv[2]);
    
    return 0;
}
