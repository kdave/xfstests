/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
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

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test all the DMAPI functions in dm_handle.c.  The
command line is:

        dm_handle pathname

where pathname is the name of a file.  If any function fails, an error message
containing the work ERROR will be written to stderr.

Tested DMAPI functions are:
	dm_fd_to_handle
	dm_handle_cmp
	dm_handle_free
	dm_handle_hash
	dm_handle_is_valid
	dm_handle_to_fshandle
	dm_handle_to_fsid
	dm_handle_to_ino
	dm_handle_to_igen
	dm_make_handle
	dm_make_fshandle
	dm_path_to_handle
	dm_path_to_fshandle

----------------------------------------------------------------------------*/


char	*Progname;



int
main(
	int		argc,
	char		**argv)
{
	char		*pathname;
	char		*name;
	void		*hanp1, *hanp2, *hanp3, *fshanp1, *fshanp2, *fshanp3;
	size_t		hlen1, hlen2, hlen3, fshlen1, fshlen2, fshlen3;
	u_int		hash1, hash2, hash3, fshash1, fshash2, fshash3;
	dm_fsid_t	fsid;
	dm_ino_t	ino;
	dm_igen_t	igen;
	char		buffer[100];
	char		buffer1[100];
	char		fsbuffer1[100];
	char		buffer2[100];
	char		fsbuffer2[100];
	char		buffer3[100];
	char		fsbuffer3[100];
	int		fd;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	if (argc != 2) {
		fprintf(stderr, "usage:	%s path\n", argv[0]);
		exit(1);
	}
	pathname = argv[1];

	(void)dm_init_service(&name);

	if (dm_path_to_handle(pathname, &hanp1, &hlen1) != 0) {
		fprintf(stderr, "dm_path_to_handle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	hash1 = dm_handle_hash(hanp1, hlen1);
	hantoa(hanp1, hlen1, buffer1);
	fprintf(stdout, "  han1:  hash %u  value %s  (dm_path_to_handle)\n",
		hash1, buffer1);
	if (dm_handle_is_valid(hanp1, hlen1) == DM_FALSE) {
		fprintf(stderr, "ERROR: han1 is not valid\n");
	}

	if (dm_path_to_fshandle(pathname, &fshanp1, &fshlen1) != 0) {
		fprintf(stderr, "dm_path_to_fshandle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	fshash1 = dm_handle_hash(fshanp1, fshlen1);
	hantoa(fshanp1, fshlen1, fsbuffer1);
	fprintf(stdout, "fshan1:  hash %u  value %s  (dm_path_to_fshandle\n",
		fshash1, fsbuffer1);
	if (dm_handle_is_valid(fshanp1, fshlen1) == DM_FALSE) {
		fprintf(stderr, "ERROR: fshan1 is not valid\n");
	}

	if ((fd = open(pathname, O_RDONLY)) < 0) {
		fprintf(stderr, "open of %s failed, %s\n", pathname,
			strerror(errno));
		exit(1);
	}
	if (dm_fd_to_handle(fd, &hanp2, &hlen2) != 0) {
		fprintf(stderr, "dm_fd_to_handle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	(void)close(fd);
	hash2 = dm_handle_hash(hanp2, hlen2);
	hantoa(hanp2, hlen2, buffer2);
	fprintf(stdout, "  han2:  hash %u  value %s  (dm_fd_to_handle)\n",
		hash2, buffer2);
	if (dm_handle_is_valid(hanp2, hlen2) == DM_FALSE) {
		fprintf(stderr, "ERROR: han2 is not valid\n");
	}

	if (dm_handle_to_fshandle(hanp2, hlen2, &fshanp2, &fshlen2) != 0) {
		fprintf(stderr, "dm_handle_to_fshandle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	fshash2 = dm_handle_hash(fshanp2, fshlen2);
	hantoa(fshanp2, fshlen2, fsbuffer2);
	fprintf(stdout, "fshan2:  hash %u  value %s  (dm_handle_to_fshandle)\n",
		fshash2, fsbuffer2);
	if (dm_handle_is_valid(fshanp2, fshlen2) == DM_FALSE) {
		fprintf(stderr, "ERROR: fshan2 is not valid\n");
	}

	if (dm_handle_cmp(hanp1, hlen1, hanp2, hlen2)) {
		fprintf(stderr, "ERROR: han1 and han2 differ in dm_handle_cmp\n");
	}
	if (strcmp(buffer1, buffer2)) {
		fprintf(stderr, "ERROR: han1 and han2 differ in strcmp\n");
	}
	if (hash1 != hash2) {
		fprintf(stderr, "ERROR: hash1 and hash2 differ\n");
	}

	if (dm_handle_cmp(fshanp1, fshlen1, fshanp2, fshlen2)) {
		fprintf(stderr, "ERROR: fshan1 and fshan2 differ in dm_handle_cmp\n");
	}
	if (strcmp(fsbuffer1, fsbuffer2)) {
		fprintf(stderr, "ERROR: fshan1 and fshan2 differ in strcmp\n");
	}
	if (fshash1 != fshash2) {
		fprintf(stderr, "ERROR: fshash1 and fshash2 differ\n");
	}

	/* Break the handle into its component parts and display them.  Use
	   hantoa() instead of printing the parts directly because some are
	   32 bits on Veritas and 64 bits on SGI.
	*/

	if (dm_handle_to_fsid(hanp1, hlen1, &fsid) != 0) {
		fprintf(stderr, "dm_handle_to_fsid failed, %s\n",
			strerror(errno));
		exit(1);
	}
	hantoa(&fsid, sizeof(fsid), buffer);
	fprintf(stdout, "fsid  %s	(dm_handle_to_fsid)\n", buffer);

	if (dm_handle_to_ino(hanp1, hlen1, &ino) != 0) {
		fprintf(stderr, "dm_handle_to_ino failed, %s\n",
			strerror(errno));
		exit(1);
	}
	hantoa(&ino, sizeof(ino), buffer);
	fprintf(stdout, "ino   %s	(dm_handle_to_ino)\n", buffer);

	if (dm_handle_to_igen(hanp1, hlen1, &igen) != 0) {
		fprintf(stderr, "dm_handle_to_igen failed, %s\n",
			strerror(errno));
		exit(1);
	}
	hantoa(&igen, sizeof(igen), buffer);
	fprintf(stdout, "igen  %s	(dm_handle_to_igen)\n", buffer);

	/* Now use the parts to remake the handle and verify we get the same
	   answer.
	*/

	if (dm_make_handle(&fsid, &ino, &igen, &hanp3, &hlen3) != 0) {
		fprintf(stderr, "dm_make_handle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	hash3 = dm_handle_hash(hanp3, hlen3);
	hantoa(hanp3, hlen3, buffer3);
	fprintf(stdout, "  han3:  hash %u  value %s  (dm_make_handle)\n",
		hash3, buffer3);
	if (dm_handle_is_valid(hanp3, hlen3) == DM_FALSE) {
		fprintf(stderr, "ERROR: han3 is not valid\n");
	}

	if (dm_handle_cmp(hanp1, hlen1, hanp3, hlen3)) {
		fprintf(stderr, "ERROR: hanp1 and hanp3 differ in dm_handle_cmp\n");
	}
	if (strcmp(buffer1, buffer3)) {
		fprintf(stderr, "ERROR: hanp1 and hanp3 differ in strcmp\n");
	}
	if (hash1 != hash3) {
		fprintf(stderr, "ERROR: hash1 and hash3 differ\n");
	}

	if (dm_make_fshandle(&fsid, &fshanp3, &fshlen3) != 0) {
		fprintf(stderr, "dm_make_fshandle failed, %s\n",
			strerror(errno));
		exit(1);
	}
	fshash3 = dm_handle_hash(fshanp3, fshlen3);
	hantoa(fshanp3, fshlen3, fsbuffer3);
	fprintf(stdout, "fshan3:  hash %u  value %s  (dm_make_fshandle)\n",
		fshash3, fsbuffer3);
	if (dm_handle_is_valid(fshanp3, fshlen3) == DM_FALSE) {
		fprintf(stderr, "ERROR: fshan3 is not valid\n");
	}

	if (dm_handle_cmp(fshanp1, fshlen1, fshanp3, fshlen3)) {
		fprintf(stderr, "ERROR: fshan1 and fshan3 differ in dm_handle_cmp\n");
	}
	if (strcmp(fsbuffer1, fsbuffer3)) {
		fprintf(stderr, "ERROR: fshan1 and fshan3 differ in strcmp\n");
	}
	if (fshash1 != fshash3) {
		fprintf(stderr, "ERROR: fshash1 and fshash3 differ\n");
	}

	dm_handle_free(hanp1, hlen1);
	dm_handle_free(hanp2, hlen2);
	dm_handle_free(hanp3, hlen3);
	dm_handle_free(fshanp1, fshlen1);
	dm_handle_free(fshanp2, fshlen2);
	dm_handle_free(fshanp3, fshlen3);
	exit(0);
}
