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
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <getopt.h>
#ifdef linux
#include <dmapi.h>
#endif

int
main( int argc, char **argv )
{
	extern char *optarg;
	int c;
	int ret;
	dm_sessid_t oldsid = DM_NO_SESSION;
	dm_sessid_t newsid = 0;
	char *sessinfo = "test1";
	char *versionstr;

	while( (c = getopt(argc, argv, "hs:i:")) != -1 ) {
		switch(c){
		case 's':
			oldsid = atoi( optarg );
			break;
		case 'i':
			sessinfo = optarg;
			break;
		case 'h':
			fprintf(stderr, "Usage: %s [-s oldsid] [-i sessinfo_txt]\n", argv[0]);
			exit(2);
		}
	}

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_create_session( oldsid, sessinfo, &newsid);
	printf( "ret=%d\n", ret );
	printf( "newsid=%d\n", newsid );
	exit(0);
}

