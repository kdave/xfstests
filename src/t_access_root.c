/* 
 *  t_access_root.c - trivial test program to show permission bug.
 *
 *  Written by Michael Kerrisk - copyright ownership not pursued.
 *  Sourced from: http://linux.derkeiler.com/Mailing-Lists/Kernel/2003-10/6030.html  
 */

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#define UID 500
#define GID 100
#define PERM 0
#define TESTPATH "/tmp/t_access"

static void
errExit(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
} /* errExit */

static void
accessTest(char *file, int mask, char *mstr)
{
    printf("access(%s, %s) returns %d\n", file, mstr, access(file, mask));
} /* accessTest */

int
main(int argc, char *argv[])
{
    int fd, perm, uid, gid;
    char *testpath;
    char cmd[PATH_MAX + 20];

    testpath = (argc > 1) ? argv[1] : TESTPATH;
    perm = (argc > 2) ? strtoul(argv[2], NULL, 8) : PERM;
    uid = (argc > 3) ? atoi(argv[3]) : UID;
    gid = (argc > 4) ? atoi(argv[4]) : GID;

    unlink(testpath);

    fd = open(testpath, O_RDWR | O_CREAT, 0);
    if (fd == -1) errExit("open");

    if (fchown(fd, uid, gid) == -1) errExit("fchown");
    if (fchmod(fd, perm) == -1) errExit("fchmod");
    close(fd);

    snprintf(cmd, sizeof(cmd), "ls -l %s", testpath);
    system(cmd);

    if (seteuid(uid) == -1) errExit("seteuid");

    accessTest(testpath, 0, "0");
    accessTest(testpath, R_OK, "R_OK");
    accessTest(testpath, W_OK, "W_OK");
    accessTest(testpath, X_OK, "X_OK");
    accessTest(testpath, R_OK | W_OK, "R_OK | W_OK");
    accessTest(testpath, R_OK | X_OK, "R_OK | X_OK");
    accessTest(testpath, W_OK | X_OK, "W_OK | X_OK");
    accessTest(testpath, R_OK | W_OK | X_OK, "R_OK | W_OK | X_OK");

    exit(EXIT_SUCCESS);
} /* main */
