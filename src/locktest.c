// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

/*
 * Synchronized byte range lock exerciser
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <endian.h>
#include <byteswap.h>
#include <errno.h>
#include <string.h>
#define     HEX_2_ASC(x)    ((x) > 9) ? (x)-10+'a' : (x)+'0'
#define 	FILE_SIZE	1024
#define PLATFORM_INIT()     /*no-op*/
#define PLATFORM_CLEANUP()  /*no-op*/
#define LL                  "ll"

extern int h_errno;

#define inet_aton(STRING, INADDRP) \
    (((INADDRP)->s_addr = inet_addr(STRING)) == -1 ? 0 : 1)

/* this assumes 32 bit pointers */
#define PTR_TO_U64(P) ((unsigned __int64)(unsigned int)(P))
#define U64_TO_PTR(T,U) ((T)(void *)(unsigned int)(U))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define bswap_uint16(x)         (uint16_t)bswap_16(x)
#define bswap_uint32(x)         (uint32_t)bswap_32(x)
#define bswap_uint64(x)         (uint64_t)bswap_64(x)
#else
#define bswap_uint16(x)         x
#define bswap_uint32(x)         x
#define bswap_uint64(x)         x
#endif

#define SOCKET              int
#define SOCKET_READ         read
#define SOCKET_WRITE        write
#define SOCKET_CLOSE(S)     (close(S))
#define INVALID_SOCKET      -1

#define O_BINARY            0
       
#define HANDLE              int
#define INVALID_HANDLE      -1
#define SEEK(H, O)          (lseek(H, O, SEEK_SET))
#define READ(H, B, L)       (read(H, B, L))
#define WRITE(H, B, L)      (write(H, B, L))
#define CLOSE(H)            (close(H))

#define RAND()              (rand())
#define SRAND(s)            (srand(s))

#define MIN(A,B)            (((A)<(B))?(A):(B))
#define MAX(A,B)            (((A)>(B))?(A):(B))

#define ALLOC_ALIGNED(S)    (memalign(65536, S)) 
#define FREE_ALIGNED(P)     (free(P)) 

static char	*prog;
static char	*filename = 0;
static int	debug = 0;
static int	server = 1;
static int	port = 0;
static int 	testnumber = -1;
static int	saved_errno = 0;

static SOCKET	s_fd = -1;              /* listen socket    */
static SOCKET	c_fd = -1;	        /* IPC socket       */
static HANDLE	f_fd = INVALID_HANDLE;	/* shared file      */

#define 	CMD_WRLOCK	0
#define 	CMD_RDLOCK	1
#define		CMD_UNLOCK	2
#define		CMD_CLOSE	3
#define		CMD_OPEN	4
#define		CMD_WRTEST	5
#define		CMD_RDTEST	6

#define		PASS 	1
#define		FAIL	0

#define		SERVER	0
#define		CLIENT	1

#define		TEST_NUM	0
#define		COMMAND		1
#define		OFFSET		2
#define		LENGTH		3
#define		RESULT		4
#define		WHO		5
#define		FLAGS		2 /* index 2 is also used for do_open() flag, see below */

static char *get_cmd_str(int cmd)
{
	switch (cmd) {
		case CMD_WRLOCK: return "write lock"; break;
		case CMD_RDLOCK: return "read lock"; break;
		case CMD_UNLOCK: return "unlock"; break;
		case CMD_CLOSE:  return "close"; break;
		case CMD_OPEN:   return "open"; break;
		case CMD_WRTEST: return "Wait for SIGIO"; break;
		case CMD_RDTEST: return "Truncate"; break;
	}
	return "unknown";
}
/* 
 * flags for Mac OS X do_open() 
 * O_RDONLY	0x0000
 * O_WRONLY	0x0001	
 * O_RDWR	0x0002
 * O_NONBLOCK	0x0004	
 * O_APPEND	0x0008
 * O_SHLOCK	0x0010	
 * O_EXLOCK	0x0020
 * O_ASYNC	0x0040	
 * O_FSYNC	0x0080
 * O_NOFOLLOW  	0x0100  
 * O_CREAT	0x0200
 * O_TRUNC	0x0400
 * O_EXCL	0x0800
 */

/*
 * When adding tests be sure to add to both the descriptions AND tests array. 
 * Also, be sure to undo whatever is set for each test (eg unlock any locks)
 * There is no need to have a matching client command for each server command
 * (or vice versa)
 */

