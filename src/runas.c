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

/*
 * Run a command with a particular 
 *    - effective user id
 *    - effective group id
 *    - supplementary group list
 */
 
#include "global.h"
#include <grp.h>



char *prog;

void usage(void)
{
    fprintf(stderr, "usage: %s [-u uid] [-g gid] [-s gid] cmd\n"
           "flags:\n"
           "    -u - effective user-id\n"
           "    -g - effective group-id\n"
           "    -s - supplementary group-id\n", prog);
           
}

#define SUP_MAX 20

int
main(int argc, char **argv)
{
	int c;
        uid_t uid = -1;
        gid_t gid = -1;
        char *cmd=NULL;
        gid_t sgids[SUP_MAX];
        int sup_cnt = 0;
	int status;

        prog = basename(argv[0]);

	while ((c = getopt(argc, argv, "u:g:s:")) != -1) {
		switch (c) {
		case 'u':
			uid = atoi(optarg);
			break;
		case 'g':
			gid = atoi(optarg);
			break;
		case 's':
			if (sup_cnt+1 > SUP_MAX) {
			    fprintf(stderr, "%s: too many sup groups\n", prog);
			    exit(1);
			}
			sgids[sup_cnt++] = atoi(optarg);
			break;
		case '?':
                        usage();
			exit(1);
		}
	}

        /* build up the cmd */
	for ( ; optind < argc; optind++) {
	    cmd = realloc(cmd, (cmd==NULL?0:strlen(cmd)) + 
                               strlen(argv[optind]) + 4);
	    strcat(cmd, " ");
	    strcat(cmd, argv[optind]);
	}
 

        if (gid != -1) {
	    if (setegid(gid) == -1) {
		fprintf(stderr, "%s: setegid(%d) failed: %s\n",
			prog, gid, strerror(errno));
		exit(1);
	    }	
        }

        if (sup_cnt > 0) {
	    if (setgroups(sup_cnt, sgids) == -1) {
		fprintf(stderr, "%s: setgroups() failed: %s\n",
			prog, strerror(errno));
		exit(1);
	    }	
	}

        if (uid != -1) {
	    if (seteuid(uid) == -1) {
		fprintf(stderr, "%s: seteuid(%d) failed: %s\n",
			prog, uid, strerror(errno));
		exit(1);
	    }	
        }

	status = system(cmd);

	if (WIFSIGNALED(status)) {
	    fprintf(stderr, "%s: command terminated with signal %d\n", 
                 prog, WTERMSIG(status));
	    exit(1);
	}
	else if (WIFEXITED(status)) {
	    exit(WEXITSTATUS(status));
        }
	else {
	    fprintf(stderr, "%s: command bizarre wait status 0x%x\n", 
               prog, status);
	    exit(1);
	}

	exit(0);
	/* NOTREACHED */
}
