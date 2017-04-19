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

usage: open_by_handle [-c|-l|-u|-d] <test_dir> [num_files]

Examples:

1. Create test set of N files and try to get their NFS handles:

   open_by_handle -c <test_dir> [N]

   This is used by new helper _require_exportfs() to check
   if filesystem supports exportfs

2. Get file handles for existing test set, drop caches and try to
   open all files by handle:

   open_by_handle <test_dir> [N]

3. Get file handles for existing test set, unlink all test files,
   drop caches, try to open all files by handle and expect ESTALE:

   open_by_handle -d <test_dir> [N]

4. Get file handles for existing test set, hardlink all test files,
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

#define MAXFILES 1024

struct handle {
	struct file_handle fh;
	unsigned char fid[MAX_HANDLE_SZ];
} handle[MAXFILES];

void usage(void)
{
	fprintf(stderr, "usage: open_by_handle [-c|-l|-u|-d] <test_dir> [num_files]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "open_by_handle -c <test_dir> [N] - create N test files under test_dir, try to get file handles and exit\n");
	fprintf(stderr, "open_by_handle    <test_dir> [N] - get file handles of test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -l <test_dir> [N] - create hardlinks to test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -u <test_dir> [N] - unlink (hardlinked) test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -d <test_dir> [N] - unlink test files and hardlinks, drop caches and try to open by handle\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int	i, c;
	int	fd;
	int	ret;
	int	failed = 0;
	char	fname[PATH_MAX];
	char	fname2[PATH_MAX];
	char	*test_dir;
	int	mount_fd, mount_id;
	int	numfiles = 1;
	int	create = 0, delete = 0, nlink = 1;

	if (argc < 2 || argc > 4)
		usage();

	while ((c = getopt(argc, argv, "clud")) != -1) {
		switch (c) {
		case 'c':
			create = 1;
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

	mount_fd = open(test_dir, O_RDONLY|O_DIRECTORY);
	if (mount_fd < 0) {
		perror(test_dir);
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
			printf("Warning (%s,%d), open(%s) failed.\n", __FILE__, __LINE__, fname);
			perror(fname);
			return EXIT_FAILURE;
		}
		close(fd);
		/* blow up leftovers hardlinks if they exist */
		ret = unlink(fname2);
		if (ret < 0 && errno != ENOENT) {
			perror("unlink");
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
			perror("name_to_handle");
			return EXIT_FAILURE;
		}
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
			perror("link");
			return EXIT_FAILURE;
		}
	}

	/* unlink the files */
	for (i=0; delete && i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		sprintf(fname2, "%s/link%06d", test_dir, i);
		ret = unlink(fname);
		if (ret < 0) {
			perror("unlink");
			return EXIT_FAILURE;
		}
		/* with -d flag, delete the hardlink if it exists */
		if (!nlink)
			ret = unlink(fname2);
		if (ret < 0 && errno != ENOENT) {
			perror("unlink");
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
		fd = open_by_handle_at(mount_fd, &handle[i].fh, O_RDWR);
		if (nlink && fd >= 0) {
			close(fd);
			continue;
		} else if (!nlink && fd < 0 && (errno == ENOENT || errno == ESTALE)) {
			continue;
		}
		if (fd >= 0) {
			printf("open_by_handle(%d) opened an unlinked file!\n", i);
			close(fd);
		} else {
			printf("open_by_handle(%d) returned %d incorrectly on %s file!\n", i, errno,
					nlink ? "a linked" : "an unlinked");
		}
		failed++;
	}
	if (failed)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
