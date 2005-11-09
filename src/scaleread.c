/*
 * Copyright (c) 2003-2004 Silicon Graphics, Inc.
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
 * Test scaling of multiple processes opening/reading
 * a number of small files simultaneously.
 *	- create <f> files
 *	- fork <n> processes
 *	- wait for all processes ready
 *	- start all proceses at the same time
 *	- each processes opens , read, closes each file
 *	- option to resync each process at each file
 *
 *	test [-c cpus] [-b bytes] [-f files] [-v] [-s] [-S]
 *			OR
 *	test -i [-b bytes] [-f files] 
 */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

void do_initfiles(void);
void slave(int);

#define VPRINT(x...)	do { if(verbose) fprintf(x);} while(0)
#define perrorx(s) do {perror(s); exit(1);} while (0)

long bytes=8192;
int cpus=1;
int init=0;
int strided=0;
int files=1;
int blksize=512;
int syncstep=0;
int verbose=0;

typedef struct {
        volatile long   go;
        long            fill[15];
        volatile long   rdy[512];
} share_t;

share_t	*sharep;


int
runon(int cpu)
{
#ifdef sys_sched_setaffinity
	unsigned long mask[8];
	
	if (cpu < 0 || cpu >= 512)
		return -1;
	memset(mask, 0, sizeof(mask));
	mask[cpu/64] |= 1UL<<(cpu&63);

	if (syscall(sys_sched_setaffinity, 0, sizeof(mask), mask))
		return -1;
#endif
	return 0;
}

long
scaled_atol(char *p)
{
	long val;
	char  *pe;

	val = strtol(p, &pe, 0);
	if (*pe == 'K' || *pe == 'k')
		val *= 1024L;
	else if (*pe == 'M' || *pe == 'm')
		val *= 1024L*1024L;
	else if (*pe == 'G' || *pe == 'g')
		val *= 1024L*1024L*1024L;
	else if (*pe == 'p' || *pe == 'P')
		val *= getpagesize();
	return val;
}


int
main(int argc, char** argv) {
        int shmid;
        static  char            optstr[] = "c:b:f:sSivH";
        int                     notdone, stat, i, j, c, er=0;

        opterr=1;
        while ((c = getopt(argc, argv, optstr)) != EOF)
                switch (c) {
                case 'c':
                        cpus = atoi(optarg);
                        break;
                case 'b':
                        bytes = scaled_atol(optarg);
                        break;
                case 'f':
                        files = atoi(optarg);
                        break;
                case 'i':
                        init++;
                        break;
                case 's':
                        syncstep++;
                        break;
                case 'S':
                        strided++;
                        break;
                case 'v':
                        verbose++;
                        break;
                case '?':
                        er = 1;
                        break;
                }
        if (er) {
                printf("usage: %s %s\n", argv[0], optstr);
                exit(1);
        }


	if ((shmid = shmget(IPC_PRIVATE, sizeof (share_t), IPC_CREAT|SHM_R|SHM_W))  == -1)
		perrorx("shmget failed");
	sharep = (share_t*)shmat(shmid, (void*)0, SHM_R|SHM_W);
	memset(sharep, -1, sizeof (share_t));

	if (init) {
		do_initfiles();
		exit(0);
	}
        for (i=0; i<cpus; i++) {
                if (fork() == 0)
                        slave(i);
        }

	for (i=0; i<files; i++) {
		VPRINT(stderr, "%d:", i);
		notdone = cpus;
		do {
			for (j=0; j<cpus; j++) {
				if (sharep->rdy[j] == i) {
					sharep->rdy[j] = -1;
					VPRINT(stderr, " %d", j);
					notdone--;
				}
			}
		} while (notdone);
		VPRINT(stderr, "\n");
		sharep->go = i;
		if (!syncstep)
			break;
	}
	VPRINT(stderr, "\n");

        while (wait(&stat)> 0)
		VPRINT(stderr, ".");
	VPRINT(stderr, "\n");

	exit(0);
}

void 
slave(int id)
{
	int	i, fd, byte;
	char	*buf, filename[32];

	runon (id+1);
	buf = malloc(blksize);
	bzero(buf, blksize);
	for (i=0; i<files; i++) {
		if (!i || syncstep) {
			sharep->rdy[id] = i;
			while(sharep->go != i);
		}
		sprintf(filename, "/tmp/tst.%d", (strided ? ((i + id) % files) : i));
		if ((fd = open (filename, O_RDONLY)) < 0) {
			perrorx(filename);
		}
	
		for (byte=0; byte<bytes; byte+=blksize) {
			if (read (fd, buf, blksize) != blksize)
				perrorx("read of file failed");
		}
		close(fd);
	}
	exit(0);
}

void
do_initfiles(void)
{
	int	i, fd, byte;
	char	*buf, filename[32];

	buf = malloc(blksize);
	bzero(buf, blksize);

	for (i=0; i<files; i++) {
		sprintf(filename, "/tmp/tst.%d", i);
		unlink(filename);
		if ((fd = open (filename, O_RDWR|O_CREAT, 0644)) < 0)
			perrorx(filename);
	
		for (byte=0; byte<bytes; byte+=blksize) {
			if (write (fd, buf, blksize) != blksize)
				perrorx("write of file failed");
		}
		close(fd);
	}
	sync();
}


