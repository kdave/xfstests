/*
 *
 *   Copyright (c) International Business Machines  Corp., 2000
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 *  FILE(s)     : fs_perms.c simpletest.sh textx.o Makefile README
 *  DESCRIPTION : Regression test for Linux filesystem permissions.
 *  AUTHOR      : Jeff Martin (martinjn@us.ibm.com)
 *  HISTORY     : 
 *     (04/12/01)v.99  First attempt at using C for fs-regression test.  Only tests read and write bits.
 *     (04/19/01)v1.0  Added test for execute bit.
 *     (05/23/01)v1.1  Added command line parameter to specify test file.
 *     (07/12/01)v1.2  Removed conf file and went to command line parameters.
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <utime.h>

int testsetup(mode_t mode, int cuserId, int cgroupId);
int testfperm(int userId, int groupId, char* fperm);

int main( int argc, char *argv[]) {
   char *fperm;
   int result, exresult=0,  cuserId=0, cgroupId=0, userId=0, groupId=0;
   mode_t mode;

   switch (argc) {
      case 8:
	      mode = strtol(argv[1],(char**)NULL,010);
              cuserId = atoi(argv[2]);
              cgroupId = atoi(argv[3]);
              userId = atoi(argv[4]);
              groupId = atoi(argv[5]);
              fperm = argv[6];
              exresult = atoi(argv[7]);
	      break;
      default:
	      printf("Usage: %s <mode of file> <UID of file> <GID of file> <UID of tester> <GID of tester> <permission to test r|w|x|t|T> <expected result as 0|1>\n",argv[0]);
	      exit(0);
   }

   testsetup(mode,cuserId,cgroupId);
   result=testfperm(userId,groupId,fperm);
   system("rm -f test.file");
   printf("%s a %03o file owned by (%d/%d) as user/group(%d/%d)  ",fperm,mode,cuserId,cgroupId,userId,groupId);
   if (result == exresult) {
      printf("PASS\n");
      exit(0);
      }
   else {
      printf("FAIL\n");
      exit(1);
      }
}

int testsetup(mode_t mode, int cuserId, int cgroupId) {
   system("cp testx.file test.file");
   chmod("test.file",mode);
   chown("test.file",cuserId,cgroupId);
   return(0);
}

int testfperm(int userId, int groupId, char* fperm) {

    int ret;

    /*  SET CURRENT USER/GROUP PERMISSIONS */
    ret = -1;
    if(setegid(groupId)) {
	printf("could not setegid to %d.\n",groupId);
	goto out;
    }
    if(seteuid(userId)) {
	printf("could not seteuid to %d.\n",userId);
	goto out;
    }

    if (!strcmp("x", fperm)) {
	int status;
	pid_t pid;

	pid = fork();
	if (pid == 0) {
	    execlp("./test.file","test.file",NULL);
	    exit(0);
	}
	wait(&status);
	ret = WEXITSTATUS(status);
    } else if (!strcmp("t", fperm)) {
	ret = utime("test.file", NULL) ? 0 : 1;
    } else if (!strcmp("T", fperm)) {
	time_t now = time(NULL);
	struct utimbuf times = {
		.actime = now - 1,
		.modtime = now - 1
	};

	ret = utime("test.file", &times) ? 0 : 1;
    } else {
	FILE *file;

	if((file = fopen("test.file",fperm))){
	    fclose(file);
	    ret = 1;
	    goto out;
	} else {
	    ret = 0;
	    goto out;
	}
    }

out:
    seteuid(0);
    setegid(0);
    return ret;
}
