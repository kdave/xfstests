/*
 * Copyright  2002 Silicon Graphics, Inc.  ALL RIGHTS RESERVED
 * 
 *  U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * 
 *  Use, duplication or disclosure by the Government is subject to restrictions
 *  as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the 
 *  Rights in Technical Data and Computer Software clause at DFARS 252.227-7013 
 *  and/or in similar or successor clauses in the FAR, or the DOD or NASA FAR
 *  Supplement. Unpublished -- rights reserved under the Copyright Laws 
 *  of the United States. Contractor/manufacturer is Silicon Graphics, Inc.,
 *  2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 * 
 *  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
 * 
 *  The copyright notice above does not evidence any actual or intended 
 *  publication or disclosure of this source code, which includes information 
 *  that is the confidential and/or proprietary, and is a trade secret,
 *  of Silicon Graphics, Inc. Any use, duplication or disclosure not 
 *  specifically authorized in writing by Silicon Graphics is strictly 
 *  prohibited. ANY DUPLICATION, MODIFICATION, DISTRIBUTION,PUBLIC PERFORMANCE,
 *  OR PUBLIC DISPLAY OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT 
 *  OF SILICON GRAPHICS, INC. IS STRICTLY PROHIBITED.  THE RECEIPT OR POSSESSION
 *  OF THIS SOURCE CODE AND/OR INFORMATION DOES NOT CONVEY ANY RIGHTS 
 *  TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, 
 *  OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
 */
 
