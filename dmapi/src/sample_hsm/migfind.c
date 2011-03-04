/*
 * Simple utility to find all files above a certain size in 
 * a filesystem. The output is to stdout, and is of the form:
 *	filehandle length	filehandle 	file size in bytes
 *
 * The list is not sorted in any way.
 *
 * This code was written by Peter Lawthers, and placed in the public
 * domain for use by DMAPI implementors and app writers.
 *
 * Standard disclaimer:
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <lib/hsm.h>

#include <getopt.h>

#define NUMLEN	16		/* arbitrary max len of input size */
#define MAX_K   (((u_int)LONG_MAX) / 1024)
#define MAX_M   (((u_int)LONG_MAX) / (1024*1024))



extern char	*optarg;
extern int	 optind, optopt, opterr;
char		*Progname;

extern void	print_victim(void *, size_t, dm_off_t);
extern void	err_msg(char *, ...);
extern void	errno_msg(char *, ...);

int 		setup_dmapi(dm_sessid_t *);
int		scan_fs(dm_sessid_t, void *, size_t, dm_off_t);
int		verify_size(char *, dm_off_t *);
void		usage(char *);

void
usage(
	char *prog)
{
	fprintf(stderr, "Usage: %s ", prog);
	fprintf(stderr, " [-s file threshold]");
	fprintf(stderr, " filesystem\n");
}

/*
 * Convert an input string on the form 10m or 128K to something reasonable
 */
int
verify_size(
        char		*str,
        dm_off_t	*sizep)
{
        char		*cp;
        dm_off_t	 size;

        if (strlen(str) > NUMLEN) {
                printf("Size %s is invalid \n", str);
                return(1);
        }

        size = strtol(str,0,0); 
        if (size < 0 || size >= LONG_MAX ) {
                printf("Size %lld is invalid \n", (long long) size);
                return(1);
        }

        cp = str;
        while (isdigit(*cp))
                cp++;
        if (*cp == 'k' || *cp == 'K') {
                if ( size >= (u_int) MAX_K) {
                        printf("Size %lld is invalid\n", (long long) size);
                        return(1);
                }
                size *= 1024;
        } else if (*cp == 'm' || *cp == 'M') {
                if ( size >= (u_int) MAX_M) {
                        printf("Size %lld is invalid\n", (long long) size);
                        return(1);
                }
                size *= (1024*1024);
        }
        *sizep = size;
        return(0);
}



int
main(
	int	argc, 
	char	*argv[])
{
	
	int	 	 c;
	int	 	 error;
	dm_off_t 	 size;
	char		*fsname;
	dm_sessid_t	 sid;
	void		*fs_hanp;
	size_t	 	 fs_hlen;
	char		*sizep = "0";


	Progname = argv[0];
	size     = 0;

	while ((c = getopt(argc, argv, "s:")) != EOF) {
		switch (c) {
		case 's':
			sizep = optarg;
			break;

		case '?':
		default:
			usage(Progname);
			exit(1);
		}
	}
	if (optind >= argc) {
		usage(Progname);
		exit(1);
	}
	/*
	 * Verify the input size string is legit
	 */
	error = verify_size(sizep, &size);
	if (error) 
		exit(1);

	fsname = argv[optind];

	/*
	 * Now we have our filesystem name and possibly a size threshold
	 * to look for. Init the dmapi, and get a filesystem handle so
	 * we can scan the filesystem
	 */
	error = setup_dmapi(&sid);
	if (error) 
		exit(1);
	
	if (dm_path_to_fshandle(fsname, &fs_hanp, &fs_hlen) == -1) {
		errno_msg("Can't get filesystem handle");
		exit(1);
	}



	/*
	 * Get the attributes of all files in the filesystem
	 */
	error = scan_fs(sid, fs_hanp, fs_hlen, size);
	if (error) 
		exit(1);
	

	/*
	 * We're done, so we can shut down our session.
	 */
	if (dm_destroy_session(sid) == -1) {
		errno_msg("Can't close session");
		exit(1);
	}

	return(0);

}

/*
 * Get the attributes for all the files in a filesystem in bulk,
 * and print out the handles and sizes of any that meet our target
 * criteria.
 *
 * We are not interested in file names; if we were, then we would 
 * have to do a dm_get_dirattrs() on each directroy, then use 
 * dm_handle_to_path() to get the pathname. 
 */
int
scan_fs(
	dm_sessid_t 	 sid, 
	void		*fs_hanp, 
	size_t 	 	 fs_hlen, 
	dm_off_t	 target_size)
{
	u_int		 mask;			/* attributes to scan for */
	dm_stat_t	*dm_statbuf, *sbuf;	/* attributes buffer */
	dm_attrloc_t	 locp;			/* opaque location in fs */
	size_t		 rlenp;			/* ret length of stat info */
	size_t		 buflen;		/* length of stat buffer */
	void		*hanp;			/* file handle */
	size_t	 	 hlen;			/* file handle */
	int		 more;			/* loop terminator */
	int		 error; 


	/*
	 * Size the stat buffer to return info on 1K files at a time
	 */
	buflen     = sizeof(dm_stat_t) * 1024;
#ifdef	VERITAS_21
	if (buflen > 65536)
		buflen = 65536;
#endif
	dm_statbuf = (dm_stat_t *)calloc(1, buflen);
	if (dm_statbuf == NULL)  {
		err_msg("Can't get memory for stat buffer");
		return(1);
	}


	/*
	 * Initialize the offset opaque offset cookie that
	 * we use in successive calls to dm_get_bulkattr()
	 */
	error = dm_init_attrloc(sid, fs_hanp, fs_hlen, DM_NO_TOKEN, &locp);
	if (error == -1) {
		errno_msg("%s/%d: Can't initialize offset cookie (%d)", __FILE__, __LINE__, errno);
		free(dm_statbuf);
		return(1);
	}

	/*
	 * Set our stat mask so that we'll only get the normal stat(2)
	 * info and the file's handle
	 */
	mask = DM_AT_HANDLE | DM_AT_STAT;
	do {
		more = dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
				       mask, &locp, buflen, dm_statbuf, &rlenp);
		if (more == -1) {
			errno_msg("%s/%d: Can't get bulkattr for filesystem", __FILE__, __LINE__, errno);
			break;
		}

		/*
		 * Walk through the stat buffer and pull out files 
		 * that are of interest
		 *
		 * The stat buffer is variable length, so we must
		 * use the DM_STEP_TO_NEXT macro to access each individual
		 * dm_stat_t structure in the returned buffer.
		 */
		sbuf = dm_statbuf;
		while (sbuf != NULL) {
			if (S_ISREG(sbuf->dt_mode) &&
			    sbuf->dt_size >= target_size) {	
				hanp = DM_GET_VALUE(sbuf, dt_handle, void *);
				hlen = DM_GET_LEN(sbuf, dt_handle);

				print_victim(hanp, hlen, sbuf->dt_size);
			}
			sbuf = DM_STEP_TO_NEXT(sbuf, dm_stat_t *);
		}
	} while (more == 1);

	free(dm_statbuf);
	if (more == -1) 
		return(1);

	return(0);
}

