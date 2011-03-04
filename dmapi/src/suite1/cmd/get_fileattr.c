/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.
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

#include <lib/hsm.h>

#include <string.h>
#include <getopt.h>

extern	int	optind;
extern	int	opterr;
extern	char	*optarg;

char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-a|-A] [-s sid] [-t token] {pathname|handle}\n",
		Progname);
	exit(1);
}


int
main(
	int	argc,
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	dm_token_t	token = DM_NO_TOKEN;
#if 0
	char		buffer[500];
#endif
	void		*hanp;
	size_t		hlen;
	dm_stat_t	dmstat;
	char		*pathname_obj;
	int		a_flag = 0;
	char		*name;
	int		opt;
	int		validate = 0;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	opterr = 0;
	while ((opt = getopt(argc, argv, "Aas:t:v")) != EOF) {
		switch (opt) {
		case 'A':
			a_flag = 2;
			break;
		case 'a':
			a_flag = 1;
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 't':
			token = atol(optarg);
			break;
		case 'v':
			validate = 1;
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc) {
		usage();
	}
	pathname_obj = argv[optind];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file's handle or convert the external handle. */

	if (opaque_to_handle(pathname_obj, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n", pathname_obj);
		exit(1);
	}

	if (!a_flag) {
		fprintf(stdout, "path/handle            %s\n", pathname_obj);

		/* Get the file's state, print it, then verify it against
		   what is in the file's stat block.
		*/

		if (dm_get_fileattr(sid, hanp, hlen, token,
	DM_AT_EMASK|DM_AT_PMANR|DM_AT_PATTR|DM_AT_DTIME|DM_AT_CFLAG|DM_AT_STAT,
		    &dmstat)) {
			fprintf(stderr, "dm_get_fileattr failed, %s\n",
				strerror(errno));
			exit(1);
		}

		print_state(&dmstat);
		if(validate)
			(void)validate_state(&dmstat, pathname_obj, 1);
#if 0
	} else {
		if ((rc = filesys_bulkscan_init(pathname, &scanp)) != 0) {
			fprintf(stderr, "filesys_bulkscan failed, %s\n",
				fileio_err_image(rc));
			exit(1);
		}
		for (;;) {
			rc = filesys_bulkscan_read(scanp, &fhandle, &fullstat);
			if (rc != FILEIO_NOERROR)
				break;

			(void)fhandle_to_buffer(&fhandle, buffer, sizeof(buffer));
			if (a_flag == 1) {
				fprintf(stdout, "handle          %s\n", buffer);
				print_state(&fullstat);
				fprintf(stdout, "--------------------------\n");
			} else {
				fprintf(stdout, "%s|", buffer);
				print_line(&fullstat);
			}
		}

		if (rc != FILEIO_ENDOFSCAN) {
			fprintf(stderr, "filesys_bulkscan_read failed, %s\n",
				fileio_err_image(rc));
			exit(1);
		}
		if ((rc = filesys_bulkscan_close(&scanp)) != 0) {
			fprintf(stderr, "filesys_bulkscan_close failed, %s\n",
				fileio_err_image(rc));
			exit(1);
		}
#endif
	}
	exit(0);
}
