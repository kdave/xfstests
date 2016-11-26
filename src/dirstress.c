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
  * This is mostly a "crash & burn" test. -v turns on verbosity
  * and -c actually fails on errors - but expected errors aren't
  * expected...
  */
 
#include "global.h"

int verbose;
int pid;

int checkflag=0;

#define MKNOD_DEV 0

static int dirstress(char *dirname, int	dirnum, int nfiles, int keep, int nprocs_per_dir);
static int create_entries(int nfiles);
static int scramble_entries(int	nfiles);
static int remove_entries(int nfiles);

int
main(
	int	argc,
	char	*argv[])
{
	char	*dirname;
	int	nprocs;
	int	nfiles;
	int	c;
	int	errflg;
	int	i;
	long	seed;
	int	childpid;
	int	nprocs_per_dir;
	int	keep;
        int     status, istatus;
        
        pid=getpid();

	errflg = 0;
	dirname = NULL;
	nprocs = 4;
	nfiles = 100;
	seed = time(NULL);
	nprocs_per_dir = 1;
	keep = 0;
        verbose = 0;
	while ((c = getopt(argc, argv, "d:p:f:s:n:kvc")) != EOF) {
		switch(c) {
			case 'p':
				nprocs = atoi(optarg);
				break;
			case 'f':
				nfiles = atoi(optarg);
				break;
			case 'n':
				nprocs_per_dir = atoi(optarg);
				break;
			case 'd':
				dirname = optarg;
				break;
			case 's':
				seed = strtol(optarg, NULL, 0);
				break;
			case 'k':
				keep = 1;
				break;
			case '?':
				errflg++;
				break;
			case 'v':
				verbose++;
				break;
			case 'c':
                                checkflag++;
                                break;
		}
	}
	if (errflg || (dirname == NULL)) {
		printf("Usage: dirstress [-d dir] [-p nprocs] [-f nfiles] [-n procs per dir]\n"
                       "                 [-v] [-s seed] [-k] [-c]\n");
		exit(0); 
	}

	printf("** [%d] Using seed %ld\n", pid, seed);
	srandom(seed);

	for (i = 0; i < nprocs; i++) {
                if (verbose) fprintf(stderr,"** [%d] fork\n", pid);
		childpid = fork();
		if (childpid < 0) {
			perror("Fork failed");
			exit(errno);
		}
		if (childpid == 0) {
                        int r;
			/* child */
                        pid=getpid();
                        
                        if (verbose) fprintf(stderr,"** [%d] forked\n", pid);
			r=dirstress(dirname, i / nprocs_per_dir, nfiles, keep, nprocs_per_dir);
                        if (verbose) fprintf(stderr,"** [%d] exit %d\n", pid, r);
			exit(r);
		}
	}
        if (verbose) fprintf(stderr,"** [%d] wait\n", pid);
        istatus=0;
        
        /* wait & reap children, accumulating error results */
	while (wait(&status) != -1)
            istatus+=status/256;
        
	printf("INFO: Dirstress complete\n");
        if (verbose) fprintf(stderr,"** [%d] parent exit %d\n", pid, istatus);
	return istatus;
}



