/*
 * Copyright (c) 2009 Josef Bacik
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License V2
 * as published by the Free Software Foundation.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fiemap.h>

/* Global for non-critical message suppression */
int quiet;

static void
usage(void)
{
	printf("Usage: fiemap-tester [-m map] [-r number of runs] [-s seed] [-qS]");
	printf("[-p preallocate (1/0)] ");
	printf("filename\n");
	printf("  -m map    : generate a file with the map given and test\n");
	printf("  -p 0/1    : turn block preallocation on or off\n");
	printf("  -r count  : number of runs to execute (default infinity)\n");
	printf("  -s seed   : seed for random map generator (default 1)\n");
	printf("  -q        : be quiet about non-errors\n");
	printf("  -S        : sync file before mapping (via ioctl flags)\n");
	printf("-m and -r cannot be used together\n");
	exit(EXIT_FAILURE);
}

static char *
generate_file_mapping(int blocks, int prealloc)
{
	char *map;
	int num_types = 2, cur_block = 0;
	int i = 0;

	map = malloc(sizeof(char) * blocks);
	if (!map)
		return NULL;

	if (prealloc)
		num_types++;


	for (i = 0; i < blocks; i++) {
		long num = random() % num_types;
		switch (num) {
		case 0:
			map[cur_block] = 'D';
			break;
		case 1:
			map[cur_block] = 'H';
			break;
		case 2:
			map[cur_block] = 'P';
			break;
		}
		cur_block++;
	}

	return map;
}

static int
create_file_from_mapping(int fd, char *map, int blocks, int blocksize)
{
	int cur_offset = 0, ret = 0, bufsize;
	char *buf;
	int i = 0;

	bufsize = sizeof(char) * blocksize;
	if (posix_memalign((void **)&buf, 4096, bufsize))
		return -1;

	memset(buf, 'a', bufsize);

	for (i = 0; i < blocks; i++) {
		switch (map[i]) {
		case 'D':
			ret = write(fd, buf, bufsize);
			if (ret < bufsize) {
				printf("Short write\n");
				ret = -1;
				goto out;
			}
			break;
#ifdef HAVE_FALLOCATE
		case 'P':
			ret = fallocate(fd, 0, cur_offset, blocksize);
			if (ret < 0) {
				printf("Error fallocating\n");
				goto out;
			}
			/* fallthrough; seek to end of prealloc space */
#endif
		case 'H':
			ret = lseek(fd, blocksize, SEEK_CUR);
			if (ret == (off_t)-1) {
				printf("Error lseeking\n");
				ret = -1;
				goto out;
			}
			break;
		default:
			printf("Hrm, unrecognized flag in map\n");
			ret = -1;
			goto out;
		}
		cur_offset += blocksize;
	}

	ret = 0;
out:
	return ret;
}

static void
show_extent_block(struct fiemap_extent *extent, int blocksize)
{
	__u64	logical = extent->fe_logical;
	__u64	phys = extent->fe_physical;
	__u64	length = extent->fe_length;
	int	flags = extent->fe_flags;

	printf("logical: [%8llu..%8llu] phys: %8llu..%8llu "
	       "flags: 0x%03X tot: %llu\n",
		logical / blocksize, (logical + length - 1) / blocksize,
		phys / blocksize, (phys + length - 1) / blocksize,
		flags,
		(length / blocksize));
}

static void
show_extents(struct fiemap *fiemap, int blocksize)
{
	unsigned int i;

	for (i = 0; i < fiemap->fm_mapped_extents; i++)
		show_extent_block(&fiemap->fm_extents[i], blocksize);
}

