/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <lib/hsm.h>

#include <string.h>
#include <malloc.h>
#include <getopt.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_write_invis().  The
command line is:

   handle_write_invis [-c char] [-o offset] [-l length] [-s sid] handle

where:
'char' is the character to use as a repeated pattern ('X' is the default),
'offset' is the offset of the start of the write (0 is the default),
'length' is the length of the write in bytes (1 is the default),
'sid' is the session ID whose events you you are interested in.
'handle' is the ascii representation of the file's handle.
	You can use the test tool path_to_handle to get this.

DM_WRITE_SYNC is is not supported.

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-c char] [-o offset] [-l length] "
		"[-s sid] handle\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*han_str = NULL;
	dm_off_t	offset = 0;
	dm_size_t	length = 1;
	u_char		ch = 'X';
	void		*bufp = NULL;
	void		*hanp;
	size_t		hlen;
	dm_ssize_t	rc;
	char		*name;
	int		opt;
	int		error;

	if (Progname = strrchr(argv[0], '/')) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "c:o:l:s:")) != EOF) {
		switch (opt) {
		case 'c':
			ch = *optarg;
			break;
		case 'o':
			offset = atol(optarg);
			break;
		case 'l':
			length = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	han_str = argv[optind];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file's handle. */

	if ((error = atohan(han_str, &hanp, &hlen)) != 0) {
		fprintf(stderr, "atohan() failed, %s\n", strerror(error));
		return(1);
	}

	if (length > 0) {
		/* In case it is a realtime file, align the buffer on a
		   sufficiently big boundary.
		*/
		if ((bufp = memalign(4096, length)) == NULL) {
			fprintf(stderr, "malloc of %llu bytes failed\n", length);
			exit(1);
		}
		memset(bufp, ch, length);
	}

	rc = dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN, 0, offset, length, bufp);

	if (rc < 0) {
		fprintf(stderr, "dm_write_invis failed, %s\n", strerror(errno));
		exit(1);
	} else if (rc != length) {
		fprintf(stderr, "expected to write %lld bytes, actually "
			"wrote %lld\n", length, rc);
		exit(1);
	}
	dm_handle_free(hanp, hlen);
	exit(0);
}