/* dxm - 28/2/2 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <unistd.h>
#include <sys/types.h>

FILE    *fp = NULL;

#define LL                  "ll"

#define PERROR(a,b)         perror(a)
#define GET_LAST_ERROR      errno

#define HANDLE              int
#define INVALID_HANDLE      -1
#define TRUNCATE_ERROR      -1
#define FLUSH_ERROR         EOF

#define FILE_BEGIN          SEEK_SET
#define FILE_CURRENT        SEEK_CUR

#define OPEN(N, F)          open(N, O_CREAT|O_RDWR|F, 0644); fp = fdopen(f, "r+")
#define SEEK(H, O, F)       (lseek(H, O, F))
#define READ(H, B, L)       (read(H, B, L))
#define WRITE(H, B, L)      (write(H, B, L))
#define CLOSE(H)            (close(H))
#define DELETE_FILE(F)      (unlink(F))
#define FLUSH(F)            (fflush(fp))
#define TRUNCATE(F)         (ftruncate(F, 0))

#define ALLOC_ALIGNED(S)    (memalign(65536, S))
#define FREE_ALIGNED(P)     (free(P))

#define DIRECT_IO_FLAG      O_DIRECT

enum {
    FLAG_OPENCLOSE  = 1,
    FLAG_READ       = 2,
    FLAG_WRITE      = 4,
    FLAG_VERBOSE    = 8,
    FLAG_TRUNCATE   = 16,
    FLAG_SEQUENTIAL = 32,
    FLAG_FLUSH      = 64,
    FLAG_DELETE     = 128,
    FLAG_DIRECT     = 256,
};

void
usage(char *argv0)
{
    printf(
	"Usage: %s [switches] <filename>\n"
	"              -i <count>   = repeat count (default forever)\n"
	"              -o           = open/close\n"
	"              -r           = read\n"
	"              -w           = write\n"
	"              -t           = truncate\n"
	"              -d           = delete\n"
        "              -b <size>    = buffer size\n"
        "              -v           = verbose\n"
        "              -s           = sequential\n"
        "              -f           = flush\n"
        "              -D           = direct-IO\n"
	"              -h           = usage\n",
            argv0);
}

extern int optind;
extern char *optarg;

int
main(int argc, char *argv[])
{
    HANDLE              f 		= INVALID_HANDLE;
    char                *filename;
    int                 i;
    int                 c;
    int 		count           = -1;
    int                 bufsize         = 4096;
    int                 flags           = 0;
    char                *buf            = NULL;
    int64_t 		seek_to 	= 0;
        
    while ((c = getopt(argc, argv, "i:orwb:svthfFDd?")) != EOF) {
	switch (c) {
	    case 'i':
		count = atoi(optarg);
		break;
	    case 'o':
		flags |= FLAG_OPENCLOSE;
		break;
	    case 'r':
		flags |= FLAG_READ;
		break;
	    case 'w':
		flags |= FLAG_WRITE;
		break;
	    case 't':
		flags |= FLAG_TRUNCATE;
		break;
	    case 'v':
		flags |= FLAG_VERBOSE;
		break;
            case 'b':
                bufsize = atoi(optarg);
                break;
            case 's':
                flags |= FLAG_SEQUENTIAL;
                break;
            case 'f':
                flags |= FLAG_FLUSH;
                break;
            case 'D':
                flags |= FLAG_DIRECT;
                break;
            case 'd':
                flags |= FLAG_DELETE;
                break;
	    case '?':
	    case 'h':
	    default:
		usage(argv[0]);
		return 1;
        }
    }
    if (optind != argc - 1) {
	usage(argv[0]);
	return 1;
    }

    filename = argv[optind];
    if (!flags) {
        fprintf(stderr, "nothing to do!\n");
        exit(1);
    }

    if (flags & FLAG_DIRECT)
	buf = (char *)ALLOC_ALIGNED(bufsize);
    else 
	buf = (char *)malloc(bufsize);
            
    if (!buf)
        PERROR("malloc", GET_LAST_ERROR);

    for (i = 0; i < bufsize; i++) {
        buf[i] = i & 127;
    }

    for (i = 0; count < 0 || i < count; i++) {
        
        if ((flags & FLAG_OPENCLOSE) || !i) {
            int fileflags;
            
            if (flags & FLAG_VERBOSE)
                printf("open %s\n", filename);
            
            fileflags = 0;
            if (flags & FLAG_DIRECT)
		fileflags |= DIRECT_IO_FLAG;
            
            f = OPEN(filename, fileflags);
            if (f == INVALID_HANDLE)
                PERROR("OPEN", GET_LAST_ERROR);

        }
        
        if ((flags & FLAG_OPENCLOSE) && (flags & FLAG_SEQUENTIAL)) {
            if (flags & FLAG_VERBOSE)
                printf("seek %" LL "d\n", seek_to);
            if (SEEK(f, seek_to, FILE_BEGIN) < 0)
	        PERROR("SEEK", GET_LAST_ERROR);
        }
            
        if (flags & FLAG_WRITE) {
            int sizewritten;
            
            if (!(flags & FLAG_SEQUENTIAL)) {
                if (flags & FLAG_VERBOSE)
                    printf("seek %" LL "d\n", seek_to);
                if (SEEK(f, seek_to, FILE_BEGIN) < 0)
	            PERROR("SEEK", GET_LAST_ERROR);
            }
            
            if (flags & FLAG_VERBOSE)
                printf("write %d\n", bufsize);
            if ((sizewritten = WRITE(f, buf, bufsize)) != bufsize) {
                if (sizewritten < 0)
	            PERROR("WRITE", GET_LAST_ERROR);
                else
                    fprintf(stderr, "short write: %d of %d\n",
                            sizewritten, bufsize);
            }
	}
        
        if (flags & FLAG_READ) {
            int sizeread;
            
            if (!(flags & FLAG_SEQUENTIAL) || (flags & FLAG_WRITE)) {
                if (flags & FLAG_VERBOSE)
                    printf("seek %" LL "d\n", seek_to);
                if (SEEK(f, seek_to, FILE_BEGIN) < 0)
	            PERROR("SEEK", GET_LAST_ERROR);
            }
            
            if (flags & FLAG_VERBOSE)
                printf("read %d\n", bufsize);
            if ((sizeread = READ(f, buf, bufsize)) != bufsize) {
                if (sizeread < 0)
	            PERROR("READ", GET_LAST_ERROR);
                else if (sizeread)
                    fprintf(stderr, "short read: %d of %d\n",
                        sizeread, bufsize);
		else {
                    fprintf(stderr, "Read past EOF\n");
		    exit(0);
		}
            }
        }
        
        if (flags & FLAG_TRUNCATE) {
            if (flags & FLAG_VERBOSE)
                printf("seek 0\n");

	    if (SEEK(f, 0, FILE_BEGIN) < 0)
	        PERROR("SEEK", GET_LAST_ERROR);
            
            if (flags & FLAG_VERBOSE)
                printf("truncate\n");
            
            if (TRUNCATE(f) == TRUNCATE_ERROR)
  	        PERROR("TRUNCATE", GET_LAST_ERROR);
        }
        
        if (flags & FLAG_FLUSH) {
            if (flags & FLAG_VERBOSE)
                printf("flush\n");
            
            if (FLUSH(f) == FLUSH_ERROR)
  	        PERROR("FLUSH", GET_LAST_ERROR);
        }

        if (flags & FLAG_SEQUENTIAL) {
	    seek_to += bufsize;
	    if (flags & FLAG_TRUNCATE) {
                if (flags & FLAG_VERBOSE)
                    printf("seek %" LL "d\n", seek_to);
                if (SEEK(f, seek_to, FILE_BEGIN) < 0)
	            PERROR("SEEK", GET_LAST_ERROR);
	    }
	}
        
        if (flags & FLAG_OPENCLOSE) {
            if (flags & FLAG_VERBOSE)
                printf("close %s\n", filename);
            CLOSE(f);
        }
        
        if (flags & FLAG_DELETE) {
            if (flags & FLAG_VERBOSE)
                printf("delete %s\n", filename);
            DELETE_FILE(filename);
        }
    }

    if (buf) {
        if (flags & FLAG_DIRECT)
	    FREE_ALIGNED(buf);
	else
	    free(buf);
    }
    
    return 0;
}