int
dirstress(
	char	*dirname,
	int	dirnum,
	int	nfiles,
	int	keep,
	int	nprocs_per_dir)
{
	int		error;
	char		buf[1024];
        int             r;
        
	sprintf(buf, "%s/stressdir", dirname);
        if (verbose) fprintf(stderr,"** [%d] mkdir %s 0777\n", pid, buf);
	error = mkdir(buf, 0777);
	if (error && (errno != EEXIST)) {
		perror("Create stressdir directory failed");
		return 1;
	}

        if (verbose) fprintf(stderr,"** [%d] chdir %s\n", pid, buf);
	error = chdir(buf);
	if (error) {
		perror("Cannot chdir to main directory");
		return 1;
	}

	sprintf(buf, "stress.%d", dirnum);
        if (verbose) fprintf(stderr,"** [%d] mkdir %s 0777\n", pid, buf);
	error = mkdir(buf, 0777);
	if (error && (errno != EEXIST)) {
		perror("Create pid directory failed");
		return 1;
	}

        if (verbose) fprintf(stderr,"** [%d] chdir %s\n", pid, buf);
	error = chdir(buf);
	if (error) {
		perror("Cannot chdir to dirnum directory");
		return 1;
	}

        r=1; /* assume failure */
        if (verbose) fprintf(stderr,"** [%d] create entries\n", pid);
	if (create_entries(nfiles)) {
            printf("!! [%d] create failed\n", pid);
        } else {
            if (verbose) fprintf(stderr,"** [%d] scramble entries\n", pid);
	    if (scramble_entries(nfiles)) {
                printf("!! [%d] scramble failed\n", pid);
            } else {
	        if (keep) {
                    if (verbose) fprintf(stderr,"** [%d] keep entries\n", pid);
                    r=0; /* success */
                } else {
                    if (verbose) fprintf(stderr,"** [%d] remove entries\n", pid);
		    if (remove_entries(nfiles)) {
                        printf("!! [%d] remove failed\n", pid);
                    } else {
                        r=0; /* success */
                    }
                }
            }
        }

        if (verbose) fprintf(stderr,"** [%d] chdir ..\n", pid);
	error = chdir("..");
	if (error) {
		/* If this is multithreaded, then expecting a ENOENT here is fine,
		 * and ESTALE is normal in the NFS case. */
		if (nprocs_per_dir > 1 && (errno == ENOENT || errno == ESTALE)) {
			return 0;
		}

		perror("Cannot chdir out of pid directory");
		return 1;
	}

	if (!keep) {
		sprintf(buf, "stress.%d", dirnum);
                if (verbose) fprintf(stderr,"** [%d] rmdir %s\n", pid, buf);
		if (rmdir(buf)) {
                    perror("rmdir");
                    if (checkflag) return 1;
                }
	}
	
        if (verbose) fprintf(stderr,"** [%d] chdir ..\n", pid);
	error = chdir("..");
	if (error) {
		/* If this is multithreaded, then expecting a ENOENT here is fine,
		 * and ESTALE is normal in the NFS case. */
		if (nprocs_per_dir > 1 && (errno == ENOENT || errno == ESTALE)) {
			return 0;
		}

		perror("Cannot chdir out of working directory");
		return 1;
	}

	if (!keep) {
                if (verbose) fprintf(stderr,"** [%d] rmdir stressdir\n", pid);
		if (rmdir("stressdir")) {
                    perror("rmdir");
                    if (checkflag) return 1;
                }
        }

	return r;
}

int
create_entries(
       int	nfiles)
{
	int	i;
	int	fd;
	char	buf[1024];

	for (i = 0; i < nfiles; i++) {
		sprintf(buf, "XXXXXXXXXXXX.%d", i);
		switch (i % 4) {
		case 0:
			/*
			 * Create a file
			 */
                        if (verbose) fprintf(stderr,"** [%d] creat %s\n", pid, buf);
			fd = creat(buf, 0666);
			if (fd > 0) {
                                if (verbose) fprintf(stderr,"** [%d] close %s\n", pid, buf);
				close(fd);
			} else {
                                fprintf(stderr,"!! [%d] close %s failed\n", pid, buf);
                                perror("close");
                                if (checkflag) return 1;
                        }
                        
			break;
		case 1:
			/*
			 * Make a directory.
			 */
                        if (verbose) fprintf(stderr,"** [%d] mkdir %s 0777\n", pid, buf);
			if (mkdir(buf, 0777)) {
                            fprintf(stderr,"!! [%d] mkdir %s 0777 failed\n", pid, buf);
                            perror("mkdir");
                            if (checkflag) return 1;
                        }
                        
			break;
		case 2:
			/*
			 * Make a symlink
			 */
                        if (verbose) fprintf(stderr,"** [%d] symlink %s %s\n", pid, buf, buf);
			if (symlink(buf, buf)) {
                            fprintf(stderr,"!! [%d] symlink %s %s failed\n", pid, buf, buf);
                            perror("symlink");
                            if (checkflag) return 1;
                        }
                        
			break;
		case 3:
			/*
			 * Make a dev node
			 */
                        if (verbose) fprintf(stderr,"** [%d] mknod %s 0x%x\n", pid, buf, MKNOD_DEV);
			if (mknod(buf, S_IFCHR | 0666, MKNOD_DEV)) {
                            fprintf(stderr,"!! [%d] mknod %s 0x%x failed\n", pid, buf, MKNOD_DEV);
                            perror("mknod");
                            if (checkflag) return 1;
                        }
                        
			break;
		default:
			break;
		}
	}
        return 0;
}


