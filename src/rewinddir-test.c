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

/*
 * Number of files we add to the test directory after calling opendir(3) and
 * before calling rewinddir(3).
 */
#define NUM_FILES 10000

int main(int argc, char *argv[])
{
	int file_counters[NUM_FILES] = { 0 };
	int dot_count = 0;
	int dot_dot_count = 0;
	struct dirent *entry;
	DIR *dir = NULL;
	char *dir_path = NULL;
	char *file_path = NULL;
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

	/* More than enough to contain any full file path. */
	file_path = malloc(strlen(dir_path) + 12);
	if (!file_path) {
		fprintf(stderr, "malloc failure\n");
		ret = ENOMEM;
		goto out;
	}

	ret = mkdir(dir_path, 0700);
	if (ret == -1) {
		fprintf(stderr, "Failed to create test directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	/* Open the directory first. */
	dir = opendir(dir_path);
	if (dir == NULL) {
		fprintf(stderr, "Failed to open directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	/*
	 * Now create all files inside the directory.
	 * File names go from 1 to NUM_FILES, 0 is unused as it's the return
	 * value for atoi(3) when an error happens.
	 */
	for (i = 1; i <= NUM_FILES; i++) {
		FILE *f;

		sprintf(file_path, "%s/%d", dir_path, i);
		f = fopen(file_path, "w");
		if (f == NULL) {
			fprintf(stderr, "Failed to create file number %d: %d\n",
				i, errno);
			ret = errno;
			goto out;
		}
		fclose(f);
	}

	/*
	 * Rewind the directory and read it.
	 * POSIX requires that after a rewind, any new names added to the
	 * directory after the openddir(3) call and before the rewinddir(3)
	 * call, must be returned by readdir(3) calls
	 */
	rewinddir(dir);

	/*
	 * readdir(3) returns NULL when it reaches the end of the directory or
	 * when an error happens, so reset errno to 0 to distinguish between
	 * both cases later.
	 */
	errno = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0) {
			dot_count++;
			continue;
		}
		if (strcmp(entry->d_name, "..") == 0) {
			dot_dot_count++;
			continue;
		}
		i = atoi(entry->d_name);
		if (i == 0) {
			fprintf(stderr,
				"Failed to parse name '%s' to integer: %d\n",
				entry->d_name, errno);
			ret = errno;
			goto out;
		}
		/* File names go from 1 to NUM_FILES, so subtract 1. */
		file_counters[i - 1]++;
	}

	if (errno) {
		fprintf(stderr, "Failed to read directory: %d\n", errno);
		ret = errno;
		goto out;
	}

	/*
	 * Now check that the readdir() calls return every single file name
	 * and without repeating any of them. If any name is missing or
	 * repeated, don't exit immediatelly, so that we print a message for
	 * all missing or repeated names.
	 */
	for (i = 0; i < NUM_FILES; i++) {
		if (file_counters[i] != 1) {
			fprintf(stderr, "File name %d appeared %d times\n",
				i + 1,  file_counters[i]);
			ret = EINVAL;
		}
	}
	if (dot_count != 1) {
		fprintf(stderr, "File name . appeared %d times\n", dot_count);
		ret = EINVAL;
	}
	if (dot_dot_count != 1) {
		fprintf(stderr, "File name .. appeared %d times\n", dot_dot_count);
		ret = EINVAL;
	}
out:
	free(dir_path);
	free(file_path);
	if (dir != NULL)
		closedir(dir);

	return ret;
}
