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
 
#include "global.h"

char	*alpha;
int	asize = 1;
int	asplit;
char	*buf;
int	dflag;
int	len = 1;
int	nproc = 1;
int	nflag;
int	vflag;
int     pid;

void	mkf(int idx, int p);

int
main(int argc, char **argv)
{
	int		a;
	int		stat;
	long long	tot;
        int             usage=0;
        char *          argv0=argv[0];

	argc--;
	argv++;
        pid=getpid();
        
        if (!argc) usage++;
	while (argc) {
		if (strcmp(*argv, "-c") == 0) {
			argc--;
			argv++;
			asize = atoi(*argv);
			if (asize > 64 || asize < 1) {
				fprintf(stderr, "bad alpha size %s\n", *argv);
				return 1;
			}
		} else if (strcmp(*argv, "-d") == 0) {
			dflag = 1;
		} else if (strcmp(*argv, "-l") == 0) {
			argc--;
			argv++;
			len = atoi(*argv);
			if (len < 1) {
				fprintf(stderr, "bad name length %s\n", *argv);
				return 1;
			}
		} else if (strcmp(*argv, "-p") == 0) {
			argc--;
			argv++;
			nproc = atoi(*argv);
			if (nproc < 1) {
				fprintf(stderr, "bad process count %s\n",
					*argv);
				return 1;
			}
		} else if (strcmp(*argv, "-n") == 0) {
			nflag = 1; 
		} else if (strcmp(*argv, "-v") == 0) {
			vflag = 1;
		} else if (strcmp(*argv, "-h") == 0) {
                        usage++;
		} else {
                        fprintf(stderr,"unknown switch \"%s\"\n", *argv);
                        usage++;
                }
		argc--;
		argv++;
	}
        
        if (usage) {
                fprintf(stderr,"permname: usage %s [-c alpha size] [-l name length] "
                               "[-p proc count] [-n] [-v] [-d] [-h]\n", argv0);
                fprintf(stderr,"          -n : don't actually perform action\n");
                fprintf(stderr,"          -v : be verbose\n");
                fprintf(stderr,"          -d : create directories, not files\n");
                exit(1);
        }
        
	if (asize % nproc) {
		fprintf(stderr,
			"alphabet size must be multiple of process count\n");
		return 1;
	}
	asplit = asize / nproc;
	alpha = malloc(asize + 1);
	buf = malloc(len + 1);
	for (a = 0, tot = 1; a < len; a++)
		tot *= asize;
        if (vflag) fprintf(stderr,"[%d] ",pid);
	fprintf(stderr,
		"alpha size = %d, name length = %d, total files = %lld, nproc=%d\n",
		asize, len, tot, nproc);
	fflush(stderr);
	for (a = 0; a < asize; a++) {
		if (a < 26)
			alpha[a] = 'a' + a;
		else if (a < 52)
			alpha[a] = 'A' + a - 26;
		else if (a < 62)
			alpha[a] = '0' + a - 52;
		else if (a == 62)
			alpha[62] = '_';
		else if (a == 63)
			alpha[63] = '@';
	}
	for (a = 0; a < nproc; a++) {
                int r=fork();
                if (r<0) {
                        perror("fork");
                        exit(1);
                }
		if (!r) {
                        pid=getpid();
			mkf(0, a);
			return 0;
		}
	}
        while (1) {
                int r=wait(&stat);
                if (r<0) {
                        if (errno==ECHILD) break;
                        perror("wait");
                        exit(1);
                }
                if (!r) break;
        }
        
	return 0;
}

void
mkf(int idx, int p)
{
	int	i;
	int	last;

	last = (idx == len - 1);
	if (last) {
		buf[len] = '\0';
		for (i = p * asplit; i < (p + 1) * asplit; i++) {
			buf[idx] = alpha[i];
                        
                        if (dflag) {
                                if (vflag) printf("[%d] mkdir %s\n", pid, buf);
                                if (!nflag) {
                                    if (mkdir(buf, 0777)<0) {
                                        perror("mkdir");
                                        exit(1);
                                    }
                                }
                        } else {
                                if (vflag) printf("[%d] touch %s\n", pid, buf);
				if (!nflag) {
                                    int f=creat(buf, 0666);
                                    if (f<0) {
                                        perror("creat");
                                        exit(1);
                                    }
                                    if (close(f)) {
                                        perror("close");
                                        exit(1);
                                    }
                                }
                        }
		}
	} else {
		for (i = 0; i < asize; i++) {
			buf[idx] = alpha[i];
			mkf(idx + 1, p);
		}
	}
}
