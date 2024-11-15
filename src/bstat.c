// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include "global.h"
#include <xfs/jdm.h>
#include <pthread.h>


int	debug;
int	quiet;
int	statit;
int	verbose;
int	threaded;

/*
 * libxfs is only available if a install-qa has been done from a local source
 * tree. That's not always done, so add the necessary functions here when not
 * detected by autoconf.
 */
#ifndef HAVE_XFS_LIBXFS_H
unsigned int
libxfs_log2_roundup(unsigned int i)
{
	unsigned int    rval;

	for (rval = 0; rval < NBBY * sizeof(i); rval++) {
		if ((1 << rval) >= i)
			break;
	}
	return rval;
}

static inline int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static inline int xfs_highbit32(__uint32_t v)
{
	return fls(v) - 1;
}
#endif

void
dotime(void *ti, char *s)
{
	char	*c;
	xfs_bstime_t *t;

	t = (xfs_bstime_t *)ti;

	c = ctime(&t->tv_sec);
	printf("\t%s %19.19s.%09d %s", s, c, t->tv_nsec, c + 20);
}

void
printbstat(struct xfs_bstat *sp)
{
	printf("ino %lld mode %#o nlink %d uid %d gid %d rdev %#x\n",
		(long long)sp->bs_ino, sp->bs_mode, sp->bs_nlink,
		sp->bs_uid, sp->bs_gid, sp->bs_rdev);
	printf("\tblksize %d size %lld blocks %lld xflags %#x extsize %d\n",
		sp->bs_blksize, (long long)sp->bs_size, (long long)sp->bs_blocks,
		sp->bs_xflags, sp->bs_extsize);
	dotime(&sp->bs_atime, "atime");
	dotime(&sp->bs_mtime, "mtime");
	dotime(&sp->bs_ctime, "ctime");
	printf( "\textents %d %d gen %d\n",
		sp->bs_extents, sp->bs_aextents, sp->bs_gen);
	printf( "\tDMI: event mask 0x%08x state 0x%04x\n",
		sp->bs_dmevmask, sp->bs_dmstate);
}

void
printstat(struct stat64 *sp)
{
	printf("ino %lld mode %#o nlink %d uid %d gid %d rdev %#x\n",
		(long long)sp->st_ino, (unsigned int)sp->st_mode, (int)sp->st_nlink,
		(int)sp->st_uid, (int)sp->st_gid, (int)sp->st_rdev);
	printf("\tblksize %llu size %lld blocks %lld\n",
		(unsigned long long)sp->st_blksize, (long long)sp->st_size, (long long)sp->st_blocks);
	dotime(&sp->st_atime, "atime");
	dotime(&sp->st_mtime, "mtime");
	dotime(&sp->st_ctime, "ctime");
}

