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
        char **cmd;
        gid_t sgids[SUP_MAX];
        int sup_cnt = 0;
	char *p;

	prog = basename(argv[0]);
	for (p = prog; *p; p++) {
		if (*p == '/') {
			prog = p + 1;
		}
	}


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
        if (optind == argc) {
            usage();
            exit(1);
        }
	else {
	    char **p;
	    p = cmd = (char **)malloc(sizeof(char *) * (argc - optind + 1));
	    for ( ; optind < argc; optind++, p++) {
	        *p = strdup(argv[optind]);
            }
	    *p = NULL;
	} 

        if (gid != -1) {
	    if (setgid(gid) == -1) {
		fprintf(stderr, "%s: setgid(%d) failed: %s\n",
			prog, (int)gid, strerror(errno));
		exit(1);
	    }
        }

	if (gid != -1 || sup_cnt != 0) {
	    if (setgroups(sup_cnt, sgids) == -1) {
		fprintf(stderr, "%s: setgroups() failed: %s\n",
			prog, strerror(errno));
		exit(1);
	    }
	}

        if (uid != -1) {
	    if (setuid(uid) == -1) {
		fprintf(stderr, "%s: setuid(%d) failed: %s\n",
			prog, (int)uid, strerror(errno));
		exit(1);
	    }
        }

	execvp(cmd[0], cmd);
	fprintf(stderr, "%s: %s\n", cmd[0], strerror(errno));
	exit(1);
}
