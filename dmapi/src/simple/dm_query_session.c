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
#include <stdlib.h>
#include <stdio.h>
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
	char *sessinfo;
	size_t rlen = 0;
	dm_sessid_t sid = 0;
	int buflen = 100;
	char *versionstr;

	while( (c = getopt(argc, argv, "hs:l:")) != -1 ) {
		switch(c){
		case 's':
			sid = atoi( optarg );
			break;
		case 'l':
			buflen = atoi( optarg );
			break;
		case 'h':
			fprintf(stderr, "Usage: %s <-s sid> [-l buflen]\n", argv[0]);
			exit(2);
		}
	}

	if( sid == 0 ){
		fprintf(stderr, "%s: must specify -s\n", argv[0] );
		exit(1);
	}

	if( (sessinfo = malloc( sizeof(char*) * buflen )) == NULL ){
		fprintf(stderr, "%s: malloc failed\n", argv[0] );
		exit(1);
	}

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_query_session( sid, buflen, sessinfo, &rlen );
	printf( "ret=%d\n", ret );
	printf( "rlen=%zd\n", rlen );
	if( ret != -1 )
		printf( "sessinfo=%s\n", sessinfo );
	exit(0);
}