static int
do_bstat(
	int		fsfd,
	jdm_fshandle_t	*fshandlep,
	char		*name,
	int		nent,
	__u64		first,
	__u64		last)
{
	__s32		count;
	int		total = 0;
	struct xfs_bstat	*t;
	int		ret;
	struct xfs_fsop_bulkreq bulkreq;
	__u64		ino;

	t = malloc(nent * sizeof(*t));

	if (verbose)
		printf(
		  "XFS_IOC_FSBULKSTAT test: last=%lld nent=%d\n",
			(long long)last, nent);

	ino = first;

	bulkreq.lastip  = &ino;
	bulkreq.icount  = nent;
	bulkreq.ubuffer = t;
	bulkreq.ocount  = &count;

	while ((ret = xfsctl(name, fsfd, XFS_IOC_FSBULKSTAT, &bulkreq)) == 0) {
		int		i;

		total += count;

		if (verbose)
			printf(
"XFS_IOC_FSBULKSTAT: first/last/ino=%lld/%lld/%lld ret=%d count=%d total=%d\n", 
				(long long)first, (long long)last,
				(long long)ino, ret, count, total);
		if (count == 0)
			break;

		if (quiet && !statit)
			goto next;

		for (i = 0; i < count; i++) {
			if (!quiet)
				printbstat(&t[i]);

			if (statit) {
				char *cc_readlinkbufp;
				int cc_readlinkbufsz;
				struct stat64	sb;
				int nread;
				int		fd;

				switch(t[i].bs_mode & S_IFMT) {
				case S_IFLNK:
					cc_readlinkbufsz = MAXPATHLEN;
					cc_readlinkbufp = (char *)calloc( 
							      1, 
							      cc_readlinkbufsz);
					nread = jdm_readlink( 
						   fshandlep,
						   &t[i],
						   cc_readlinkbufp,
						   cc_readlinkbufsz);
					if (verbose && nread > 0)
						printf(
						 "readlink: ino %lld: <%*s>\n",
							    (long long)t[i].bs_ino,
							    nread,
							    cc_readlinkbufp);
					free(cc_readlinkbufp);
					if ( nread < 0 ) {
						printf(
					      "could not read symlink ino %llu\n",
						      (unsigned long long)t[i].bs_ino );
						printbstat(&t[i]);
					}
					break;

				case S_IFCHR:
				case S_IFBLK:
				case S_IFIFO:
				case S_IFSOCK:
					break;

				case S_IFREG:
				case S_IFDIR:
					fd = jdm_open( fshandlep, &t[i], O_RDONLY);
					if (fd < 0) {
						printf(
					"unable to open handle ino %lld: %s\n",
							(long long)t[i].bs_ino,
							strerror(errno));
						continue;
					}
					if (fstat64(fd, &sb) < 0) {
						printf(
					"unable to stat ino %lld: %s\n",
							(long long)t[i].bs_ino,
							strerror(errno));
					}
					close(fd);

					/*
					 * Don't compare blksize or blocks, 
					 * they are used differently by stat
					 * and bstat.
					 */
					if ( (t[i].bs_ino != sb.st_ino) ||
					     (t[i].bs_mode != sb.st_mode) ||
					     (t[i].bs_nlink != sb.st_nlink) ||
					     (t[i].bs_uid != sb.st_uid) ||
					     (t[i].bs_gid != sb.st_gid) ||
					     (t[i].bs_rdev != sb.st_rdev) ||
					     (t[i].bs_size != sb.st_size) ||
					     /* (t[i].bs_blksize != sb.st_blksize) || */
					     (t[i].bs_mtime.tv_sec != sb.st_mtime) ||
					     (t[i].bs_ctime.tv_sec != sb.st_ctime) ) {
						printf("\nstat/bstat missmatch\n");
						printbstat(&t[i]);
						printstat(&sb);
					}
				}
			}
		}
next:
		if (debug)
			break;

		if (ino >= last)
			break;
	}
	if (verbose)
		printf(
	    "XFS_IOC_FSBULKSTAT test: last=%lld nent=%d ret=%d count=%d\n", 
			(long long)last, nent, ret, count);

	return total;
}

struct thread_args {
	pthread_t	tid;
	int		fsfd;
	jdm_fshandle_t	*fshandlep;
	char		*name;
	int		nent;
	__u64		first;
	__u64		last;
	int		ret;
};

static void *
do_bstat_thread(
	void	*args)
{
	struct thread_args *targs = args;

	targs->ret = do_bstat(targs->fsfd, targs->fshandlep, targs->name,
			      targs->nent, targs->first, targs->last);
	return NULL;
}

/*
 * Always show full working:
 *
 *   XFS_AGINO_TO_INO(mp,a,i)        \
 *           (((xfs_ino_t)(a) << XFS_INO_AGINO_BITS(mp)) | (i))
 *
 *	i always zero, so:
 *			a << XFS_INO_AGINO_BITS(mp)
 *
 *   XFS_INO_AGINO_BITS(mp)          (mp)->m_agino_log
 *
 *   mp->m_agino_log = sbp->sb_inopblog + sbp->sb_agblklog
 *
 *   sb_inopblog = fsgeom.blocksize / fsgeom.inodesize
 *   sb_agblklog = (__uint8_t)libxfs_log2_roundup((unsigned int)fsgeom.agblocks);
 *
 *	a << (libxfs_highbit32(fsgeom.blocksize /fsgeom.inodesize) +
 *				libxfs_log2_roundup(fsgeom.agblocks));
 */
#define FSGEOM_INOPBLOG(fsg) \
		(xfs_highbit32((fsg).blocksize / (fsg).inodesize))
#define FSGEOM_AGINO_TO_INO(fsg, a, i) \
	(((__u64)(a) << (FSGEOM_INOPBLOG(fsg) + \
			libxfs_log2_roundup((fsg).agblocks))) | (i))

/*
 *    XFS_OFFBNO_TO_AGINO(mp,b,o)     \
 *            ((xfs_agino_t)(((b) << XFS_INO_OFFSET_BITS(mp)) | (o)))
 *
 *       i always zero, so:
 *			b << XFS_INO_OFFSET_BITS(mp)
 *
 *    XFS_INO_OFFSET_BITS(mp)         (mp)->m_sb.sb_inopblog
 */