int
scramble_entries(
	int	nfiles)
{
	int		i;
	char		buf[1024];
	char		buf1[1024];
	long		r;
	int		fd;

	for (i = 0; i < nfiles * 2; i++) {
		switch (i % 5) {
		case 0:
			/*
			 * rename two random entries
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%ld", r);
			r = random() % nfiles;
			sprintf(buf1, "XXXXXXXXXXXX.%ld", r);

                        if (verbose) fprintf(stderr,"** [%d] rename %s %s\n", pid, buf, buf1);
			if (rename(buf, buf1)) {
                            perror("rename");
                            if (checkflag) return 1;
                        }
			break;
		case 1:
			/*
			 * unlink a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%ld", r);
                        if (verbose) fprintf(stderr,"** [%d] unlink %s\n", pid, buf);
			if (unlink(buf)) {
                            fprintf(stderr,"!! [%d] unlink %s failed\n", pid, buf);
                            perror("unlink");
                            if (checkflag) return 1;
                        }
			break;
		case 2:
			/*
			 * rmdir a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%ld", r);
                        if (verbose) fprintf(stderr,"** [%d] rmdir %s\n", pid, buf);
			if (rmdir(buf)) {
                            fprintf(stderr,"!! [%d] rmdir %s failed\n", pid, buf);
                            perror("rmdir");
                            if (checkflag) return 1;
                        }
			break;
		case 3:
			/*
			 * create a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%ld", r);

                        if (verbose) fprintf(stderr,"** [%d] creat %s 0666\n", pid, buf);
			fd = creat(buf, 0666);
			if (fd > 0) {
                                if (verbose) fprintf(stderr,"** [%d] close %s\n", pid, buf);
				if (close(fd)) {
                                    fprintf(stderr,"!! [%d] close %s failed\n", pid, buf);
                                    perror("close");
                                    if (checkflag) return 1;
                                }
			} else {
                            fprintf(stderr,"!! [%d] creat %s 0666 failed\n", pid, buf);
                            perror("creat");
                            if (checkflag) return 1;
                        }
			break;
		case 4:
			/*
			 * mkdir a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%ld", r);
                        if (verbose) fprintf(stderr,"** [%d] mkdir %s\n", pid, buf);
			if (mkdir(buf, 0777)) {
                            fprintf(stderr,"!! [%d] mkdir %s failed\n", pid, buf);
                            perror("mkdir");
                            if (checkflag) return 1;
                        }
			break;
		default:
			break;
		}
	}
        return 0;
}
			
int
remove_entries(
	int	nfiles)
{
	int		i;
	char		buf[1024];
	struct stat	statb;
	int		error;

	for (i = 0; i < nfiles; i++) {
		sprintf(buf, "XXXXXXXXXXXX.%d", i);
		error = lstat(buf, &statb);
		if (error) {
                        /* ignore this one */
			continue;
		}
		if (S_ISDIR(statb.st_mode)) {
                        if (verbose) fprintf(stderr,"** [%d] rmdir %s\n", pid, buf);
			if (rmdir(buf)) {
                            fprintf(stderr,"!! [%d] rmdir %s failed\n", pid, buf);
                            perror("rmdir");
                            if (checkflag) return 1;
                        }
		} else {
                        if (verbose) fprintf(stderr,"** [%d] unlink %s\n", pid, buf);
			if (unlink(buf)) {
                            fprintf(stderr,"!! [%d] unlink %s failed\n", pid, buf);
                            perror("unlink");
                            if (checkflag) return 1;
                        }
		}
	}
        return 0;
}
