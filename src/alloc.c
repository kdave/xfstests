/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 
#include "global.h"

#define	FSBTOBB(f)	(OFFTOBBT(FSBTOOFF(f)))
#define	BBTOFSB(b)	(OFFTOFSB(BBTOOFF(b)))
#define	OFFTOFSB(o)	((o) / blocksize)
#define	FSBTOOFF(f)	((f) * blocksize)

void usage(void)
{
    printf("usage: alloc [-b blocksize] [-d dir] [-f file] [-n] [-r] [-t]\n"
           "flags:\n"
           "    -n - non-interractive mode\n"
           "    -r - real time file\n"
           "    -t - truncate on open\n"
           "\n"  
           "commands:\n"  
           "    r   [offset] [length] - reserve\n"
           "    u   [offset] [length] - unreserve\n"
           "    a   [offset] [length] - alloc      *** identical to free\n"
           "    f   [offset] [length] - free       *** identical to alloc\n" 
           "    m/p [offset] [length] - print map\n"
           "    s                     - sync file\n"
           "    t   [offset]          - truncate\n"
           "    q                     - quit\n"
           "    h/?                   - this help\n");
           
}

int		fd;
int		blocksize = 0;

/* params are in bytes */
void map(off64_t off, off64_t len)
{
    struct getbmap	bm[2]={{0}};
    
    bm[0].bmv_count = 2;
    bm[0].bmv_offset = OFFTOBB(off);
    if (len==(off64_t)-1) { /* unsigned... */
        bm[0].bmv_length = -1;
        printf("    MAP off=%lld, len=%lld [%lld-]\n", 
                (__s64)off, (__s64)len,
                (__s64)BBTOFSB(bm[0].bmv_offset));
    } else {
        bm[0].bmv_length = OFFTOBB(len);
        printf("    MAP off=%lld, len=%lld [%lld,%lld]\n", 
                (__s64)off, (__s64)len,
                (__s64)BBTOFSB(bm[0].bmv_offset),
                (__s64)BBTOFSB(bm[0].bmv_length));
    }
    
    printf("        [ofs,count]: start..end\n");
    for (;;) {
	    if (ioctl(fd, XFS_IOC_GETBMAP, bm) < 0) {
		    perror("getbmap");
		    break;
	    }
	    if (bm[0].bmv_entries == 0)
		    break;
	    printf("        [%lld,%lld]: ",
		    (__s64)BBTOFSB(bm[1].bmv_offset),
		    (__s64)BBTOFSB(bm[1].bmv_length));
	    if (bm[1].bmv_block == -1)
		    printf("hole");
	    else
		    printf("%lld..%lld",
			    (__s64)BBTOFSB(bm[1].bmv_block),
			    (__s64)BBTOFSB(bm[1].bmv_block +
				    bm[1].bmv_length - 1));
	    printf("\n");
    }
}

