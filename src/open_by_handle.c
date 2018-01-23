/*
 * open_by_handle.c - attempt to create a file handle and open it
 *                    with open_by_handle_at() syscall
 *
 * Copyright (C) 2017 CTERA Networks. All Rights Reserved.
 * Author: Amir Goldstein <amir73il@gmail.com>
 *
 * from:
 *  stale_handle.c
 *
 *  Copyright (C) 2010 Red Hat, Inc. All Rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*

usage: open_by_handle [-cludmrwapk] <test_dir> [num_files]

Examples:

1. Create test set of of test_dir with N files and try to get their NFS handles:

   open_by_handle -cp <test_dir> [N]

   This is used by new helper _require_exportfs() to check
   if filesystem supports exportfs

2. Get file handles for existing test set, drop caches and try to
   open all files by handle:

   open_by_handle -p <test_dir> [N]

3. Get file handles for existing test set, write data to files,
   drop caches, open all files by handle, read and verify written
   data, write new data to file:

   open_by_handle -rwa <test_dir> [N]

4. Get file handles for existing test set, unlink all test files,
   remove test_dir, drop caches, try to open all files by handle
   and expect ESTALE:

   open_by_handle -dp <test_dir> [N]

5. Get file handles for existing test set, keep open file handles for all
   test files, unlink all test files, drop caches and try to open all files
   by handle (should work):

   open_by_handle -dk <test_dir> [N]

6. Get file handles for existing test set, rename all test files,
   drop caches, try to open all files by handle (should work):

   open_by_handle -m <test_dir> [N]

7. Get file handles for existing test set, hardlink all test files,
   then unlink the original files, drop caches and try to open all
   files by handle (should work):

   open_by_handle -l <test_dir> [N]
   open_by_handle -u <test_dir> [N]

   This test is done with 2 invocations of the program, first to
   hardlink (-l) and then to unlink the originals (-u), because
   we would like to be able to perform the hardlinks on overlay
   lower layer and unlink on upper layer.

   NOTE that open_by_handle -u doesn't check if the files are
   hardlinked, it just assumes that they are.  If they are not
   then the test will fail, because file handles would be stale.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <linux/limits.h>
#include <libgen.h>

#define MAXFILES 1024

struct handle {
	struct file_handle fh;
	unsigned char fid[MAX_HANDLE_SZ];
} handle[MAXFILES], dir_handle;

void usage(void)
{
	fprintf(stderr, "usage: open_by_handle [-cludmrwapk] <test_dir> [num_files]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "open_by_handle -c <test_dir> [N] - create N test files under test_dir, try to get file handles and exit\n");
	fprintf(stderr, "open_by_handle    <test_dir> [N] - get file handles of test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -k <test_dir> [N] - get file handles of files that are kept open, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -w <test_dir> [N] - write data to test files before open by handle\n");
	fprintf(stderr, "open_by_handle -r <test_dir> [N] - read data from test files after open by handle and verify written data\n");
	fprintf(stderr, "open_by_handle -a <test_dir> [N] - write data to test files after open by handle\n");
	fprintf(stderr, "open_by_handle -l <test_dir> [N] - create hardlinks to test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -u <test_dir> [N] - unlink (hardlinked) test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -d <test_dir> [N] - unlink test files and hardlinks, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -m <test_dir> [N] - rename test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -p <test_dir>     - create/delete and try to open by handle also test_dir itself\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int	i, c;
	int	fd;
	int	ret = 0;
	int	failed = 0;
	char	dname[PATH_MAX];
	char	fname[PATH_MAX];
	char	fname2[PATH_MAX];
	char	*test_dir;
	char	*mount_dir;
	int	mount_fd, mount_id;
	int	numfiles = 1;
	int	create = 0, delete = 0, nlink = 1, move = 0;
	int	rd = 0, wr = 0, wrafter = 0, parent = 0;
	int	keepopen = 0;

	if (argc < 2 || argc > 4)
		usage();

	while ((c = getopt(argc, argv, "cludmrwapk")) != -1) {
		switch (c) {
		case 'c':
			create = 1;
			break;
		case 'w':
			/* Write data before open_by_handle_at() */
			wr = 1;
			break;
		case 'r':
			/* Read data after open_by_handle_at() */
			rd = 1;
			break;
		case 'a':
			/* Write data after open_by_handle_at() */
			wrafter = 1;
			break;
		case 'l':
			nlink = 2;
			break;
		case 'u':
			delete = 1;
			nlink = 1;
			break;
		case 'd':
			delete = 1;
			nlink = 0;
			break;
		case 'm':
			move = 1;
			break;
		case 'p':
			parent = 1;
			break;
		case 'k':
			keepopen = 1;
			break;
		default:
			fprintf(stderr, "illegal option '%s'\n", argv[optind]);
		case 'h':
			usage();
		}
	}
        if (optind == argc || optind > 2)
            usage();
	test_dir = argv[optind++];
	if (optind < argc)
		numfiles = atoi(argv[optind]);
	if (!numfiles || numfiles > MAXFILES) {
		fprintf(stderr, "illegal value '%s' for num_files\n", argv[optind]);
		usage();
	}

	if (parent) {
		strcpy(dname, test_dir);
		/*
		 * -p flag implies that test_dir is NOT a mount point,
		 * so its parent can be used as mount_fd for open_by_handle_at.
		 */
		mount_dir = dirname(dname);
		if (create)
			ret = mkdir(test_dir, 0700);
		if (ret < 0 && errno != EEXIST) {
			strcat(dname, ": mkdir");
			perror(dname);
			return EXIT_FAILURE;
		}
	} else {
		mount_dir = test_dir;
	}

	mount_fd = open(mount_dir, O_RDONLY|O_DIRECTORY);
	if (mount_fd < 0) {
		perror(mount_dir);
		return EXIT_FAILURE;
	}

	/*
	 * Create the test files and remove leftover hardlinks from previous run
	 */
	for (i=0; create && i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		sprintf(fname2, "%s/link%06d", test_dir, i);
		fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			strcat(fname, ": open(O_CREAT)");
			perror(fname);
			return EXIT_FAILURE;
		}
		close(fd);
		/* blow up leftovers hardlinks if they exist */
		ret = unlink(fname2);
		if (ret < 0 && errno != ENOENT) {
			strcat(fname2, ": unlink");
			perror(fname2);
			return EXIT_FAILURE;
		}
	}

	/* sync to get the new inodes to hit the disk */
	sync();

	/* create the handles */
	for (i=0; i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		handle[i].fh.handle_bytes = MAX_HANDLE_SZ;
		ret = name_to_handle_at(AT_FDCWD, fname, &handle[i].fh, &mount_id, 0);
		if (ret < 0) {
			strcat(fname, ": name_to_handle");
			perror(fname);
			return EXIT_FAILURE;
		}
		if (keepopen) {
			/* Open without close to keep unlinked files around */
			fd = open(fname, O_RDONLY);
			if (fd < 0) {
				strcat(fname, ": open(O_RDONLY)");
				perror(fname);
				return EXIT_FAILURE;
			}
		}
	}

	if (parent) {
		dir_handle.fh.handle_bytes = MAX_HANDLE_SZ;
		ret = name_to_handle_at(AT_FDCWD, test_dir, &dir_handle.fh, &mount_id, 0);
		if (ret < 0) {
			strcat(dname, ": name_to_handle");
			perror(dname);
			return EXIT_FAILURE;
		}
	}

	/* write data to files */
	for (i=0; wr && i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		fd = open(fname, O_WRONLY, 0644);
		if (fd < 0) {
			strcat(fname, ": open");
			perror(fname);
			return EXIT_FAILURE;
		}
		if (write(fd, "aaaa", 4) != 4) {
			strcat(fname, ": write before");
			perror(fname);
			return EXIT_FAILURE;
		}
		close(fd);
	}

	/* after creating test set only check that fs supports exportfs */
	if (create)
		return EXIT_SUCCESS;

	/* hardlink the files */
	for (i=0; nlink > 1 && i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		sprintf(fname2, "%s/link%06d", test_dir, i);
		ret = link(fname, fname2);
		if (ret < 0) {
			strcat(fname2, ": link");
			perror(fname2);
			return EXIT_FAILURE;
		}
	}

	/* rename the files */
	for (i=0; move && i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		sprintf(fname2, "%s/link%06d", test_dir, i);
		ret = rename(fname, fname2);
		if (ret < 0) {
			strcat(fname2, ": rename");
			perror(fname2);
			return EXIT_FAILURE;
		}
	}

	/* unlink the files */
	for (i=0; delete && i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		sprintf(fname2, "%s/link%06d", test_dir, i);
		ret = unlink(fname);
		if (ret < 0) {
			strcat(fname, ": unlink");
			perror(fname);
			return EXIT_FAILURE;
		}
		/* with -d flag, delete the hardlink if it exists */
		if (!nlink)
			ret = unlink(fname2);
		if (ret < 0 && errno != ENOENT) {
			strcat(fname2, ": unlink");
			perror(fname2);
			return EXIT_FAILURE;
		}
	}

	if (parent && delete && !nlink) {
		ret = rmdir(test_dir);
		if (ret < 0) {
			strcat(dname, ": rmdir");
			perror(dname);
			return EXIT_FAILURE;
		}
	}

	/* sync to get log forced for unlink transactions to hit the disk */
	sync();

	/* sync once more FTW */
	sync();

	/*
	 * now drop the caches so that unlinked inodes are reclaimed and
	 * buftarg page cache is emptied so that the inode cluster has to be
	 * fetched from disk again for the open_by_handle() call.
	 */
	ret = system("echo 3 > /proc/sys/vm/drop_caches");
	if (ret < 0) {
		perror("drop_caches");
		return EXIT_FAILURE;
	}

	/*
	 * now try to open the files by the stored handles. Expecting ESTALE
	 * if all files and their hardlinks have been unlinked.
	 */
	for (i=0; i < numfiles; i++) {
		errno = 0;
		fd = open_by_handle_at(mount_fd, &handle[i].fh, wrafter ? O_RDWR : O_RDONLY);
		if ((nlink || keepopen) && fd >= 0) {
			if (rd) {
				char buf[4] = {0};
				int size = read(fd, buf, 4);
				if (size < 0) {
					strcat(fname, ": read");
					perror(fname);
					return EXIT_FAILURE;
				}
				if (size < 4 || memcmp(buf, "aaaa", 4)) {
					printf("open_by_handle(%s) returned stale data '%.*s'!\n", fname, size, buf);
				}
			}
			if (wrafter && write(fd, "aaaa", 4) != 4) {
				strcat(fname, ": write after");
				perror(fname);
				return EXIT_FAILURE;
			}
			close(fd);
			continue;
		} else if (!nlink && !keepopen && fd < 0 && (errno == ENOENT || errno == ESTALE)) {
			continue;
		}
		sprintf(fname, "%s/file%06d", test_dir, i);
		if (fd >= 0) {
			printf("open_by_handle(%s) opened an unlinked file!\n", fname);
			close(fd);
		} else {
			printf("open_by_handle(%s) returned %d incorrectly on %s%s file!\n",
					fname, errno,
					nlink ? "a linked" : "an unlinked",
					keepopen ? " open" : "");
		}
		failed++;
	}

	if (parent) {
		fd = open_by_handle_at(mount_fd, &dir_handle.fh, O_RDONLY|O_DIRECTORY);
		if (fd >= 0) {
			if (!nlink) {
				printf("open_by_handle(%s) opened an unlinked dir!\n", dname);
				return EXIT_FAILURE;
			} else if (rd) {
				/*
				 * Sanity check dir fd - expect to access orig file IFF
				 * it was not unlinked nor renamed.
				 */
				strcpy(fname, "file000000");
				ret = faccessat(fd, fname, F_OK, 0);
				if ((ret == 0) != (!delete && !move) ||
				    ((ret < 0) && errno != ENOENT)) {
					strcat(fname, ": unexpected result from faccessat");
					perror(fname);
					return EXIT_FAILURE;
				}
				/*
				 * Expect to access link file if ran test with -l flag
				 * (nlink > 1), -m flag (orig moved to link name) or
				 * -u flag (which implied previous -l run).
				 */
				strcpy(fname2, "link000000");
				ret = faccessat(fd, fname2, F_OK, 0);
				if (ret < 0 && (nlink > 1 || delete || move ||
						errno != ENOENT)) {
					strcat(fname2, ": unexpected result from faccessat");
					perror(fname2);
					return EXIT_FAILURE;
				}
			}
			close(fd);
		} else if (nlink || !(errno == ENOENT || errno == ESTALE)) {
			printf("open_by_handle(%s) returned %d incorrectly on %s dir!\n",
					dname, errno,
					nlink ? "a linked" : "an unlinked");
			return EXIT_FAILURE;
		}
	}

	if (failed)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
