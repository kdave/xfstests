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
#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <getopt.h>
#ifdef linux
#include <dmapi.h>
#else
#include <sys/dmi.h>
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
	printf( "rlen=%d\n", rlen );
	if( ret != -1 )
		printf( "sessinfo=%s\n", sessinfo );
	exit(0);
}
