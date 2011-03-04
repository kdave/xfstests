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

#include <limits.h>

#include <lib/hsm.h>
#include <lib/errtest.h>

#include <getopt.h>
#include <string.h>


/*---------------------------------------------------------------------------

For manually testing DMAPI functions dm_write_invis() and dm_read_invis()

The command line is:

	invis_test [-Rrv] [-l len] [-o offset] [-s sid] ls_path pathname

where:
   -R
	reuse existing test file
   -r
	use dm_invis_read, default is dm_invis_write.
   len
	length of read/write
   offset
	offset in file for read/write
   sid
      is the session ID whose events you you are interested in.
   ls_path
      is the path to a specific copy of ls, important only for its size
   pathname
      is the filesystem to use for the test.

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
	fprintf(stderr, "usage:\t%s [-Rrv] [-l len] [-o offset] [-s sid] ls_path pathname\n", 
		Progname);
	exit(1);
}

#define BUFSZ 100

int
main(
	int	argc, 
	char	**argv)
{
	int             Vflag=0;
	dm_sessid_t	sid = DM_NO_SESSION;
	char		*dir_name = NULL;
	char		*ls_path = NULL;
	char		*name;
	char		ch = 'A';
	char            test_file[128];
	char            command[1024];
	void		*hanp;
	size_t		hlen;
	char		buf[BUFSZ];
	dm_ssize_t	rc;
	dm_off_t	offset = 0;
	dm_size_t	length = BUFSZ;
	int		opt;
	int		reading = 0; /* writing is the default */
	int		exitstat=0;
	dm_size_t	errblockstart, errblockend;
	int		in_err_block;
	int		i;
	int		reuse_file = 0;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "Rvs:rl:o:")) != EOF) {
		switch (opt) {
		case 'v':
			Vflag++;
			break;
		case 'r':
			reading++;
			break;
		case 'R':
			reuse_file++;
			break;
		case 'l':
			length = atoi(optarg);
			break;
		case 'o':
			offset = atoi(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 2 != argc)
		usage();
	ls_path = argv[optind];
	dir_name = argv[optind+1];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);
	
	sprintf(test_file, "%s/DMAPI_test_file", dir_name);
	if( (!reading) && (!reuse_file) ){
		sprintf(command, "cp %s %s\n", ls_path, test_file); 
		system(command);
	}

	if (dm_path_to_handle(test_file, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s; bypassing test\n",
		      test_file);
		exit(1);
	}

	if( Vflag )
		printf("using length = %llu\n", (unsigned long long) length);
	if( length > BUFSZ ){
		fprintf(stderr, "length(%llu) > BUFSZ(%d)\n",
			(unsigned long long) length, BUFSZ);
		exit(1);
	}

	if( reading ){
		memset(buf, '\0', BUFSZ);

		rc = dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN,
				   offset, length, buf);
		if( rc < 0 ){
			fprintf(stderr, "dm_read_invis failed, (err=%d)\n", errno);
			dm_handle_free(hanp, hlen);
			exit(1);
		}
		if( rc != length ){
			fprintf(stderr, "dm_read_invis read %llu bytes, "
				"wanted to write %lld bytes\n",
				(long long) rc, (unsigned long long) length);
			dm_handle_free(hanp, hlen);
			exitstat++;
		}
		else {
			printf("dm_read_invis read %lld bytes\n",
				(long long) rc);
		}
		
		in_err_block = 0;
		errblockstart = errblockend = 0;
		for( i=0; i < length; ++i ){
			if( in_err_block ){
				if( buf[i] != ch ){
					/* still in the err block */
					errblockend = i;
				}
				else {
					/* end of bad block */
					fprintf(stderr, "read err block: "
						"byte %lld to %lld\n",
						(long long) errblockstart,
						(long long) errblockend);
					in_err_block = 0;
				}
			}
			else if( buf[i] != ch ){
				/* enter err block */
				errblockstart = i;
				in_err_block = 1;
			}
		}
		if( in_err_block ){
			/* end of bad block */
			fprintf(stderr, "read err block: byte %lld to %lld\n",
				(long long) errblockstart,
				(long long) errblockend);
			in_err_block = 0;
		}
	}
	else {

		memset(buf, ch, BUFSZ);

		rc = dm_write_invis(sid, hanp, hlen, DM_NO_TOKEN,
				    0, offset, length, buf);
		if( rc < 0 ){
			fprintf(stderr, "dm_write_invis failed, (err=%d)\n", errno);
			dm_handle_free(hanp, hlen);
			exit(1);
		}
		if( rc != length ){
			fprintf(stderr, "dm_write_invis wrote %lld bytes, "
				"wanted to write %lld bytes\n",
				(long long) rc, (long long) length );
			dm_handle_free(hanp, hlen);
			exit(1);
		}
		printf("dm_write_invis wrote %lld bytes\n", (long long) rc);
	}

	dm_handle_free(hanp, hlen);
	exit(exitstat);
}
