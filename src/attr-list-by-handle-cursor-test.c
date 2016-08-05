/*
 * Copyright (C) 2016 Oracle.  All Rights Reserved.
 *
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <attr/attributes.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <asm/types.h>
#include <xfs/xfs.h>
#include <xfs/handle.h>

#define ATTRBUFSZ		1024
#define BSTATBUF_NR		32

/* Read all the extended attributes of a file handle. */
void
read_handle_xattrs(
	struct xfs_handle	*handle)
{
	struct attrlist_cursor	cur;
	char			attrbuf[ATTRBUFSZ];
	char			*firstname = NULL;
	struct attrlist		*attrlist = (struct attrlist *)attrbuf;
	struct attrlist_ent	*ent;
	int			i;
	int			flags = 0;
	int			error;

	memset(&cur, 0, sizeof(cur));
	while ((error = attr_list_by_handle(handle, sizeof(*handle),
					    attrbuf, ATTRBUFSZ, flags,
					    &cur)) == 0) {
		for (i = 0; i < attrlist->al_count; i++) {
			ent = ATTR_ENTRY(attrlist, i);

			if (i != 0)
				continue;

			if (firstname == NULL) {
				firstname = malloc(ent->a_valuelen);
				memcpy(firstname, ent->a_name, ent->a_valuelen);
			} else {
				if (memcmp(firstname, ent->a_name,
					   ent->a_valuelen) == 0)
					fprintf(stderr,
						"Saw duplicate xattr \"%s\", buggy XFS?\n",
						ent->a_name);
				else
					fprintf(stderr,
						"Test passes.\n");
				goto out;
			}
		}

		if (!attrlist->al_more)
			break;
	}

out:
	if (firstname)
		free(firstname);
	if (error)
		perror("attr_list_by_handle");
	return;
}

int main(
	int			argc,
	char			*argv[])
{
	struct xfs_handle	*fshandle;
	size_t			fshandle_len;
	struct xfs_handle	*handle;
	size_t			handle_len;
	int			error;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		return 1;
	}

	error = path_to_fshandle(argv[1], (void **)&fshandle, &fshandle_len);
	if (error) {
		perror("getting fshandle");
		return 2;
	}

	error = path_to_handle(argv[1], (void **)&handle, &handle_len);
	if (error) {
		perror("getting handle");
		return 3;
	}

	read_handle_xattrs(handle);

	free_handle(handle, handle_len);
	free_handle(fshandle, fshandle_len);
	return 0;
}
