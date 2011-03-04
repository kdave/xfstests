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

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include <lib/hsm.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_get_allocinfo().  The
command line is:

	get_allocinfo [-D] [-n nelem] [-o offp] [-s sid] pathname

where pathname is the name of a file, 'offp' is a byte offset from the
beginning of the file where you want to start dumping, and 'nelem' allows
you to specify how many extent structures to use in each dm_get_allocinfo
call.

The code checks the returned extents as much as possible for errors.
It detects bad ex_type values, verifies that there is always a trailing
hole at the end of the file, that the ex_offset of each extent matches the
ex_offset+ex_length of the previous extent, and that ex_offset+ex_length
is always an even multiple of 512.  It verifies that all ex_offset values
after the first fall on a 512-byte boundary.  It verifies that the '*offp'
value returned by dm_get_allocinfo() is 0 at the end of the file, and
equals ex_offset+ex_length of the last extent if not at the end of the file.
Any error is reported to stderr, and the program then terminates with a
non-zero exit status.

The program produces output similar to xfs_bmap in order to make comparison
easier.  Here is some sample output.

f1: offset 1
        rc 0, nelemp 17
        0: [0..127]: resv       [1..511]

Line 1 gives the name of the file and the byte offset within the file where
the dump started.  Line 2 appears once for each dm_get_allocinfo() call,
giving the return value (rc) and the number of extents which were returned.
Line 3 is repeated once for each extent.  The first field "0:" is the extent
number.  The second field "[0..127]:" give the starting and ending block for
the extent in 512-byte units.  The third field is either "resv" to indicate
allocated space or "hole" if the extent is a hole.  The fourth field
"[1..511]" only appears if the dump did not start with byte zero of the
first block.  In that case, the first number shows the actual byte offset
within the block (1 in this case).  The second number should always be
511 since we always dump to the end of a block.

Possible tests
--------------

Dump some holey files and compare the output of this program with xfs_bmap.

Produce a file with holes, and perform the following tests using just one
extent (-e 1).
        Dump extents from beginning of the file.
        Dump from byte 1 of the file.
        Dump from the last byte of the first extent.
        Dump from the first byte of the second extent.
        Dump from the middle of the second extent.
        Dump the first byte of the last extent.
        Dump the last byte of the last extent.
        Dump the first byte after the last extent.
        Dump starting at an offset way past the end of the file.

Produce a fragmented file with many adjacent DM_EXTENT_RES extents.
        Repeat the above tests.

Produce a GRIO file with holes.
        Repeat the above tests.

Run the following shell script.

#!/bin/ksh

#       Dump the same holey file $max times, each time using one less extent
#       structure than the previous time.  The grep and diff code
#       checks to make sure that you get the same answer each time, no matter
#       how many extents you use.  If all is okay, The only messages you will
#       see are the "trial $num" messages.

max=20          # should be bigger than the number extents in the file

num=$max
while [ $num -gt 0 ]
do
        echo "trial $num"
        ./test_alloc -e $num f1 | grep '\[' > x.$num
        if [ $num -lt $max ]
        then
                diff x.$num x.$max
        fi
        num=`expr $num - 1`
done

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;

static int print_alloc(dm_sessid_t sid, void* hanp, size_t hlen,
	char *pathname, dm_off_t startoff, u_int nelem);

char	*Progname;
int	Dflag = 0;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-D] [-n nelem] [-o off] [-s sid] "
		"pathname\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	 sid = DM_NO_SESSION;
	dm_off_t 	startoff = 0;		/* starting offset */
	u_int		nelem = 100;
	char		*pathname;
	void		*hanp;
	size_t	 	 hlen;
	dm_stat_t	sbuf;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "Dn:o:s:")) != EOF) {
		switch(opt) {
		case 'D':
			Dflag++;
			break;
		case 'n':
			nelem = atol(optarg);
			break;
		case 'o':
			startoff = atoll(optarg);
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
	pathname = argv[optind];

	if (dm_init_service(&name)) {
		fprintf(stderr, "dm_init_service failed, %s\n",
			strerror(errno));
		exit(1);
	}

	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	/* Get the file's handle and verify that it is a regular file. */

	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
		fprintf(stderr, "can't get handle for %s\n", pathname);
		exit(1);
	}
	if (dm_get_fileattr(sid, hanp, hlen, DM_NO_TOKEN, DM_AT_STAT, &sbuf)) {
		fprintf(stderr, "dm_get_fileattr failed\n");
		exit(1);
	}
	if (!S_ISREG(sbuf.dt_mode)) {
		fprintf(stderr, "%s is not a regular file\n", pathname);
		exit(1);
	}

	/* Print the allocation. */

	if (print_alloc(sid, hanp, hlen, pathname, startoff, nelem))
		exit(1);

	dm_handle_free(hanp, hlen);
	exit(0);
}


