/*
 * Copyright (c) 2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/attributes.h>
#include <sys/fs/xfs_itable.h>

#ifndef ATTR_PARENT
#define ATTR_PARENT 0x0040
#endif

#define EA_LISTBUF_SZ 16384

int
main(int argc, char *argv[])
{
	char *path;
	int fd;
	char *prog = argv[0];
	void *handle;
	size_t hlen;
	attrlist_cursor_t cursor;
	attrlist_t *ea_listbuf = (attrlist_t *)malloc(EA_LISTBUF_SZ);
	uint64_t parent_ino;
	uint64_t link_cnt;
	int sts;
	int nameix;
	attrlist_ent_t *entp;

	if (argc < 2) {
		fprintf(stderr, "%s: missing pathname argument\n", prog);
		return 1;
	}
	path = argv[1];

	if (argc > 2) {
		fprintf(stderr, "%s: too many arguments\n", prog);
		return 1;
	}

	/* if file already exists then error out */
	if (access(path, F_OK) == 0) {
		fprintf(stderr, "%s: file \"%s\" already exists\n", prog, path);
		return 1;
	}

	fd = open(path, O_RDWR|O_CREAT|O_EXCL);
	if (fd == -1) {
		fprintf(stderr, "%s: failed to create \"%s\": %s\n", prog, path, strerror(errno));
		return 1;
	}


	/* for linux libhandle version - to set libhandle fsfd cache */
	{
		void *fshandle;
		size_t fshlen;

		if (path_to_fshandle(path, &fshandle, &fshlen) != 0) {
			fprintf(stderr, "%s: failed path_to_fshandle \"%s\": %s\n",
				prog, path, strerror(errno));
			return 1;
		}
	}


	/* 
	 * look at parentptr EAs and see if the path exists now that
	 * it has been unlinked.
	 */ 
	if (fd_to_handle(fd, &handle, &hlen) != 0) {
		fprintf(stderr, "%s: failed to fd_to_handle \"%s\": %s\n",
			prog, path, strerror(errno));
		return 1;
	}

	if (unlink(path) == -1) {
		fprintf(stderr, "%s: failed to unlink \"%s\": %s\n", prog, path, strerror(errno));
		return 1;
	}

	memset(&cursor, 0, sizeof(cursor));

	/* just do one call - don't bother with continue logic */
	sts = attr_list_by_handle(handle,
			hlen,
			(char*)ea_listbuf,
			EA_LISTBUF_SZ,
			ATTR_PARENT,
			&cursor);
	if (sts != 0) {
		fprintf(stderr, "%s: failed to list attr for \"%s\": %s\n", prog, path, strerror(errno));
		return 1;
	}

	printf("ea count = %d\n", ea_listbuf->al_count);

	/*
	 * process EA list
	 */
	for (nameix = 0; nameix < ea_listbuf->al_count; nameix++) {
		entp = ATTR_ENTRY(ea_listbuf, nameix);

		sts = sscanf(entp->a_name, "%llx %llx", &parent_ino, &link_cnt);
		if (sts != 2) {
			fprintf(stderr,
				"inode-path for \"%s\" is corrupted\n",
				path);

			/* go onto next EA name */
			continue;
		}
		/* just print the info out */
		printf("[%d] parent_ino = %llu, link_cnt = %llu\n", nameix, parent_ino, link_cnt);
	}


	free_handle(handle, hlen);

	if (close(fd) == -1) {
		fprintf(stderr, "%s: failed to close \"%s\": %s\n", prog, path, strerror(errno));
		return 1;
	}

	return 0;
}
