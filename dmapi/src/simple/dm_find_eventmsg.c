// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
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
