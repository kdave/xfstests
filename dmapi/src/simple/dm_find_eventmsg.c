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
	dm_sessid_t sid = 0;
	dm_token_t token = 0;
	dm_eventmsg_t msg;
	size_t rlen;
	int buflen = sizeof(dm_eventmsg_t) + 100;
	char *versionstr;

	while( (c = getopt(argc, argv, "hs:t:l:q")) != -1 ) {
		switch(c){
		case 's':
			sid = atoi( optarg );
			break;
		case 't':
			token = atoi( optarg );
			break;
		case 'l':
			buflen = atoi( optarg );
			break;
		case 'q':
			printf("dm_eventmsg_t=%zd\n", sizeof(dm_eventmsg_t) );
			exit(0);
		case 'h':
			fprintf(stderr, "Usage: %s <-s sid> <-t token> [-l buflen]\n", argv[0]);
			fprintf(stderr, "       %s -q\n", argv[0]);
			exit(2);
		}
	}

	if( sid == 0 ){
		fprintf(stderr, "%s: must specify -s\n", argv[0] );
		exit(1);
	}

	if( token == 0 ){
		fprintf(stderr, "%s: must specify -t\n", argv[0] );
		exit(1);
	}

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_find_eventmsg( sid, token, buflen, &msg, &rlen );
	printf( "ret=%d\n", ret );
	printf( "rlen=%zd\n", rlen );
	exit(0);
}