#define FSGEOM_OFFBNO_TO_AGINO(fsg, b, o) \
		((__u32)(((b) << FSGEOM_INOPBLOG(fsg)) | (o)))
static int
do_threads(
	int		fsfd,
	jdm_fshandle_t	*fshandlep,
	char		*name,
	int		nent,
	__u64		first,
	int		numthreads)
{
	struct xfs_fsop_geom geom;
	struct thread_args *targs;
	int		ntargs = 0;
	int		ret;
	int		i, j;
	int		total = 0;


	/* get number of AGs */
	ret = ioctl(fsfd, XFS_IOC_FSGEOMETRY, &geom);
	if (ret) {
		perror("XFS_IOC_FSGEOMETRY");
		exit(1);
	}

	/* allocate thread array */
	targs = malloc(geom.agcount * sizeof(*targs));
	if (ret) {
		perror("malloc(targs)");
		exit(1);
	}

	for (i = 0; i < geom.agcount; i++) {
		__u64 last;

		last = FSGEOM_AGINO_TO_INO(geom, i,
					   FSGEOM_OFFBNO_TO_AGINO(geom,
						       geom.agblocks - 1, 0));

		if (first > last) {
			i--;
			continue;
		}
		first = MAX(first, FSGEOM_AGINO_TO_INO(geom, i, 0));

		targs[i].fsfd = fsfd;
		targs[i].fshandlep = fshandlep;
		targs[i].name = name;
		targs[i].nent = nent;
		targs[i].first = first;
		targs[i].last = last;
		targs[i].ret = 0;
	}
	ntargs = i;

	/* start threads */
	i = 0;
	while (i < ntargs) {
		for (j = 0; j < numthreads; j++) {
			int tgt = i + j;

			if (tgt >= ntargs)
				break;

			ret = pthread_create(&targs[tgt].tid, NULL,
					     do_bstat_thread, &targs[tgt]);
			if (ret) {
				perror("pthread-create");
				exit(1);
			}
		}


		/* join threads */
		for (j--; j >= 0; j--) {
			int tgt = i + j;

			if (targs[tgt].tid) {
				pthread_join(targs[tgt].tid, NULL);
				total += targs[tgt].ret;
			}
		}
		i += numthreads;
	}

	/* die */
	return total;
}

void
usage(void)
{
	printf("\n"\
"Usage:\n"\
"\n"\
"	xfs_bstat [-c] [-q] [-v] [-l <num>] [-t <num>] [ dir [ batch_size ]]\n"\
"\n"\
"   -c   Check the results against stat(3) output\n"\
"   -q   Quiet\n"\
"   -v   Verbose output\n"\
"   -l <num>  Inode to start with\n"\
"   -t <num>  Threads to run\n");

	exit(1);
}


int
main(int argc, char **argv)
{
	int		fsfd;
	__u64		first = 0;
	char		*name;
	int		nent;
	int		ret;
	jdm_fshandle_t	*fshandlep = NULL;
	int		c;
	int		numthreads = 0;

	while ((c = getopt(argc, argv, "cdl:qt:v")) != -1) {
		switch (c) {
		case 'q':
			quiet = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'c':
			statit = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'l':
			first = atoi(optarg);
			break;
		case 't':
			threaded = 1;
			numthreads = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		name = ".";
	else
		name = *argv;

	if (argc < 2)
		nent = 4096;
	else
		nent = atoi(*++argv);

	fsfd = open(name, O_RDONLY);
	if (fsfd < 0) {
		perror(name);
		exit(1);
	}

	if (verbose)
		printf("Bulkstat test on %s, batch size=%d statcheck=%d\n", 
			name, nent, statit);

	if (statit) {
		fshandlep = jdm_getfshandle( name );
		if (! fshandlep) {
			printf("unable to construct sys handle for %s: %s\n",
			  name, strerror(errno));
			return -1;
		}
	}

	if (threaded)
		ret = do_threads(fsfd, fshandlep, name, nent, first, numthreads);
	else
		ret = do_bstat(fsfd, fshandlep, name, nent, first, -1LL);

	if (verbose)
		printf("Bulkstat found %d inodes\n", ret);

	if (fsfd)
		close(fsfd);

	if (ret < 0 )
		perror("xfsctl(XFS_IOC_FSBULKSTAT)");

	return 0;
}
