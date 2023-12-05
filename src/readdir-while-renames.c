// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 SUSE Linux Products GmbH.  All Rights Reserved.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

/* Number of files we add to the test directory. */
#define NUM_FILES 5000

int main(int argc, char *argv[])
{
	struct dirent *entry;
	DIR *dir = NULL;
	char *dir_path = NULL;
	int dentry_count = 0;
	int ret = 0;
	int i;

	if (argc != 2) {
		fprintf(stderr, "Usage:  %s <directory>\n", argv[0]);
		ret = 1;
		goto out;
	}

	dir_path = malloc(strlen(argv[1]) + strlen("/testdir") + 1);
	if (!dir_path) {
		fprintf(stderr, "malloc failure\n");
		ret = ENOMEM;
		goto out;
	}
	i = strlen(argv[1]);
	memcpy(dir_path, argv[1], i);
	memcpy(dir_path + i, "/testdir", strlen("/testdir"));
	dir_path[i + strlen("/testdir")] = '\0';

	ret = mkdir(dir_path, 0700);
	if (ret == -1) {
		fprintf(stderr, "Failed to create test directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	ret = chdir(dir_path);
	if (ret == -1) {
		fprintf(stderr, "Failed to chdir to the test directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	/* Now create all files inside the directory. */
	for (i = 1; i <= NUM_FILES; i++) {
		char file_name[32];
		FILE *f;

		sprintf(file_name, "%s/%d", dir_path, i);
		f = fopen(file_name, "w");
		if (f == NULL) {
			fprintf(stderr, "Failed to create file number %d: %d\n",
				i, errno);
			ret = errno;
			goto out;
		}
		fclose(f);
	}

	dir = opendir(dir_path);
	if (dir == NULL) {
		fprintf(stderr, "Failed to open directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	/*
	 * readdir(3) returns NULL when it reaches the end of the directory or
	 * when an error happens, so reset errno to 0 to distinguish between
	 * both cases later.
	 */
	errno = 0;
	while ((entry = readdir(dir)) != NULL) {
		dentry_count++;
		/*
		 * The actual number of dentries returned varies per filesystem
		 * implementation. On a 6.7-rc4 kernel, on x86_64 with default
		 * mkfs options, xfs returns 5031 dentries while ext4, f2fs and
		 * btrfs return 5002 (the 5000 files plus "." and ".."). These
		 * variations are fine and valid according to POSIX, as some of
		 * the renames may be visible or not while calling readdir(3).
		 * We only want to check we don't enter into an infinite loop,
		 * so let the maximum number of dentries be 3 * NUM_FILES, which
		 * is very reasonable.
		 */
		if (dentry_count > 3 * NUM_FILES) {
			fprintf(stderr,
				"Found too many directory entries (%d)\n",
				dentry_count);
			ret = 1;
			goto out;
		}
		/* Can't rename "." and "..", skip them. */
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;
		ret = rename(entry->d_name, "TEMPFILE");
		if (ret == -1) {
			fprintf(stderr,
				"Failed to rename '%s' to TEMPFILE: %d\n",
				entry->d_name, errno);
			ret = errno;
			goto out;
		}
		ret = rename("TEMPFILE", entry->d_name);
		if (ret == -1) {
			fprintf(stderr,
				"Failed to rename TEMPFILE to '%s': %d\n",
				entry->d_name, errno);
			ret = errno;
			goto out;
		}
	}

	if (errno) {
		fprintf(stderr, "Failed to read directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	/* It should return at least NUM_FILES entries +2 (for "." and ".."). */
	if (dentry_count < NUM_FILES + 2) {
		fprintf(stderr,
			"Found less directory entries than expected (%d but expected %d)\n",
			dentry_count, NUM_FILES + 2);
		ret = 2;
	}
out:
	free(dir_path);
	if (dir != NULL)
		closedir(dir);

	return ret;
}