int
main(int argc, char **argv)
{
	int		c;
	char		*dirname = NULL;
	int		done = 0;
	struct flock64	f;
	char		*filename = NULL;
	off64_t		len;
	char		line[1024];
	off64_t		off;
	int		oflags;
	static char	*opnames[] =
		{ "freesp", "allocsp", "unresvsp", "resvsp" };
	int		opno;
	static int	optab[] =
		{ XFS_IOC_FREESP64, XFS_IOC_ALLOCSP64, XFS_IOC_UNRESVSP64, XFS_IOC_RESVSP64 };
	int		rflag = 0;
	struct statvfs64	svfs;
	int		tflag = 0;
        int             nflag = 0;
	int		unlinkit = 0;
	__int64_t	v;

	while ((c = getopt(argc, argv, "b:d:f:rtn")) != -1) {
		switch (c) {
		case 'b':
			blocksize = atoi(optarg);
			break;
		case 'd':
			if (filename) {
				printf("can't specify both -d and -f\n");
				exit(1);
			}
			dirname = optarg;
			break;
		case 'f':
			if (dirname) {
				printf("can't specify both -d and -f\n");
				exit(1);
			}
			filename = optarg;
			break;
		case 'r':
			rflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'n':
                        nflag++;
                        break;
		default:
			printf("unknown option\n");
                        usage();
			exit(1);
		}
	}
	if (!dirname && !filename)
		dirname = ".";
	if (!filename) {
		static char	tmpfile[] = "allocXXXXXX";

		mkstemp(tmpfile);
		filename = malloc(strlen(tmpfile) + strlen(dirname) + 2);
		sprintf(filename, "%s/%s", dirname, tmpfile);
		unlinkit = 1;
	}
	oflags = O_RDWR | O_CREAT | (tflag ? O_TRUNC : 0);
	fd = open(filename, oflags, 0666);
        if (!nflag) {
            printf("alloc:\n");
	    printf("    filename %s\n", filename);
        }
	if (fd < 0) {
		perror(filename);
		exit(1);
	}
	if (unlinkit)
		unlink(filename);
	if (!blocksize) {
	        if (fstatvfs64(fd, &svfs) < 0) {
		        perror(filename);
		        exit(1);
	        }
		blocksize = (int)svfs.f_bsize;
        }
        if (blocksize<0) {
                fprintf(stderr,"illegal blocksize %d\n", blocksize);
                exit(1);
        }
	printf("    blocksize %d\n", blocksize);
	if (rflag) {
		struct fsxattr a;

		if (ioctl(fd, XFS_IOC_FSGETXATTR, &a) < 0) {
			perror("XFS_IOC_FSGETXATTR");
			exit(1);
		}
		a.fsx_xflags |= XFS_XFLAG_REALTIME;
		if (ioctl(fd, XFS_IOC_FSSETXATTR, &a) < 0) {
			perror("XFS_IOC_FSSETXATTR");
			exit(1);
		}
	}
	while (!done) {
                char *p;
                
                if (!nflag) printf("alloc> ");
                fflush(stdout);
                if (!fgets(line, 1024, stdin)) break;
                
                p=line+strlen(line);
                if (p!=line&&p[-1]=='\n') p[-1]=0;
                
		opno = 0;
		switch (line[0]) {
		case 'r':
			opno++;
		case 'u':
			opno++;
		case 'a':
			opno++;
		case 'f':
			v = strtoll(&line[2], &p, 0);
			if (*p == 'b') {
				off = FSBTOOFF(v);
				p++;
			} else
				off = v;
			f.l_whence = SEEK_SET;
			f.l_start = off;
			if (*p == '\0')
				v = -1;
			else
				v = strtoll(p, &p, 0);
			if (*p == 'b') {
				len = FSBTOOFF(v);
				p++;
			} else
				len = v;
                        
                        printf("    CMD %s, off=%lld, len=%lld\n", 
                                opnames[opno], (__s64)off, (__s64)len);
                        
			f.l_len = len;
			c = ioctl(fd, optab[opno], &f);
			if (c < 0) {
				perror(opnames[opno]);
				break;
			}
                        
                        map(off,len);                        
			break;
		case 'p':
		case 'm':
			p = &line[1];
			v = strtoll(p, &p, 0);
			if (*p == 'b') {
				off = FSBTOOFF(v);
				p++;
			} else
				off = v;
			if (*p == '\0')
				len = -1;
			else {
				v = strtoll(p, &p, 0);
				if (*p == 'b')
					len = FSBTOOFF(v);
				else
					len = v;
			}
                        map(off,len);
			break;
		case 't':
			p = &line[1];
			v = strtoll(p, &p, 0);
			if (*p == 'b')
				off = FSBTOOFF(v);
			else
				off = v;
                        printf("    TRUNCATE off=%lld\n", (__s64)off);
			if (ftruncate64(fd, off) < 0) {
				perror("ftruncate");
				break;
			}
			break;
                case 's':
                        printf("    SYNC\n");
                        fsync(fd);
                        break;
		case 'q':
                        printf("    QUIT\n");
			done = 1;
			break;
                case '?':
                case 'h':
                        usage();
                        break;
		default:
			printf("unknown command '%s'\n", line);
			break;
		}
	}
        if (!nflag) printf("\n");
	close(fd);
	exit(0);
	/* NOTREACHED */
}
