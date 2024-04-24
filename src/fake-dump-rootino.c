// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Fujitsu Limited.  All Rights Reserved. */
#define _LARGEFILE64_SOURCE
#include <endian.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <uuid/uuid.h>

// Definitions from xfsdump
#define PGSZLOG2	12
#define PGSZ		(1 << PGSZLOG2)
#define GLOBAL_HDR_SZ		PGSZ
#define GLOBAL_HDR_MAGIC_SZ	8
#define GLOBAL_HDR_STRING_SZ	0x100
#define GLOBAL_HDR_TIME_SZ	4
#define GLOBAL_HDR_UUID_SZ	0x10
typedef int32_t time32_t;
struct global_hdr {
	char gh_magic[GLOBAL_HDR_MAGIC_SZ];		/*   8    8 */
		/* unique signature of xfsdump */
	uint32_t gh_version;				/*   4    c */
		/* header version */
	uint32_t gh_checksum;				/*   4   10 */
		/* 32-bit unsigned additive inverse of entire header */
	time32_t gh_timestamp;				/*   4   14 */
		/* time32_t of dump */
	char gh_pad1[4];				/*   4   18 */
		/* alignment */
	uint64_t gh_ipaddr;				/*   8   20 */
		/* from gethostid(2), room for expansion */
	uuid_t gh_dumpid;				/*  10   30 */
		/* ID of dump session	 */
	char gh_pad2[0xd0];				/*  d0  100 */
		/* alignment */
	char gh_hostname[GLOBAL_HDR_STRING_SZ];	/* 100  200 */
		/* from gethostname(2) */
	char gh_dumplabel[GLOBAL_HDR_STRING_SZ];	/* 100  300 */
		/* label of dump session */
	char gh_pad3[0x100];				/* 100  400 */
		/* padding */
	char gh_upper[GLOBAL_HDR_SZ - 0x400];		/* c00 1000 */
		/* header info private to upper software layers */
};
typedef struct global_hdr global_hdr_t;

#define sizeofmember( t, m )	sizeof( ( ( t * )0 )->m )

#define DRIVE_HDR_SZ		sizeofmember(global_hdr_t, gh_upper)
struct drive_hdr {
	uint32_t dh_drivecnt;				/*   4    4 */
		/* number of drives used to dump the fs */
	uint32_t dh_driveix;				/*   4    8 */
		/* 0-based index of the drive used to dump this stream */
	int32_t dh_strategyid;				/*   4    c */
		/* ID of the drive strategy used to produce this dump */
	char dh_pad1[0x1f4];				/* 1f4  200 */
		/* padding */
	char dh_specific[0x200];			/* 200  400 */
		/* drive strategy-specific info */
	char dh_upper[DRIVE_HDR_SZ - 0x400];		/* 800  c00 */
		/* header info private to upper software layers */
};
typedef struct drive_hdr drive_hdr_t;

#define MEDIA_HDR_SZ		sizeofmember(drive_hdr_t, dh_upper)
struct media_hdr {
	char mh_medialabel[GLOBAL_HDR_STRING_SZ];	/* 100  100 */
		/* label of media object containing file */
	char mh_prevmedialabel[GLOBAL_HDR_STRING_SZ];	/* 100  200 */
		/* label of upstream media object */
	char mh_pad1[GLOBAL_HDR_STRING_SZ];		/* 100  300 */
		/* in case more labels needed */
	uuid_t mh_mediaid;				/*  10  310 */
		/* ID of media object 	*/
	uuid_t mh_prevmediaid;				/*  10  320 */
		/* ID of upstream media object */
	char mh_pad2[GLOBAL_HDR_UUID_SZ];		/*  10  330 */
		/* in case more IDs needed */
	uint32_t mh_mediaix;				/*   4  334 */
		/* 0-based index of this media object within the dump stream */
	uint32_t mh_mediafileix;			/*   4  338 */
		/* 0-based index of this file within this media object */
	uint32_t mh_dumpfileix;			/*   4  33c */
		/* 0-based index of this file within this dump stream */
	uint32_t mh_dumpmediafileix;			/*   4  340 */
		/* 0-based index of file within dump stream and media object */
	uint32_t mh_dumpmediaix;			/*   4  344 */
		/* 0-based index of this dump within the media object */
	int32_t mh_strategyid;				/*   4  348 */
		/* ID of the media strategy used to produce this dump */
	char mh_pad3[0x38];				/*  38  380 */
		/* padding */
	char mh_specific[0x80];			/*  80  400 */
		/* media strategy-specific info */
	char mh_upper[MEDIA_HDR_SZ - 0x400];		/* 400  800 */
		/* header info private to upper software layers */
};
typedef struct media_hdr media_hdr_t;