static int
check_flags(struct fiemap *fiemap, int blocksize)
{
	struct fiemap_extent *extent;
	__u64 aligned_offset, aligned_length;
	int c;

	for (c = 0; c < fiemap->fm_mapped_extents; c++) {
		extent = &fiemap->fm_extents[c];

		aligned_offset = extent->fe_physical & ~((__u64)blocksize - 1);
		aligned_length = extent->fe_length & ~((__u64)blocksize - 1);

		if ((aligned_offset != extent->fe_physical ||
		     aligned_length != extent->fe_length) &&
		    !(extent->fe_flags & FIEMAP_EXTENT_NOT_ALIGNED)) {
			printf("ERROR: FIEMAP_EXTENT_NOT_ALIGNED is not set "
			       "but the extent is unaligned: %llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}

		if (extent->fe_flags & FIEMAP_EXTENT_DATA_ENCRYPTED &&
		    !(extent->fe_flags & FIEMAP_EXTENT_ENCODED)) {
			printf("ERROR: FIEMAP_EXTENT_DATA_ENCRYPTED is set, "
			       "but FIEMAP_EXTENT_ENCODED is not set: %llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}

		if (extent->fe_flags & FIEMAP_EXTENT_NOT_ALIGNED &&
		    aligned_offset == extent->fe_physical &&
		    aligned_length == extent->fe_length) {
			printf("ERROR: FIEMAP_EXTENT_NOT_ALIGNED is set but "
			       "offset and length is blocksize aligned: "
			       "%llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}

		if (extent->fe_flags & FIEMAP_EXTENT_LAST &&
		    c + 1 < fiemap->fm_mapped_extents) {
			printf("ERROR: FIEMAP_EXTENT_LAST is set but there are"
			       " more extents left: %llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}

		if (extent->fe_flags & FIEMAP_EXTENT_DELALLOC &&
		    !(extent->fe_flags & FIEMAP_EXTENT_UNKNOWN)) {
			printf("ERROR: FIEMAP_EXTENT_DELALLOC is set but "
			       "FIEMAP_EXTENT_UNKNOWN is not set: %llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}

		if (extent->fe_flags & FIEMAP_EXTENT_DATA_INLINE &&
		    !(extent->fe_flags & FIEMAP_EXTENT_NOT_ALIGNED)) {
			printf("ERROR: FIEMAP_EXTENT_DATA_INLINE is set but "
			       "FIEMAP_EXTENT_NOT_ALIGNED is not set: %llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}

		if (extent->fe_flags & FIEMAP_EXTENT_DATA_TAIL &&
		    !(extent->fe_flags & FIEMAP_EXTENT_NOT_ALIGNED)) {
			printf("ERROR: FIEMAP_EXTENT_DATA_TAIL is set but "
			       "FIEMAP_EXTENT_NOT_ALIGNED is not set: %llu\n",
			       (unsigned long long)
			       (extent->fe_logical / blocksize));
			return -1;
		}
	}

	return 0;
}

static int
check_data(struct fiemap *fiemap, __u64 logical_offset, int blocksize,
	   int last, int prealloc)
{
	struct fiemap_extent *extent;
	__u64 orig_offset = logical_offset;
	int c, found = 0;

	for (c = 0; c < fiemap->fm_mapped_extents; c++) {
		__u64 start, end;
		extent = &fiemap->fm_extents[c];

		start = extent->fe_logical;
		end = extent->fe_logical + extent->fe_length;

		if (logical_offset > end)
			continue;

		if (logical_offset + blocksize < start)
			break;

		if (logical_offset >= start &&
		    logical_offset < end) {
			if (prealloc &&
			    !(extent->fe_flags & FIEMAP_EXTENT_UNWRITTEN)) {
				printf("ERROR: preallocated extent is not "
				       "marked with FIEMAP_EXTENT_UNWRITTEN: "
				       "%llu\n",
				       (unsigned long long)
				       (start / blocksize));
				return -1;
			}

			if (logical_offset + blocksize > end) {
				logical_offset = end+1;
				continue;
			} else {
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		printf("ERROR: couldn't find extent at %llu\n",
		       (unsigned long long)(orig_offset / blocksize));
	} else if (last &&
		   !(fiemap->fm_extents[c].fe_flags & FIEMAP_EXTENT_LAST)) {
		printf("ERROR: last extent not marked as last: %llu\n",
		       (unsigned long long)(orig_offset / blocksize));
		found = 0;
	}

	return (!found) ? -1 : 0;
}

static int
check_weird_fs_hole(int fd, __u64 logical_offset, int blocksize)
{
	static int warning_printed = 0;
	int block, i;
	size_t buf_len = sizeof(char) * blocksize;
	char *buf;

	block = (int)(logical_offset / blocksize);
	if (ioctl(fd, FIBMAP, &block) < 0) {
		perror("Can't fibmap file");
		return -1;
	}

	if (!block) {
		printf("ERROR: FIEMAP claimed there was data at a block "
		       "which should be a hole, and FIBMAP confirmend that "
		       "it is in fact a hole, so FIEMAP is wrong: %llu\n",
		       (unsigned long long)(logical_offset / blocksize));
		return -1;
	}

	buf = malloc(buf_len);
	if (!buf) {
		perror("Could not allocate temporary buffer");
		return -1;
	}

	if (pread(fd, buf, buf_len, (off_t)logical_offset) < 0) {
		perror("Error reading from file");
		free(buf);
		return -1;
	}

	for (i = 0; i < buf_len; i++) {
		if (buf[i] != 0) {
			printf("ERROR: FIEMAP claimed there was data (%c) at "
			       "block %llu that should have been a hole, and "
			       "FIBMAP confirmed that it was allocated, but "
			       "it should be filled with 0's, but it was not "
			       "so you have a big problem!\n",
			       buf[i],
			       (unsigned long long)(logical_offset / blocksize));
			free(buf);
			return -1;
		}
	}

	if (warning_printed || quiet) {
		free(buf);
		return 0;
	}

	printf("HEY FS PERSON: your fs is weird.  I specifically wanted a\n"
	       "hole and you allocated a block anyway.  FIBMAP confirms that\n"
	       "you allocated a block, and the block is filled with 0's so\n"
	       "everything is kosher, but you still allocated a block when\n"
	       "didn't need to.  This may or may not be what you wanted,\n"
	       "which is why I'm only printing this message once, in case\n"
	       "you didn't do it on purpose. This was at block %llu.\n",
	       (unsigned long long)(logical_offset / blocksize));
	warning_printed = 1;
	free(buf);

	return 0;
}

static int
check_hole(struct fiemap *fiemap, int fd, __u64 logical_offset, int blocksize)
{
	struct fiemap_extent *extent;
	int c;

	for (c = 0; c < fiemap->fm_mapped_extents; c++) {
		__u64 start, end;
		extent = &fiemap->fm_extents[c];

		start = extent->fe_logical;
		end = extent->fe_logical + extent->fe_length;

		if (logical_offset > end)
			continue;
		if (logical_offset + blocksize < start)
			break;

		if (logical_offset >= start &&
		    logical_offset < end) {

			if (check_weird_fs_hole(fd, logical_offset,
						blocksize) == 0)
				break;

			printf("ERROR: found an allocated extent where a hole "
			       "should be: %llu\n",
			       (unsigned long long)(start / blocksize));
			return -1;
		}
	}

	return 0;
}

static int query_fiemap_count(int fd, int blocks, int blocksize)
{
	struct fiemap fiemap = { 0, };

	fiemap.fm_length = blocks * blocksize;

	if (ioctl(fd, FS_IOC_FIEMAP, (unsigned long)&fiemap) < 0) {
		perror("FIEMAP query ioctl failed");
		return -1;
	}

	return 0;
}

static int
compare_fiemap_and_map(int fd, char *map, int blocks, int blocksize, int syncfile)
{
	struct fiemap *fiemap;
	char *fiebuf;
	int blocks_to_map, ret, cur_extent = 0, last_data = 0;
	__u64 map_start, map_length;
	int i, c;

	if (query_fiemap_count(fd, blocks, blocksize) < 0)
		return -1;

	blocks_to_map = (random() % blocks) + 1;
	fiebuf = malloc(sizeof(struct fiemap) +
			(blocks_to_map * sizeof(struct fiemap_extent)));
	if (!fiebuf) {
		perror("Could not allocate fiemap buffers");
		return -1;
	}

	fiemap = (struct fiemap *)fiebuf;
	map_start = 0;
	map_length = blocks_to_map * blocksize;

	for (i = 0; i < blocks; i++) {
		if (map[i] != 'H')
			last_data = i;
	}

	fiemap->fm_flags = syncfile ? FIEMAP_FLAG_SYNC : 0;
	fiemap->fm_extent_count = blocks_to_map;
	fiemap->fm_mapped_extents = 0;

	do {
		fiemap->fm_start = map_start;
		fiemap->fm_length = map_length;

		ret = ioctl(fd, FS_IOC_FIEMAP, (unsigned long)fiemap);
		if (ret < 0) {
			perror("FIEMAP ioctl failed");
			free(fiemap);
			return -1;
		}

		if (check_flags(fiemap, blocksize))
			goto error;

		for (i = cur_extent, c = 1; i < blocks; i++, c++) {
			__u64 logical_offset = i * blocksize;

			if (c > fiemap->fm_mapped_extents) {
				i++;
				break;
			}

			switch (map[i]) {
			case 'D':
				if (check_data(fiemap, logical_offset,
					       blocksize, last_data == i, 0))
					goto error;
				break;
			case 'H':
				if (check_hole(fiemap, fd, logical_offset,
					       blocksize))
					goto error;
				break;
			case 'P':
				if (check_data(fiemap, logical_offset,
					       blocksize, last_data == i, 1))
					goto error;
				break;
			default:
				printf("ERROR: weird value in map: %c\n",
				       map[i]);
				goto error;
			}
		}
		cur_extent = i;
		map_start = i * blocksize;
	} while (cur_extent < blocks);

	ret = 0;
	return ret;
error:
	printf("map is '%s'\n", map);
	show_extents(fiemap, blocksize);
	return -1;
}

int
main(int argc, char **argv)
{
	int	blocksize = 0;	/* filesystem blocksize */
	int	fd;		/* file descriptor */
	int	opt;
	int	rc;
	char	*fname;		/* filename to map */
	char	*map = NULL;	/* file map to generate */
	int	runs = -1;	/* the number of runs to have */
	int	blocks = 0;	/* the number of blocks to generate */
	int	maxblocks = 0;	/* max # of blocks to create */
	int	prealloc = 1;	/* whether or not to do preallocation */
	int	syncfile = 0;	/* whether fiemap should  sync file first */
	int	seed = 1;

	while ((opt = getopt(argc, argv, "m:r:s:p:qS")) != -1) {
		switch(opt) {
		case 'm':
			map = strdup(optarg);
			break;
		case 'p':
			prealloc = atoi(optarg);;
#ifndef HAVE_FALLOCATE
			if (prealloc) {
				printf("Not built with preallocation support\n");
				usage();
			}
#endif
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			runs = atoi(optarg);
			break;
		case 's':
			seed = atoi(optarg);
			break;
		/* sync file before mapping */
		case 'S':
			syncfile = 1;
			break;
		default:
			usage();
		}
	}

	if (runs != -1 && map)
		usage();

	fname = argv[optind++];
	if (!fname)
		usage();

	fd = open(fname, O_RDWR|O_CREAT|O_TRUNC|O_DIRECT, 0644);
	if (fd < 0) {
		perror("Can't open file");
		exit(1);
	}

	if (ioctl(fd, FIGETBSZ, &blocksize) < 0) {
		perror("Can't get filesystem block size");
		close(fd);
		exit(1);
	}

#ifdef HAVE_FALLOCATE
	/* if fallocate passes, then we can do preallocation, else not */
	if (prealloc) {
		prealloc = !((int)fallocate(fd, 0, 0, blocksize));
		if (!prealloc)
			printf("preallocation not supported, disabling\n");
	}
#else
	prealloc = 0;
#endif

	if (ftruncate(fd, 0)) {
		perror("Can't truncate file");
		close(fd);
		exit(1);
	}

	if (map) {
		blocks = strlen(map);
		runs = 0;
	}

	srandom(seed);

	/* max file size 2mb / block size */
	maxblocks = (2 * 1024 * 1024) / blocksize;

	if (runs == -1)
		printf("Starting infinite run, if you don't see any output "
		       "then its working properly.\n");
	do {
		if (!map) {
			blocks = random() % maxblocks;
			if (blocks == 0) {
				if (!quiet)
					printf("Skipping 0 length file\n");
				continue;
			}

			map = generate_file_mapping(blocks, prealloc);
			if (!map) {
				printf("Could not create map\n");
				exit(1);
			}
		}

		rc = create_file_from_mapping(fd, map, blocks, blocksize);
		if (rc) {
			perror("Could not create file\n");
			free(map);
			close(fd);
			exit(1);
		}

		rc = compare_fiemap_and_map(fd, map, blocks, blocksize, syncfile);
		if (rc) {
			printf("Problem comparing fiemap and map\n");
			free(map);
			close(fd);
			exit(1);
		}

		free(map);
		map = NULL;

		if (ftruncate(fd, 0)) {
			perror("Could not truncate file\n");
			close(fd);
			exit(1);
		}

		if (lseek(fd, 0, SEEK_SET)) {
			perror("Could not seek set\n");
			close(fd);
			exit(1);
		}

		if (runs) runs--;
	} while (runs != 0);

	close(fd);

	return 0;
}
