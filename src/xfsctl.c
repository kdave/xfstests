/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <xfs/xfs.h>
#include <xfs/jdm.h>

/* simple test program to try out a bunch of xfsctls:
 *     XFS_IOC_FSCOUNTS
 *     XFS_IOC_GET_RESBLKS
 *     XFS_IOC_SET_RESBLKS
 *     XFS_IOC_PATH_TO_FSHANDLE
 *     XFS_IOC_PATH_TO_HANDLE
 *     XFS_IOC_FD_TO_HANDLE
 *     XFS_IOC_OPEN_BY_HANDLE
 *     XFS_IOC_READLINK_BY_HANDLE
*/

    
void fscounts(char *fname, int fsfd)
{
    xfs_fsop_counts_t   counts;
    int                 ret;
    
    ret=xfsctl(fname, fsfd, XFS_IOC_FSCOUNTS, &counts);
    if (ret) {
        perror("xfsctl(XFS_IOC_FSCOUNTS)");
        exit(1);
    }

    printf("XFS_IOC_FSCOUNTS-\n    freedata: %lld freertx: %lld freeino: %lld allocino: %lld\n",
            (long long)counts.freedata, (long long)counts.freertx,
	   (long long)counts.freeino, (long long)counts.allocino);
}
    
__u64 getresblks(char *fname, int fsfd)
{
    xfs_fsop_resblks_t  res;
    int                 ret;
    
    ret=xfsctl(fname, fsfd, XFS_IOC_GET_RESBLKS, &res);
    if (ret) {
        perror("xfsctl(XFS_IOC_GET_RESBLKS)");
        exit(1);
    }
    
    printf("XFS_IOC_GET_RESBLKS-\n    resblks: %lld blksavail: %lld\n",
            (long long)res.resblks, (long long)res.resblks_avail);
    
    return res.resblks;
}
    
__u64 setresblks(char *fname, int fsfd, __u64 blks)
{
    xfs_fsop_resblks_t  res;
    int                 ret;
    
    res.resblks=blks;
    ret=xfsctl(fname, fsfd, XFS_IOC_SET_RESBLKS, &res);
    if (ret) {
        perror("xfsctl(XFS_IOC_SET_RESBLKS)");
        exit(1);
    }
    
    printf("XFS_IOC_SET_RESBLKS-\n    resblks: %lld blksavail: %lld\n",
            (long long)res.resblks, (long long)res.resblks_avail);
    
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
            (unsigned long long)buf.st_dev, (unsigned long long)buf.st_ino, buf.st_mode);
}
    

void handle(char *fname, int fsfd, char *path)
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
    ret=xfsctl(fname, fsfd, XFS_IOC_PATH_TO_FSHANDLE, &handle);
    if (ret) {
        perror("xfsctl(XFS_IOC_PATH_TO_FSHANDLE)");
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
    ret=xfsctl(fname, fsfd, XFS_IOC_FD_TO_HANDLE, &handle);
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
    ret=xfsctl(fname, fsfd, XFS_IOC_OPEN_BY_HANDLE, &handle);
    if (ret<0) {
        perror("xfsctl(XFS_IOC_OPEN_BY_HANDLE)");
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
    ret=xfsctl(fname, fsfd, XFS_IOC_PATH_TO_HANDLE, &handle);
    if (ret) {
        perror("xfsctl(XFS_IOC_PATH_TO_HANDLE)");
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
    ret=xfsctl(fname, fsfd, XFS_IOC_READLINK_BY_HANDLE, &handle);
    if (ret<0) {
        perror("xfsctl(XFS_IOC_READLINK_BY_HANDLE)");
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
    fscounts(argv[0], fsfd);
    /* XFS_IOC_GET_RESBLKS & XFS_IOC_SET_RESBLKS */
    getresblks(argv[0], fsfd);
    setresblks(argv[0], fsfd, 1000);
    getresblks(argv[0], fsfd);
    setresblks(argv[0], fsfd, 0);
    /* TODO - XFS_IOC_FSINUMBERS */

    /* XFS_IOC_PATH_TO_FSHANDLE */
    /* XFS_IOC_PATH_TO_HANDLE */
    /* XFS_IOC_FD_TO_HANDLE */
    /* XFS_IOC_OPEN_BY_HANDLE */
    /* XFS_IOC_READLINK_BY_HANDLE */
    handle(argv[0], fsfd, argv[2]);
    
    return 0;
}