char *descriptions[] = {
    /* 1 */"Add a lock to an empty lock list",
    /* 2 */"Add a lock to the start and end of a list - no overlaps",
    /* 3 */"Add a lock to the middle of a list - no overlap",
    /* 4 */"Add different lock types to middle of the list - overlap exact match", 
    /* 5 */"Add new lock which completely overlaps any old lock in the list", 
    /* 6 */"Add new lock which itself is completely overlaped by any old lock in the list",
    /* 7 */"Add new lock which starts before any old lock in the list",
    /* 8 */"Add new lock which starts in the middle of any old lock in the list and ends after",
    /* 9 */"Add different new lock types which completely overlaps any old lock in the list",
    /* 10 */"Add different new locks which are completely overlaped by an old lock in the list",
    /* 11 */"Add different new lock types which start before the old lock in the list",
    /* 12 */"Add different new lock types which start in the middle of an old lock in the list and end after",
    /* 13 */"Add new lock, differing types and processes, to middle of the list - exact overlap match",
    /* 14 */"Add new lock, differing types and processes, which completely overlap any of the locks in the list",
    /* 15 */"Add new lock, differing types and processes, which are completely overlaped by locks in the list",
    /* 16 */"Add new lock, differing types and processes, which start before a lock in the list",
    /* 17 */"Add new lock, differing types and processes, which starts in the middle of a lock, and ends after",
    /* 18 */"Acquire write locks with overlapping ranges",
    /* 19 */"Acquire write locks with non-overlapping ranges extending beyond EOF",
    /* 20 */"Acquire write locks with overlapping ranges extending beyond EOF",
    /* 21 */"Acquire write locks on whole files",
    /* 22 */"Acquire write lock on whole file and range write lock",
    /* 23 */"Acquire read locks with non-overlapping ranges",
    /* 24 */"Acquire read locks with overlapping ranges",
    /* 25 */"Acquire read and write locks with no overlapping ranges",
    /* 26 */"Acquire read and write locks with overlapping ranges",
    /* 27 */"Acquire whole file write lock and then close without unlocking (and attempt a lock)",
    /* 28 */"Acquire two read locks, close and reopen the file, and test if the inital lock is still there",
    /* 29 */"Verify that F_GETLK for F_WRLCK doesn't require that file be opened for write",
    #if defined(macosx)
    /* 30 */"Close the opened file and open the file with SHLOCK, other client will try to open with SHLOCK too",
    /* 31 */"Close the opened file and open the file with SHLOCK, other client will try to open with EXLOCK",
    /* 32 */"Close the opened file and open the file with EXLOCK, other client will try to open with EXLOCK too"
    #endif
};

