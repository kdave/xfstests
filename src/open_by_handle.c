// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2010 Red Hat, Inc. All Rights reserved.
 * Copyright (C) 2017 CTERA Networks. All Rights Reserved.
 * Author: Amir Goldstein <amir73il@gmail.com>
 *
 * Attempt to create a file handle and open it with open_by_handle_at() syscall
 *
 * from:
 *  stale_handle.c
 */

/*

usage: open_by_handle [-cludmrwapknhs] [<-i|-o> <handles_file>] <test_dir> [num_files]

Examples:

1. Create test set of of test_dir with N files and try to get their NFS handles:

   open_by_handle -cp <test_dir> [N]

   This is used by new helper _require_exportfs() to check
   if filesystem supports exportfs

2. Get file handles for existing test set, drop caches and try to
   open all files by handle:

   open_by_handle -p <test_dir> [N]

3. Get file handles for existing test set and write them to a file:

   open_by_handle -p -o <handles_file> <test_dir> [N]

4. Read file handles from file and open files by handle without
   dropping caches beforehand. Sleep afterhand to keep files open:

   open_by_handle -nps -i <handles_file> <test_dir> [N]

5. Get file handles for existing test set, write data to files,
   drop caches, open all files by handle, read and verify written
   data, write new data to file:

   open_by_handle -rwa <test_dir> [N]

6. Get file handles for existing test set, unlink all test files,
   remove test_dir, drop caches, try to open all files by handle
   and expect ESTALE:

   open_by_handle -dp <test_dir> [N]

7. Get file handles for existing test set, keep open file handles for all
   test files, unlink all test files, drop caches and try to open all files
   by handle (should work):

   open_by_handle -dk <test_dir> [N]

8. Get file handles for existing test set, rename all test files,
   drop caches, try to open all files by handle (should work):

   open_by_handle -m <test_dir> [N]

9. Get file handles for existing test set, hardlink all test files,
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
#include <stdint.h>
#include <stdbool.h>

#include <sys/stat.h>
#include "statx.h"

#ifndef AT_HANDLE_MNT_ID_UNIQUE
#	define AT_HANDLE_MNT_ID_UNIQUE 0x001
#endif

#define MAXFILES 1024

struct handle {
	struct file_handle fh;
	unsigned char fid[MAX_HANDLE_SZ];
} handle[MAXFILES], dir_handle;

void usage(void)
{
	fprintf(stderr, "usage: open_by_handle [-cludmrwapknhs] [<-i|-o> <handles_file>] <test_dir> [num_files]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "open_by_handle -c <test_dir> [N] - create N test files under test_dir, try to get file handles and exit\n");
	fprintf(stderr, "open_by_handle    <test_dir> [N] - get file handles of test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -n <test_dir> [N] - get file handles of test files and try to open by handle without drop caches\n");
	fprintf(stderr, "open_by_handle -k <test_dir> [N] - get file handles of files that are kept open, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -w <test_dir> [N] - write data to test files before open by handle\n");
	fprintf(stderr, "open_by_handle -r <test_dir> [N] - read data from test files after open by handle and verify written data\n");
	fprintf(stderr, "open_by_handle -a <test_dir> [N] - write data to test files after open by handle\n");
	fprintf(stderr, "open_by_handle -l <test_dir> [N] - create hardlinks to test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -u <test_dir> [N] - unlink (hardlinked) test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -d <test_dir> [N] - unlink test files and hardlinks, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -m <test_dir> [N] - rename test files, drop caches and try to open by handle\n");
	fprintf(stderr, "open_by_handle -p <test_dir>     - create/delete and try to open by handle also test_dir itself\n");
	fprintf(stderr, "open_by_handle -i <handles_file> <test_dir> [N] - read test files handles from file and try to open by handle\n");
	fprintf(stderr, "open_by_handle -o <handles_file> <test_dir> [N] - get file handles of test files and write handles to file\n");
	fprintf(stderr, "open_by_handle -s <test_dir> [N] - wait in sleep loop after opening files by handle to keep them open\n");
	fprintf(stderr, "open_by_handle -z <test_dir> [N] - query filesystem required buffer size\n");
	exit(EXIT_FAILURE);
}

static int do_name_to_handle_at(const char *fname, struct file_handle *fh,
				int bufsz)
{
	int ret;
	int mntid_short;

	static bool skip_mntid, skip_mntid_unique;

	uint64_t statx_mntid_short = 0, statx_mntid_unique = 0;
	struct statx statxbuf;

	/* Get both the short and unique mount id. */
	if (!skip_mntid) {
		if (xfstests_statx(AT_FDCWD, fname, 0, STATX_MNT_ID, &statxbuf) < 0) {
			fprintf(stderr, "%s: statx(STATX_MNT_ID): %m\n", fname);
			return EXIT_FAILURE;
		}
		if (!(statxbuf.stx_mask & STATX_MNT_ID))
			skip_mntid = true;
		else
			statx_mntid_short = statxbuf.stx_mnt_id;
	}

	if (!skip_mntid_unique) {
		if (xfstests_statx(AT_FDCWD, fname, 0, STATX_MNT_ID_UNIQUE, &statxbuf) < 0) {
			fprintf(stderr, "%s: statx(STATX_MNT_ID_UNIQUE): %m\n", fname);
			return EXIT_FAILURE;
		}
		/*
		 * STATX_MNT_ID_UNIQUE was added fairly recently in Linux 6.8, so if the
		 * kernel doesn't give us a unique mount ID just skip it.
		 */
		if (!(statxbuf.stx_mask & STATX_MNT_ID_UNIQUE))
			skip_mntid_unique = true;
		else
			statx_mntid_unique = statxbuf.stx_mnt_id;
	}

	fh->handle_bytes = bufsz;
	ret = name_to_handle_at(AT_FDCWD, fname, fh, &mntid_short, 0);
	if (bufsz < fh->handle_bytes) {
		/* Query the filesystem required bufsz and the file handle */
		if (ret != -1 || errno != EOVERFLOW) {
			fprintf(stderr, "%s: unexpected result from name_to_handle_at: %d (%m)\n", fname, ret);
			return EXIT_FAILURE;
		}
		ret = name_to_handle_at(AT_FDCWD, fname, fh, &mntid_short, 0);
	}
	if (ret < 0) {
		fprintf(stderr, "%s: name_to_handle: %m\n", fname);
		return EXIT_FAILURE;
	}

	if (!skip_mntid) {
		if (mntid_short != (int) statx_mntid_short) {
			fprintf(stderr, "%s: name_to_handle_at returned a different mount ID to STATX_MNT_ID: %u != %lu\n", fname, mntid_short, statx_mntid_short);
			return EXIT_FAILURE;
		}
	}

	if (!skip_mntid_unique) {
		struct handle dummy_fh;
		uint64_t mntid_unique = 0;

		/*
		 * Get the unique mount ID. We don't need to get another copy of the
		 * handle so store it in a dummy struct.
		 */
		dummy_fh.fh.handle_bytes = fh->handle_bytes;
		ret = name_to_handle_at(AT_FDCWD, fname, &dummy_fh.fh, (int *) &mntid_unique, AT_HANDLE_MNT_ID_UNIQUE);
		if (ret < 0) {
			if (errno != EINVAL) {
				fprintf(stderr, "%s: name_to_handle_at(AT_HANDLE_MNT_ID_UNIQUE): %m\n", fname);
				return EXIT_FAILURE;
			}
			/* EINVAL means AT_HANDLE_MNT_ID_UNIQUE is not supported */
			skip_mntid_unique = true;
		} else {
			if (mntid_unique != statx_mntid_unique) {
				fprintf(stderr, "%s: name_to_handle_at(AT_HANDLE_MNT_ID_UNIQUE) returned a different mount ID to STATX_MNT_ID_UNIQUE: %lu != %lu\n", fname, mntid_unique, statx_mntid_unique);
				return EXIT_FAILURE;
			}
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int	i, c;
	int	fd;
	int	ret = 0;
	int	failed = 0;
	char	mname[PATH_MAX];
	char	dname[PATH_MAX];
	char	fname[PATH_MAX];
	char	fname2[PATH_MAX];
	char	*test_dir;
	char	*mount_dir;
	int	mount_fd;
	char	*infile = NULL, *outfile = NULL;
	int	in_fd = 0, out_fd = 0;
	int	numfiles = 1;
	int	create = 0, delete = 0, nlink = 1, move = 0;
	int	rd = 0, wr = 0, wrafter = 0, parent = 0;
	int	keepopen = 0, drop_caches = 1, sleep_loop = 0;
	int	bufsz = MAX_HANDLE_SZ;

	if (argc < 2)
		usage();

	while ((c = getopt(argc, argv, "cludmrwapknhi:o:sz")) != -1) {
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
		case 'n':
			drop_caches = 0;
			break;
		case 'i':
			infile = optarg;
			in_fd = open(infile, O_RDONLY);
			if (in_fd < 0) {
				perror(infile);
				return EXIT_FAILURE;
			}
			break;
		case 'o':
			outfile = optarg;
			out_fd = creat(outfile, 0644);
			if (out_fd < 0) {
				perror(outfile);
				return EXIT_FAILURE;
			}
			break;
		case 's':
			sleep_loop = 1;
			break;
		case 'z':
			bufsz = 0;
			break;
		default:
			fprintf(stderr, "illegal option '%s'\n", argv[optind]);
		case 'h':
			usage();
		}
	}
        if (optind == argc)
            usage();
	test_dir = argv[optind++];
	if (optind < argc)
		numfiles = atoi(argv[optind]);
	if (!numfiles || numfiles > MAXFILES) {
		fprintf(stderr, "illegal value '%s' for num_files\n", argv[optind]);
		usage();
	}

	/*
	 * The way we determine the mount_dir to be used for mount_fd argument
	 * for open_by_handle_at() depends on other command line arguments:
	 *
	 * -p flag usually (see -i below) implies that test_dir is NOT a mount
	 *    point, but a directory inside a mount point that we will create
	 *    and/or encode/decode during the test, so we use test_dir's parent
	 *    for mount_fd. Even when not creatig test_dir, if we would use
	 *    test_dir as mount_fd, then drop_caches will not drop the test_dir
	 *    dcache entry.
	 *
	 * If -p is not specified, we don't have a hint whether test_dir is a
	 *    mount point or not, so we assume the worst case, that it is a
	 *    mount point and therefore, we cannnot use parent as mount_fd,
	 *    because parent may be on a differnt file system.
	 *
	 * -i flag, even with -p flag, implies that test_dir IS a mount point,
	 *    because we are testing open by handle of dir, which may have been
	 *    deleted or renamed and we are not creating nor encoding the
	 *    directory file handle. -i flag is meant to be used for tests
	 *    after encoding file handles and mount cycle the file system. If
	 *    we would require the test to pass in with -ip the test_dir we
	 *    want to decode and not the mount point, that would have populated
	 *    the dentry cache and the use of -ip flag combination would not
	 *    allow testing decode of dir file handle in cold dcache scenario.
	 */
	strcpy(dname, test_dir);
	if (parent && !in_fd) {
		strcpy(mname, test_dir);
		mount_dir = dirname(mname);
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

	/*
	 * encode the file handles or read them from file (-i) and maybe store
	 * them to a file (-o).
	 */
	for (i=0; i < numfiles; i++) {
		sprintf(fname, "%s/file%06d", test_dir, i);
		if (in_fd) {
			ret = read(in_fd, (char *)&handle[i], sizeof(*handle));
			if (ret < sizeof(*handle)) {
				fprintf(stderr, "failed reading file handle #%d from '%s'\n", i, infile);
				return EXIT_FAILURE;
			}
		} else {
			ret = do_name_to_handle_at(fname, &handle[i].fh, bufsz);
			if (ret)
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
		if (out_fd) {
			ret = write(out_fd, (char *)&handle[i], sizeof(*handle));
			if (ret < sizeof(*handle)) {
				fprintf(stderr, "failed writing file handle #%d to '%s'\n", i, outfile);
				return EXIT_FAILURE;
			}
		}
	}

	if (parent) {
		if (in_fd) {
			ret = read(in_fd, (char *)&dir_handle, sizeof(*handle));
			if (ret < sizeof(*handle)) {
				fprintf(stderr, "failed reading dir file handle from '%s'\n", infile);
				return EXIT_FAILURE;
			}
		} else {
			ret = do_name_to_handle_at(test_dir, &dir_handle.fh, bufsz);
			if (ret)
				return EXIT_FAILURE;
		}
		if (out_fd) {
			ret = write(out_fd, (char *)&dir_handle, sizeof(*handle));
			if (ret < sizeof(*handle)) {
				fprintf(stderr, "failed writing dir file handle to '%s'\n", outfile);
				return EXIT_FAILURE;
			}
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

	/* If creating test set or saving files handles, we are done */
	if (create || out_fd)
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
	if (drop_caches) {
		ret = system("echo 3 > /proc/sys/vm/drop_caches");
		if (ret < 0) {
			perror("drop_caches");
			return EXIT_FAILURE;
		}
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
			if (!sleep_loop)
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
			if (!sleep_loop)
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

	/*
	 * Sleep keeping files open by handle - the program need to be killed
	 * to release the open files.
	 */
	while (sleep_loop)
		sleep(1);

	return EXIT_SUCCESS;
}