#define CONTENT_HDR_SZ		sizeofmember(media_hdr_t, mh_upper)
#define CONTENT_HDR_FSTYPE_SZ	16
#define CONTENT_STATSZ		160 /* must match dlog.h DLOG_MULTI_STATSZ */
struct content_hdr {
	char ch_mntpnt[GLOBAL_HDR_STRING_SZ];		/* 100  100 */
		/* full pathname of fs mount point */
	char ch_fsdevice[GLOBAL_HDR_STRING_SZ];	/* 100  200 */
		/* full pathname of char device containing fs */
	char  ch_pad1[GLOBAL_HDR_STRING_SZ];		/* 100  300 */
		/* in case another label is needed */
	char ch_fstype[CONTENT_HDR_FSTYPE_SZ];	/*  10  310 */
		/* from fsid.h */
	uuid_t ch_fsid;					/*  10  320 */
		/* file system uuid */
	char  ch_pad2[GLOBAL_HDR_UUID_SZ];		/*  10  330 */
		/* in case another id is needed */
	char ch_pad3[8];				/*   8  338 */
		/* padding */
	int32_t ch_strategyid;				/*   4  33c */
		/* ID of the content strategy used to produce this dump */
	char ch_pad4[4];				/*   4  340 */
		/* alignment */
	char ch_specific[0xc0];			/*  c0  400 */
		/* content strategy-specific info */
};
typedef struct content_hdr content_hdr_t;

typedef uint64_t xfs_ino_t;
struct startpt {
	xfs_ino_t sp_ino;		/* first inode to dump */
	off64_t sp_offset;	/* bytes to skip in file data fork */
	int32_t sp_flags;
	int32_t sp_pad1;
};
typedef struct startpt startpt_t;

#define CONTENT_INODE_HDR_SZ  sizeofmember(content_hdr_t, ch_specific)
struct content_inode_hdr {
	int32_t cih_mediafiletype;			/*   4   4 */
		/* dump media file type: see #defines below */
	int32_t cih_dumpattr;				/*   4   8 */
		/* dump attributes: see #defines below */
	int32_t cih_level;				/*   4   c */
		/* dump level */
	char pad1[4];					/*   4  10 */
		/* alignment */
	time32_t cih_last_time;				/*   4  14 */
		/* if an incremental, time of previous dump at a lesser level */
	time32_t cih_resume_time;			/*   4  18 */
		/* if a resumed dump, time of interrupted dump */
	xfs_ino_t cih_rootino;				/*   8  20 */
		/* root inode number */
	uuid_t cih_last_id;				/*  10  30 */
		/* if an incremental, uuid of prev dump */
	uuid_t cih_resume_id;				/*  10  40 */
		/* if a resumed dump, uuid of interrupted dump */
	startpt_t cih_startpt;				/*  18  58 */
		/* starting point of media file contents */
	startpt_t cih_endpt;				/*  18  70 */
		/* starting point of next stream */
	uint64_t cih_inomap_hnkcnt;			/*   8  78 */

	uint64_t cih_inomap_segcnt;			/*   8  80 */

	uint64_t cih_inomap_dircnt;			/*   8  88 */

	uint64_t cih_inomap_nondircnt;			/*   8  90 */

	xfs_ino_t cih_inomap_firstino;			/*   8  98 */

	xfs_ino_t cih_inomap_lastino;			/*   8  a0 */

	uint64_t cih_inomap_datasz;			/*   8  a8 */
		/* bytes of non-metadata dumped */
	char cih_pad2[CONTENT_INODE_HDR_SZ - 0xa8];	/*  18  c0 */
		/* padding */
};
typedef struct content_inode_hdr content_inode_hdr_t;
// End of definitions from xfsdump

int main(int argc, char *argv[]) {

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <path/to/dumpfile> <fake rootino>\n", argv[0]);
		exit(1);
	}

	const char *filepath = argv[1];
	const uint64_t fake_root_ino = (uint64_t)strtol(argv[2], NULL, 10);

	int fd = open(filepath, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	global_hdr_t *header = mmap(NULL, GLOBAL_HDR_SZ, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (header == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	drive_hdr_t *dh = (drive_hdr_t *)header->gh_upper;
	media_hdr_t *mh = (media_hdr_t *)dh->dh_upper;
	content_hdr_t *ch = (content_hdr_t *)mh->mh_upper;
	content_inode_hdr_t *cih = (content_inode_hdr_t *)ch->ch_specific;
	uint64_t *rootino_ptr = &cih->cih_rootino;

	int32_t checksum = (int32_t)be32toh(header->gh_checksum);
	uint64_t orig_rootino = be64toh(*rootino_ptr);

	// Fake root inode number
	*rootino_ptr = htobe64(fake_root_ino);

	// Update checksum along with overwriting rootino.
	uint64_t gap = orig_rootino - fake_root_ino;
	checksum += (gap >> 32) + (gap & 0x00000000ffffffff);
	header->gh_checksum = htobe32(checksum);

	munmap(header, GLOBAL_HDR_SZ);
	close(fd);
	return 0;
}