static int64_t tests[][6] =
	/*	test #	Action	[offset|flags]	length		expected	server/client */
	{ 	
	/* Various simple tests exercising the list */

/* SECTION 1: WRITE and CMD_UNLOCK with the same process (SERVER) */
	/* Add a lock to an empty list */
		{1,	CMD_WRLOCK,	1,		10,		PASS,		SERVER	},
		{1,	CMD_UNLOCK,	1,		10,		PASS,		SERVER	},
		
	/* Add a lock to the start and end of a list - 1, 13 - no overlap */
		{2,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{2,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{2,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		{2,	CMD_WRLOCK,	1,		5,		PASS,		SERVER	},
		{2,	CMD_WRLOCK,	70,		5,		PASS,		SERVER	},
		
		{2,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{2,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		{2,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		{2,	CMD_UNLOCK,	1,		5,		PASS,		SERVER	},
		{2,	CMD_UNLOCK,	70,		5,		PASS,		SERVER	},
		
	/* Add a lock to the middle of a list - no overlap */
		{3,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{3,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{3,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		{3,	CMD_WRLOCK,	42,		5,		PASS,		SERVER	},
		
		{3,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{3,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		{3,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		{3,	CMD_UNLOCK,	42,		5,		PASS,		SERVER	},
		
	/* Add different lock types to middle of the list - overlap exact match - 3 */
		{4,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{4,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{4,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Exact match - same lock type */
		{4,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		/* Exact match - different lock type */
		{4,	CMD_RDLOCK,	30,		10,		PASS,		SERVER	},
		/* Exact match - unlock */
		{4,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		/* New lock - as above, inserting in the list again */
		{4,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		
		{4,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{4,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		{4,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		
	/* Add new lock which completely overlaps any old lock in the list - 4,5,6 */
		{5,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{5,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{5,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* The start is the same, end overlaps */
		{5,	CMD_WRLOCK,	30,		15,		PASS,		SERVER	},
		/* The start is before, end is the same */
		{5,	CMD_WRLOCK,	25,		20,		PASS,		SERVER	},
		/* Both start and end overlap */
		{5,	CMD_WRLOCK,	22,		26,		PASS,		SERVER	},
		
		{5,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{5,	CMD_UNLOCK,	22,		26,		PASS,		SERVER	},
		{5,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		
	/* Add new lock which itself is completely overlaped by any old lock in the list - 7,8,10 */
		{6,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{6,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{6,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* The start is the same, end is in the middle of old lock - NOP */
		{6,	CMD_WRLOCK,	30,		5,		PASS,		SERVER	},
		/* The start and end are in the middle of old lock - NOP */
		{6,	CMD_WRLOCK,	32,		6,		PASS,		SERVER	},
		/* Start in the middle and end is the same - NOP */
		{6,	CMD_WRLOCK,	32,		8,		PASS,		SERVER	},
		
		{6,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{6,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		{6,	CMD_UNLOCK,	32,		8,		PASS,		SERVER	},
		{6,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		
	/* Add new lock which starts before any old lock in the list - 2,9 */
		{7,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{7,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{7,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Here is the new lock */
		{7,	CMD_WRLOCK,	27,		10,		PASS,		SERVER	},
		/* Go again with the end of the new lock matching the start of old lock */
		{7,	CMD_WRLOCK,	25,		2,		PASS,		SERVER	},
		
		{7,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{7,	CMD_UNLOCK,	25,		15,		PASS,		SERVER	},
		{7,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
	
	/* Add new lock which starts in the middle of any old lock in the list and ends after - 11,12 */
		{8,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{8,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{8,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Here is the new lock */
		{8,	CMD_WRLOCK,	35,		10,		PASS,		SERVER	},
		/* Go again with the end of the new lock matching the start of old lock */
		{8,	CMD_WRLOCK,	45,		2,		PASS,		SERVER	},
		
		{8,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{8,	CMD_UNLOCK,	30,		17,		PASS,		SERVER	},
		{8,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
/* SECTION 2: Overlapping READ and WRITE and CMD_UNLOCK with the same process (SERVER) */
	/* Add different new lock types which completely overlaps any old lock in the list - 4,5,6 */
		{9,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{9,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{9,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* The start is the same, end overlaps */
		{9,	CMD_RDLOCK,	30,		15,		PASS,		SERVER	},
		/* The start is before, end is the same */
		{9,	CMD_WRLOCK,	25,		20,		PASS,		SERVER	},
		/* Both start and end overlap */
		{9,	CMD_RDLOCK,	22,		26,		PASS,		SERVER	},
		
		{9,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{9,	CMD_UNLOCK,	22,		26,		PASS,		SERVER	},
		{9,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		
	/* Add different new locks which are completely overlaped by an old lock in the list - 7,8,10 */
		{10,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{10,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{10,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* The start is the same, end is in the middle of old lock */
		{10,	CMD_RDLOCK,	30,		5,		PASS,		SERVER	},
		/* The start and end are in the middle of a lock */
		{10,	CMD_WRLOCK,	32,		2,		PASS,		SERVER	},
		/* Start in the middle and end is the same */
		{10,	CMD_RDLOCK,	36,		5,		PASS,		SERVER	},
		
		{10,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{10,	CMD_UNLOCK,	30,		11,		PASS,		SERVER	},
		{10,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		
	/* Add different new lock types which start before the old lock in the list - 2,9 */
		{11,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{11,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{11,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Here is the new lock */
		{11,	CMD_RDLOCK,	27,		10,		PASS,		SERVER	},
		/* Go again with the end of the new lock matching the start of lock */
		{11,	CMD_WRLOCK,	25,		3,		PASS,		SERVER	},
		
		{11,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{11,	CMD_UNLOCK,	25,		15,		PASS,		SERVER	},
		{11,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
	
	/* Add different new lock types which start in the middle of an old lock in the list and end after - 11,12 */
		{12,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{12,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{12,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Here is the new lock */
		{12,	CMD_RDLOCK,	35,		10,		PASS,		SERVER	},
		/* Go again with the end of the new lock matching the start of old lock */
		{12,	CMD_WRLOCK,	44,		3,		PASS,		SERVER	},
		
		{12,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{12,	CMD_UNLOCK,	30,		18,		PASS,		SERVER	},
		{12,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},

/* SECTION 3: Overlapping READ and WRITE and CMD_UNLOCK with the different processes (CLIENT/SERVER) */
	/* Add new lock, differing types and processes, to middle of the list - exact overlap match - 3 */
		{13,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{13,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{13,	CMD_RDLOCK,	50,		10,		PASS,		SERVER	},
		/* Same lock type, different process */
		{13,	CMD_WRLOCK,	30,		10,		FAIL,		CLIENT	},
		{13,	CMD_RDLOCK,	50,		10,		PASS,		CLIENT	},
		/* Exact match - different lock type, different process */
		{13,	CMD_RDLOCK,	30,		10,		FAIL,		CLIENT	},
		/* Exact match - unlock */
		{13,	CMD_UNLOCK,	30,		10,		PASS,		CLIENT	},
		/* New lock - as above, inserting in the list again */
		{13,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		/* Exact match - same lock type, different process */
		{13,	CMD_WRLOCK,	30,		10,		PASS,		CLIENT	},
		
		{13,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{13,	CMD_UNLOCK,	30,		10,		PASS,		CLIENT	},
		{13,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
		
	/* Add new lock, differing types and processes, which completely overlap any of the locks in the list - 4,5,6 */
		{14,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{14,	CMD_WRLOCK,	30,		10,		PASS,		SERVER	},
		{14,	CMD_RDLOCK,	50,		10,		PASS,		SERVER	},
		/* The start is the same, end overlaps */
		{14,	CMD_RDLOCK,	30,		15,		FAIL,		CLIENT	},
		{14,	CMD_WRLOCK,	30,		15,		FAIL,		CLIENT	},
		/* The start is before, end is the same */
		{14,	CMD_RDLOCK,	25,		20,		FAIL,		CLIENT	},
		{14,	CMD_WRLOCK,	25,		20,		FAIL,		CLIENT	},
		/* Both start and end overlap */
		{14,	CMD_RDLOCK,	22,		26,		FAIL,		CLIENT	},
		{14,	CMD_WRLOCK,	22,		26,		FAIL,		CLIENT	},
		
		/* The start is the same, end overlaps */
		{14,	CMD_RDLOCK,	50,		15,		PASS,		CLIENT	},
		{14,	CMD_WRLOCK,	50,		17,		FAIL,		CLIENT	},
		/* The start is before, end is the same */
		{14,	CMD_RDLOCK,	45,		20,		PASS,		CLIENT	},
		{14,	CMD_WRLOCK,	43,		22,		FAIL,		CLIENT	},
		/* Both start and end overlap */
		{14,	CMD_RDLOCK,	42,		26,		PASS,		CLIENT	},
		{14,	CMD_WRLOCK,	41,		28,		FAIL,		CLIENT	},
		
		{14,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{14,	CMD_UNLOCK,	22,		26,		PASS,		SERVER	},
		{14,	CMD_UNLOCK,	42,		26,		PASS,		CLIENT	},

	/* Add new lock, differing types and processes, which are completely overlaped by an old lock in the list - 7,8,10 */
		{15,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{15,	CMD_RDLOCK,	30,		10,		PASS,		SERVER	},
		{15,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* The start is the same, end is in the middle of old lock */
		{15,	CMD_RDLOCK,	50,		5,		FAIL,		CLIENT	},
		{15,	CMD_WRLOCK,	50,		5,		FAIL,		CLIENT	},
		/* The start and end are in the middle of old lock */
		{15,	CMD_RDLOCK,	52,		6,		FAIL,		CLIENT	},
		{15,	CMD_WRLOCK,	52,		6,		FAIL,		CLIENT	},
		/* Start in the middle and end is the same */
		{15,	CMD_RDLOCK,	52,		8,		FAIL,		CLIENT	},
		{15,	CMD_WRLOCK,	52,		8,		FAIL,		CLIENT	},
		/* The start is the same, end is in the middle of old lock */
		{15,	CMD_RDLOCK,	30,		5,		PASS,		CLIENT	},
		{15,	CMD_WRLOCK,	30,		5,		FAIL,		CLIENT	},
		/* The start and end are in the middle of old lock */
		{15,	CMD_RDLOCK,	32,		6,		PASS,		CLIENT	},
		{15,	CMD_WRLOCK,	32,		6,		FAIL,		CLIENT	},
		/* Start in the middle and end is the same */
		{15,	CMD_RDLOCK,	32,		8,		PASS,		CLIENT	},
		{15,	CMD_WRLOCK,	32,		8,		FAIL,		CLIENT	},
		
		{15,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{15,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		{15,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},
	/* Add new lock, differing types and processes, which start before a lock in the list - 2,9 */
		{16,	CMD_RDLOCK,	10,		10,		PASS,		SERVER	},
		{16,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Start is before, end is the start of the old lock in list */
		{16,	CMD_RDLOCK,	5,		6,		PASS,		CLIENT	},
		{16,	CMD_WRLOCK,	5,		6,		FAIL,		CLIENT	},
		/* Start is before, end is in the middle of the old lock */
		{16,	CMD_RDLOCK,	5,		10,		PASS,		CLIENT	},
		{16,	CMD_WRLOCK,	5,		10,		FAIL,		CLIENT	},
		/* Start is before, end is the start of the old lock in list */
		{16,	CMD_RDLOCK,	45,		6,		FAIL,		CLIENT	},
		{16,	CMD_WRLOCK,	45,		6,		FAIL,		CLIENT	},
		/* Start is before, end is in the middle of the old lock */
		{16,	CMD_RDLOCK,	45,		10,		FAIL,		CLIENT	},
		{16,	CMD_WRLOCK,	45,		10,		FAIL,		CLIENT	},
		
		{16,	CMD_UNLOCK,	5,		15,		PASS,		CLIENT	},
		{16,	CMD_UNLOCK,	30,		10,		PASS,		SERVER	},
		{16,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},

	/* Add new lock, differing types and processes, which starts in the middle of a lock, and ends after - 11,12 */
		{17,	CMD_WRLOCK,	10,		10,		PASS,		SERVER	},
		{17,	CMD_RDLOCK,	30,		10,		PASS,		SERVER	},
		{17,	CMD_WRLOCK,	50,		10,		PASS,		SERVER	},
		/* Start in the middle, end after lock in list */
		{17,	CMD_WRLOCK,	35,		10,		FAIL,		CLIENT	},
		/* Start matches end of lock in list  */
		{17,	CMD_RDLOCK,	35,		10,		PASS,		CLIENT	},
		{17,	CMD_RDLOCK,	44,		2,		PASS,		CLIENT	},
		/* Start in the middle, end after lock in list */
		{17,	CMD_RDLOCK,	55,		10,		FAIL,		CLIENT	},
		{17,	CMD_WRLOCK,	55,		10,		FAIL,		CLIENT	},
		/* Start matches end of lock in list  */
		{17,	CMD_RDLOCK,	59,		5,		FAIL,		CLIENT	},
		{17,	CMD_WRLOCK,	59,		5,		FAIL,		CLIENT	},
		
		{17,	CMD_UNLOCK,	10,		10,		PASS,		SERVER	},
		{17,	CMD_UNLOCK,	30,		16,		PASS,		CLIENT	},
		{17,	CMD_UNLOCK,	50,		10,		PASS,		SERVER	},

/* SECTION 4: overlapping and EOF tests */
	/* Acquire overlapping ranges */
		{18,	CMD_WRLOCK,	11,		7,		PASS,		SERVER	},
		{18,	CMD_WRLOCK,	13,		8,		FAIL,		CLIENT	},
		{18,	CMD_UNLOCK,	11,		7,		PASS,		SERVER	},
	/* Acquire different ranges beyond EOF */
		{19,	CMD_WRLOCK,	10,		FILE_SIZE,	PASS,		SERVER	},
		{19,	CMD_WRLOCK,	FILE_SIZE + 10,	10,		PASS,		CLIENT	},
		{19,	CMD_UNLOCK,	10,		FILE_SIZE,	PASS,		SERVER	},
		{19,	CMD_UNLOCK,	FILE_SIZE + 10,	10,		PASS,		CLIENT	},
	/* Acquire same range beyong EOF */
		{20,	CMD_WRLOCK,	10,		FILE_SIZE,	PASS,		SERVER,	},
		{20,	CMD_WRLOCK,	10,		FILE_SIZE,	FAIL,		CLIENT,	},
		{20,	CMD_UNLOCK,	10,		FILE_SIZE,	PASS,		SERVER,	},
	/* Acquire whole file lock */
		{21,	CMD_WRLOCK,	0,		0,		PASS,		SERVER,	},
		{21,	CMD_WRLOCK,	0,		0,		FAIL,		CLIENT,	},
		{21,	CMD_UNLOCK,	0,		0,		PASS,		SERVER,	},
	/* Acquire whole file lock, then range */
		{22,	CMD_WRLOCK,	0,		0,		PASS,		SERVER,	},
		{22,	CMD_WRLOCK,	1,		5,		FAIL,		CLIENT,	},
		{22,	CMD_UNLOCK,	0,		0,		PASS,		SERVER,	},
	/* Acquire non-overlapping read locks */
		{23,	CMD_RDLOCK, 1,		5,		PASS,		SERVER, },
		{23,	CMD_RDLOCK, 7,		6,		PASS,		CLIENT, },
		{23,	CMD_UNLOCK, 1, 		5,		PASS,		SERVER, },
		{23,	CMD_UNLOCK, 7,		6,		PASS,		CLIENT, },
	/* Acquire overlapping read locks */
		{24,	CMD_RDLOCK, 1,		5,		PASS,		SERVER, },
		{24,	CMD_RDLOCK, 2,		6,		PASS,		CLIENT, },
		{24,	CMD_UNLOCK, 1, 		5,		PASS,		SERVER, },
		{24,	CMD_UNLOCK, 1,		7,		PASS,		CLIENT, },
	/* Acquire non-overlapping read and write locks */
		{25,	CMD_RDLOCK, 1,		5,		PASS,		SERVER, },
		{25,	CMD_WRLOCK, 7,		6,		PASS,		CLIENT, },
		{25,	CMD_UNLOCK, 1, 		5,		PASS,		SERVER, },
		{25,	CMD_UNLOCK, 7,		6,		PASS,		CLIENT, },
	/* Acquire overlapping read and write locks */
		{26,	CMD_RDLOCK, 1,		5,		PASS,		SERVER, },
		{26,	CMD_WRLOCK, 2,		6,		FAIL,		CLIENT, },
		{26,	CMD_UNLOCK, 1, 		5,		PASS,		SERVER, },
	/* Acquire whole file lock, then close (without unlocking) */
		{27,	CMD_WRLOCK,	0,		0,		PASS,		SERVER,	},
		{27,	CMD_WRLOCK,	1,		5,		FAIL,		CLIENT,	},
		{27,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{27,	CMD_WRLOCK,	1,		5,		PASS,		CLIENT,	},
		{27,	CMD_OPEN,	O_RDWR,		0,		PASS,		SERVER,	},
		{27,	CMD_UNLOCK,	1,		5,		PASS,		CLIENT,	},
	/* Acquire two read locks, close one file and then reopen to check that first lock still exists */
		{28,	CMD_RDLOCK,	1,		5,		PASS,		SERVER,	},
		{28,	CMD_RDLOCK,	1,		5,		PASS,		CLIENT,	},
		{28,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{28,	CMD_OPEN,	O_RDWR,		0,		PASS,		SERVER,	},
		{28,	CMD_WRLOCK,	0,		0,		FAIL,		SERVER,	},
		{28,	CMD_UNLOCK,	1,		5,		PASS,		SERVER,	},
	/* Verify that F_GETLK for F_WRLCK doesn't require that file be opened for write */
		{29,	CMD_CLOSE, 0,		0,		PASS,		SERVER, },
		{29,	CMD_OPEN, O_RDONLY,	0,		PASS,		SERVER, },
		{29,	CMD_WRTEST, 0,		0,		PASS,		SERVER, },
		{29,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{29,	CMD_OPEN,	O_RDWR,		0,		PASS,		SERVER,	},
#ifdef macosx
	/* Close the opened file and open the file with SHLOCK, other client will try to open with SHLOCK too */
		{30,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{30,	CMD_OPEN,	O_RDWR|O_SHLOCK|O_NONBLOCK,	0,	PASS,		SERVER,	},
		{30,	CMD_CLOSE,0,		0,		PASS,		CLIENT,	},
		{30,	CMD_OPEN,	O_RDWR|O_SHLOCK|O_NONBLOCK,	0,	PASS,		CLIENT,	},
	/* Close the opened file and open the file with SHLOCK, other client will try to open with EXLOCK */
		{31,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{31,	CMD_CLOSE,0,		0,		PASS,		CLIENT,	},
		{31,	CMD_OPEN,	O_RDWR|O_SHLOCK|O_NONBLOCK,	0,	PASS,		SERVER,	},
		{31,	CMD_OPEN,	O_RDWR|O_EXLOCK|O_NONBLOCK,	0,	FAIL,		CLIENT,	},
	/* Close the opened file and open the file with EXLOCK, other client will try to open with EXLOCK too */
		{32,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{32,	CMD_CLOSE,0,		0,		FAIL,		CLIENT,	},
		{32,	CMD_OPEN,	O_RDWR|O_EXLOCK|O_NONBLOCK,	0,	PASS,		SERVER,	},
		{32,	CMD_OPEN,	O_RDWR|O_EXLOCK|O_NONBLOCK,	0,	FAIL,		CLIENT,	},
		{32,	CMD_CLOSE,0,		0,		PASS,		SERVER,	},
		{32,	CMD_CLOSE,0,		0,		FAIL,		CLIENT,	},
		{32,	CMD_OPEN,	O_RDWR,		0,		PASS,		SERVER,	},
		{32,	CMD_OPEN,	O_RDWR,		0,		PASS,		CLIENT,	},
#endif /* macosx */
	/* indicate end of array */
		{0,0,0,0,0,SERVER},
		{0,0,0,0,0,CLIENT}
	};

static struct {
    int32_t		test;
    int32_t		command;
    int64_t	        offset;
    int64_t		length;
    int32_t		result;
    int32_t		index;
    int32_t		error;
    int32_t		padding; /* So mac and irix have the same size struct (bloody alignment) */
} ctl;


void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] sharedfile\n\
\n\
options:\n\
  -p port	TCP/IP port number for client-server communication\n\
  -d		enable debug tracing\n\
  -n #		test number to run\n\
  -h host	run as client and connect to server on remote host\n\
  		[default run as server]\n", prog);
    exit(1);
}

#define INIT_BUFSZ 512 

void
initialize(HANDLE fd)
{
    char*	ibuf;
    int		j=0;
    int		nwrite;
    int 	offset = 0;
    int 	togo = FILE_SIZE;

    ibuf = (char*)malloc(INIT_BUFSZ);
    memset(ibuf, ':', INIT_BUFSZ);

    SEEK(fd, 0L);
    while (togo) {
	offset+=j;
	j = togo > INIT_BUFSZ ? INIT_BUFSZ : togo;

	if ((nwrite = WRITE(fd, ibuf, j)) != j) {
	    if (nwrite < 0)
		perror("initialize write:");
	    else
		fprintf(stderr, "initialize: write() returns %d, not %d as expected\n", 
                        nwrite, j);
	    exit(1);
	    /*NOTREACHED*/
	}
	togo -= j;
    }
}


int do_open(int flag)
{
    int flags = flag|O_CREAT|O_BINARY;

    if(debug > 1)
	fprintf(stderr, "do_open %s 0x%x\n", filename, flags);

    if ((f_fd = open(filename, flags, 0666)) == INVALID_HANDLE) {
	perror("shared file create");
	return FAIL;
	/*NOTREACHED*/
    }
    return PASS;
}

static int do_lock(int cmd, int type, int start, int length)
{
    int ret;
    int filedes = f_fd;
    struct flock fl;

    if(debug > 1) {
	fprintf(stderr, "do_lock: cmd=%d type=%d start=%d, length=%d\n", cmd, type, start, length);
    }

    if (f_fd < 0)
	return f_fd;
    
    fl.l_start = start;
    fl.l_len = length;
    fl.l_whence = SEEK_SET;
    fl.l_pid = getpid();
    fl.l_type = type;

    errno = 0;

    ret = fcntl(filedes, cmd, &fl);
    saved_errno = errno;	    

    if(debug > 1 && ret)
	fprintf(stderr, "do_lock: ret = %d, errno = %d (%s)\n", ret, errno, strerror(errno));

    return(ret==0?PASS:FAIL);
}

int do_close(void)
{	
    if(debug > 1) {
	fprintf(stderr, "do_close\n");
    }

    errno =0;
    CLOSE(f_fd);
    f_fd = INVALID_HANDLE;

    saved_errno = errno;	    
	
    if (errno)
	return FAIL;
    return PASS;
}

static void init_ctl(int32_t index)
{
    ctl.test= (int32_t)tests[index][TEST_NUM];
    ctl.command = (int32_t)tests[index][COMMAND];
    ctl.offset = tests[index][OFFSET];
    ctl.length = tests[index][LENGTH];
    ctl.index = index;
    ctl.result = (int32_t)tests[index][RESULT];
    ctl.error = 0;
}

void
send_ctl(void)
{
    int         nwrite;

    if (debug > 1) {
	fprintf(stderr, "send_ctl: test=%d, command=%d offset=%"LL"d, length=%"LL"d, result=%d, error=%d\n", 
                ctl.test, ctl.command, (long long)ctl.offset, (long long)ctl.length,ctl.result, ctl.error);
    }

    ctl.test= bswap_uint32(ctl.test);
    ctl.command = bswap_uint32(ctl.command);
    ctl.offset = bswap_uint64(ctl.offset);
    ctl.length = bswap_uint64(ctl.length);
    ctl.result = bswap_uint32(ctl.result);
    ctl.index= bswap_uint32(ctl.index);
    ctl.error = bswap_uint32(ctl.error);
    nwrite = SOCKET_WRITE(c_fd, (char*)&ctl, sizeof(ctl));

    ctl.test= bswap_uint32(ctl.test);
    ctl.command = bswap_uint32(ctl.command);
    ctl.offset = bswap_uint64(ctl.offset);
    ctl.length = bswap_uint64(ctl.length);
    ctl.result = bswap_uint32(ctl.result);
    ctl.index= bswap_uint32(ctl.index);
    ctl.error= bswap_uint32(ctl.error);
    if (nwrite != sizeof(ctl)) {
        if (nwrite < 0)
            perror("send_ctl: write");
        else
            fprintf(stderr, "send_ctl[%d]: write() returns %d, not %zu as expected\n", 
                    ctl.test, nwrite, sizeof(ctl));
        exit(1);
        /*NOTREACHED*/
    }
}

void recv_ctl(void)
{
    int         nread;

    if ((nread = SOCKET_READ(c_fd, (char*)&ctl, sizeof(ctl))) != sizeof(ctl)) {
        if (nread < 0)
            perror("recv_ctl: read");
        else {
            fprintf(stderr, "recv_ctl[%d]: read() returns %d, not %zu as expected\n", 
                    ctl.test, nread, sizeof(ctl));
	    fprintf(stderr, "socket might has been closed by other locktest\n");
	} 
        exit(1);
        /*NOTREACHED*/
    }
    ctl.test= bswap_uint32(ctl.test);
    ctl.command = bswap_uint32(ctl.command);
    ctl.offset = bswap_uint64(ctl.offset);
    ctl.length = bswap_uint64(ctl.length);
    ctl.result = bswap_uint32(ctl.result);
    ctl.index= bswap_uint32(ctl.index);
    ctl.error= bswap_uint32(ctl.error);

    if (debug > 1) {
	fprintf(stderr, "recv_ctl: test=%d, command=%d offset=%"LL"d, length=%"LL"d, result=%d, error=%d\n", 
                ctl.test, ctl.command, (long long)ctl.offset, (long long)ctl.length, ctl.result, ctl.error);
    }
}

void
cleanup(void)
{
    if (f_fd>=0)
        CLOSE(f_fd);
    
    if (c_fd>=0)
        SOCKET_CLOSE(c_fd);
    
    if (s_fd>=0)
        SOCKET_CLOSE(s_fd);
    
    PLATFORM_CLEANUP();
}

int
main(int argc, char *argv[])
{
    int		i, sts;
    int		c;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    char	*host = NULL;
    char	*endnum;
    int		errflag = 0;
    char	*p;
    extern char	*optarg;
    extern int	optind;
    int fail_count = 0;; 
    
    atexit(cleanup);
    
    PLATFORM_INIT();

    /* trim command name of leading directory components */
    prog = argv[0];
    for (p = prog; *p; p++) {
	if (*p == '/')
	    prog = p+1;
    }

    while ((c = getopt(argc, argv, "dn:h:p:?")) != EOF) {
	switch (c) {

	case 'd':	/* debug flag */
	    debug++;
	    break;

	case 'h':	/* (server) hostname */
	    server = 0;
	    host = optarg;
	    break;

	case 'n':
	    testnumber = atoi(optarg);
	    break;

	case 'p':	/* TCP/IP port */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -p argument must be a numeric\n", 
                        prog);
		exit(1);
		/*NOTREACHED*/
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-1) {
	usage();
	/*NOTREACHED*/
    }

    filename=argv[optind];
    if (do_open(O_RDWR) == FAIL)
	exit(1);

    setbuf(stderr, NULL);

    if (server) {
        int one = 1;
        
	s_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (s_fd == INVALID_SOCKET) {
	    perror("socket");
	    exit(1);
	    /*NOTREACHED*/
	}
	if (setsockopt(s_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(one)) < 0) {
	    perror("setsockopt(nodelay)");
	    exit(1);
	    /*NOTREACHED*/
	}
        if (setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one))<0) {
            perror("setsockopt(reuseaddr)");
            exit(1);
            /*NOTREACHED*/
        }
#ifdef SO_REUSEPORT
        if (setsockopt(s_fd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one))<0) {
            perror("setsockopt(reuseport)");
            exit(1);
            /*NOTREACHED*/
        }
#endif

	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons((short)port);
	sts = bind(s_fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
	if (sts < 0) {
	    perror("bind");
	    exit(1);
	    /*NOTREACHED*/
	}

	sts = listen(s_fd, 5);	/* Max. of 5 pending connection requests */
	if (sts == -1) {
	    perror("listen");
	    exit(1);
	    /*NOTREACHED*/
	}

	if (port == 0) {
	        socklen_t addr_len = sizeof(myAddr);

		if (getsockname(s_fd, &myAddr, &addr_len)) {
		    perror("getsockname");
		    exit(1);
		}

		port = ntohs(myAddr.sin_port);
	}

	printf("server port: %d\n", port);
	fflush(stdout);

	c_fd = accept(s_fd, NULL, NULL);
	if (c_fd == INVALID_SOCKET) {
	    perror("accept");
	    exit(1);
	    /*NOTREACHED*/
	}

	if (debug) fprintf(stderr, "Client accepted\n");
	SRAND(12345L);
    }
    else {
        struct hostent  *servInfo;

        if ((servInfo = gethostbyname(host)) == NULL) {
	    printf("Couldn't get hostbyname for %s", host);
	    if (h_errno == HOST_NOT_FOUND)
		printf(": host not found");
	    printf("\n");
            exit(1);
            /*NOTREACHED*/
        }

        c_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (c_fd == INVALID_SOCKET) {
            perror("socket");
            exit(1);
            /*NOTREACHED*/
        }
        /* avoid 200 ms delay */
        if (setsockopt(c_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&i, sizeof(i)) < 0) {
            perror("setsockopt(nodelay)");
            exit(1);
            /*NOTREACHED*/
        }
        /* Don't linger on close */
        if (setsockopt(c_fd, SOL_SOCKET, SO_LINGER, (char *)&noLinger, sizeof(noLinger)) < 0) {
            perror("setsockopt(nolinger)");
            exit(1);
            /*NOTREACHED*/
        }

	memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
	myAddr.sin_family = AF_INET;
	memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
	myAddr.sin_port = htons((short)port);

	if (connect(c_fd, (struct sockaddr*)&myAddr, sizeof(myAddr)) < 0) {
            perror("unable to connect");
            fprintf(stderr, "Server might still initializing the shared file\n ");
            exit(1);
            /*NOTREACHED*/
	}

	if (debug) fprintf(stderr, "Connected to server\n");
	SRAND(6789L);
    }

    if (server)
	/* only server need do shared file */
	initialize(f_fd);

    /*
     * TCP/IP connection to be established, safe to proceed.
     *
     * real work is in here ...
     */
    i = 0;
{
    int index = 0;
    int end = 0;
    int result = 0;
    int last_test = 0;
    int test_count = -1;
    int fail_flag = 0;
    while(!end) {
	if (server) {
	    if(testnumber > 0) {
		last_test = testnumber - 1;
		while(tests[index][TEST_NUM] != testnumber && tests[index][TEST_NUM] != 0) {
		    index++;
		}
	    }
	    /* If we have a server command, deal with it */
	    if(tests[index][WHO] == SERVER) {
		if(debug>1)
		    fprintf(stderr, "Got a server command (%d)\n", index);
		if(tests[index][TEST_NUM] == 0) {
		    index++;
		    continue;
		} 
		memset(&ctl, 0, sizeof(ctl));
		ctl.test = tests[index][TEST_NUM];

		if(tests[index][TEST_NUM] != 0) {
		    switch(tests[index][COMMAND]) {
			case CMD_WRLOCK:
			    result = do_lock(F_SETLK, F_WRLCK, tests[index][OFFSET], tests[index][LENGTH]);
			    break;
			case CMD_RDLOCK:
			    result = do_lock(F_SETLK, F_RDLCK, tests[index][OFFSET], tests[index][LENGTH]);
			    break;
			case CMD_UNLOCK:
			    result = do_lock(F_SETLK, F_UNLCK, tests[index][OFFSET], tests[index][LENGTH]);
			    break;
			case CMD_CLOSE:
			    result = do_close();
			    break;
			case CMD_OPEN:
			    result = do_open(tests[index][FLAGS]);
			    break;
			case CMD_WRTEST:
			    result = do_lock(F_GETLK, F_WRLCK, tests[index][OFFSET], tests[index][LENGTH]);
			    break;
			case CMD_RDTEST:
			    result = do_lock(F_GETLK, F_RDLCK, tests[index][OFFSET], tests[index][LENGTH]);
			    break;
		    }
		    if( result != tests[index][RESULT]) {
			fail_flag++;
			/* We have a failure */
			if(debug)
			    fprintf(stderr, "Server failure in test %d, while %sing using offset %lld, length %lld - err = %d:%s\n", 
					ctl.test,
					get_cmd_str(tests[index][COMMAND]),
						(long long)tests[index][OFFSET],
						(long long)tests[index][LENGTH],
						saved_errno, strerror(saved_errno));
			fprintf(stderr, "Server failure in %lld:%s\n",
					(long long)tests[index][TEST_NUM],
					descriptions[tests[index][TEST_NUM] - 1]);
		    }
		}
	    /* else send it off to the client */
	    } else if (tests[index][WHO] == CLIENT) {
		if(tests[index][TEST_NUM] == 0) {
		    ctl.test = 0;
		    end=1;
		} 
		/* get the client to do something */
		init_ctl(index);
		if(debug > 1)
		    fprintf(stderr, "Sending command to client (%d) - %s - %lld:%lld\n", 
					index,
					get_cmd_str(ctl.command),
					(long long)tests[index][OFFSET],
					(long long)tests[index][LENGTH]);
		/* get the client to do something */
		ctl.index = index;
		send_ctl();
		if(ctl.test != 0) {
		    /* Get the clients response */
		    recv_ctl();
		    /* this is the whether the test passed or failed,
		     * not what the command returned */
		    if( ctl.result == FAIL ) {
			fail_flag++;
			if(debug)
			    fprintf(stderr, "Client failure in test %d, while %sing using offset %lld, length %lld - err = %d:%s\n",
					ctl.test, get_cmd_str(ctl.command),
					(long long)ctl.offset, (long long)ctl.length,
					ctl.error, strerror(ctl.error));
			fprintf(stderr, "Client failure in %lld:%s\n",
					(long long)tests[index][TEST_NUM],
					descriptions[tests[index][TEST_NUM] - 1]);
		    }
		}
	    }

	    if(last_test != tests[index][TEST_NUM]) {
	        test_count++;
	        if(fail_flag)
		    fail_count++;
	        fail_flag = 0;
	        last_test = tests[index][TEST_NUM];
	    }
		
	    index++;
	} else { /* CLIENT */
	    if(debug > 2)
		fprintf(stderr,"client: waiting...\n");
	    /* wait for the server to do something */
	    recv_ctl();

	    /* check for a client command */
	    index = ctl.index;
	    if (tests[index][WHO] != CLIENT) { 
		fprintf(stderr, "not a client command index (%d)\n", index);
		exit(1);
	    }
		
	    if(ctl.test == 0) {
		end = 1;
		break;
	    }

	    switch(ctl.command) {
		case CMD_WRLOCK:
		    result = do_lock(F_SETLK, F_WRLCK, ctl.offset, ctl.length);
		    break;
		case CMD_RDLOCK:
		    result = do_lock(F_SETLK, F_RDLCK, ctl.offset, ctl.length);
		    break;
		case CMD_UNLOCK:
		    result = do_lock(F_SETLK, F_UNLCK, ctl.offset, ctl.length);
		    break;
		case CMD_CLOSE:
		    result = do_close();
		    break;
		case CMD_OPEN:
		    result = do_open(tests[index][FLAGS]);
		    break;
		case CMD_WRTEST:
		    result = do_lock(F_GETLK, F_WRLCK, ctl.offset, ctl.length);
		    break;
		case CMD_RDTEST:
		    result = do_lock(F_GETLK, F_RDLCK, ctl.offset, ctl.length);
		    break;
	    }
	    if( result != ctl.result ) {
		if(debug)
		    fprintf(stderr,"Got %d, wanted %d\n",
				result, ctl.result);
		ctl.result = FAIL;
		ctl.error = saved_errno;
		fail_count++;
	    } else {
		ctl.result = PASS;
		ctl.error = 0;
	    }
	    if(debug > 2)
		fprintf(stderr,"client: sending result to server (%d)\n", ctl.index);
	    /* Send result to the server */
	    send_ctl();
	    if(last_test != tests[index][TEST_NUM]) {
		test_count++;
		last_test = tests[index][TEST_NUM];
	    }
	}
    }
    if(server)
	printf("%d tests run, %d failed\n", test_count, fail_count);
}   
    
    exit(fail_count);
    /*NOTREACHED*/
}