static int
print_alloc(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	char		*pathname,
	dm_off_t	startoff,
	u_int		nelem)
{
	dm_off_t	endoff;
	dm_extent_t	*extent;
	u_int		nelemp;
	u_int		num = 0;
	u_int		i;
	char		*type = NULL;
	int		rc;

	fprintf(stdout, "%s: starting offset %lld\n", pathname,
		(long long) startoff);

	/* Allocate space for the number of extents requested by the user. */

	if ((extent = malloc(nelem * sizeof(*extent))) == NULL) {
		fprintf(stderr, "can't malloc extent structures\n");
		return(1);
	}

	rc = 1;
	endoff = startoff;

	while (rc != 0) {
		rc = dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN, &startoff,
			nelem, extent, &nelemp);

		if (rc < 0) {
			fprintf(stderr, "dm_get_allocinfo failed, %s\n",
				strerror(errno));
			return(1);
		}

		fprintf(stdout, "\treturned %d, nelemp %d\n", rc, nelemp);
		if (Dflag && nelemp)
			fprintf(stdout, " ex_type    ex_offset       ex_length\n");

		/* Note: it is possible for nelemp to be zero! */

		for (i = 0; i < nelemp; i++) {
			/* The extent must either be reserved space or a hole.
			*/

			switch (extent[i].ex_type) {
			case DM_EXTENT_RES:
				type = "resv";
				break;
			case DM_EXTENT_HOLE:
				type = "hole";
				break;
			default:
				fprintf(stderr, "invalid extent type %d\n",
					extent[i].ex_type);
				return(1);
			}

			if (!Dflag) {
				fprintf(stdout, "\t%d: [%lld..%lld]: %s", num,
					(long long) extent[i].ex_offset / 512,
					(long long) (extent[i].ex_offset +
					extent[i].ex_length - 1) / 512, type);
				if ((extent[i].ex_offset % 512 != 0) ||
				    (endoff % 512 != 0)) {
					fprintf(stdout, "\t[%lld..%lld]\n",
						(long long) extent[i].ex_offset % 512,
						(long long) (endoff-1) % 512);
				} else {
					fprintf(stdout, "\n");
				}
			} else {
				fprintf(stdout, "%5s	%13lld	%13lld\n",
					type, (long long) extent[i].ex_offset,
					(long long) extent[i].ex_length);
			}

			/* The ex_offset of the first extent should match the
			   'startoff' specified by the caller.  The ex_offset
			   in subsequent extents should always match
			   (ex_offset + ex_length) of the previous extent,
			   and should always start on a 512 byte boundary.
			*/

			if (extent[i].ex_offset != endoff) {
				fprintf(stderr, "new extent (%lld)is not "
					"adjacent to previous one (%lld)\n",
					(long long) extent[i].ex_offset,
					(long long) endoff);
				return(1);
			}
			if (num && (extent[i].ex_offset % 512) != 0) {
				fprintf(stderr, "non-initial ex_offset (%lld) "
					"is not a 512-byte multiple\n",
					(long long) extent[i].ex_offset);
				return(1);
			}

			/* Non-initial extents should have ex_length values
			   that are an even multiple of 512.  The initial
			   extent should be a multiple of 512 less the offset
			   into the starting 512-byte block.
			*/

			if (((extent[i].ex_offset % 512) + extent[i].ex_length) % 512 != 0) { 
				fprintf(stderr, "ex_length is incorrect based "
					"upon the ex_offset\n");
				return(1);
			}

			endoff = extent[i].ex_offset + extent[i].ex_length;
			num++;		/* count of extents printed */
		}

		/* If not yet at end of file, the startoff parameter should
		   match the ex_offset plus ex_length of the last extent
		   retrieved.
		*/

		if (rc && startoff != endoff) {
			fprintf(stderr, "startoff is %lld, should be %lld\n",
				(long long) startoff, (long long) endoff);
			return(1);
		}

		/* If we are at end of file, the last extent should be a
		   hole.
		*/

		if (!rc && type && strcmp(type, "hole")) {
			fprintf(stderr, "file didn't end with a hole\n");
			return(1);
		}
	}

	/* At end of file, startoff should always equal zero. */

	if (startoff != 0) {
		fprintf(stderr, "ERROR: startoff was not zero at end of file\n");
		return(1);
	}
	return(0);
}
