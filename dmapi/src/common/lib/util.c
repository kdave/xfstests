/*
 * Utility routines
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

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>

#include <lib/hsm.h>

#include <string.h>
#include <time.h>
#ifdef linux
#include <stdint.h>
#define S_IAMB (S_IRWXU|S_IRWXG|S_IRWXO)
#endif

#define	S_MASK	(S_ISUID|S_ISGID|S_ISVTX|S_IAMB)

extern char	*Progname;
extern int	 errno;

void     	 err_msg(char *, ...);
void     	 errno_msg(char *, ...);

struct ev_name_to_value ev_names[] = {
	{ "DM_EVENT_CANCEL",		DM_EVENT_CANCEL		},
	{ "DM_EVENT_MOUNT",		DM_EVENT_MOUNT		},
	{ "DM_EVENT_PREUNMOUNT",	DM_EVENT_PREUNMOUNT	},
	{ "DM_EVENT_UNMOUNT",		DM_EVENT_UNMOUNT	},
	{ "DM_EVENT_DEBUT",		DM_EVENT_DEBUT		},
	{ "DM_EVENT_CREATE",		DM_EVENT_CREATE		},
	{ "DM_EVENT_CLOSE",		DM_EVENT_CLOSE		},
	{ "DM_EVENT_POSTCREATE",	DM_EVENT_POSTCREATE	},
	{ "DM_EVENT_REMOVE",		DM_EVENT_REMOVE		},
	{ "DM_EVENT_POSTREMOVE",	DM_EVENT_POSTREMOVE	},
	{ "DM_EVENT_RENAME",		DM_EVENT_RENAME		},
	{ "DM_EVENT_POSTRENAME",	DM_EVENT_POSTRENAME	},
	{ "DM_EVENT_LINK",		DM_EVENT_LINK		},
	{ "DM_EVENT_POSTLINK",		DM_EVENT_POSTLINK	},
	{ "DM_EVENT_SYMLINK",		DM_EVENT_SYMLINK	},
	{ "DM_EVENT_POSTSYMLINK",	DM_EVENT_POSTSYMLINK	},
	{ "DM_EVENT_READ",		DM_EVENT_READ		},
	{ "DM_EVENT_WRITE",		DM_EVENT_WRITE		},
	{ "DM_EVENT_TRUNCATE",		DM_EVENT_TRUNCATE	},
	{ "DM_EVENT_ATTRIBUTE",		DM_EVENT_ATTRIBUTE	},
	{ "DM_EVENT_DESTROY",		DM_EVENT_DESTROY	},
	{ "DM_EVENT_NOSPACE",		DM_EVENT_NOSPACE	},
	{ "DM_EVENT_USER",		DM_EVENT_USER		}
};

int	ev_namecnt = sizeof(ev_names) / sizeof(ev_names[0]);

dm_eventtype_t
ev_name_to_value(
	char		*name)
{
	int		i;

	for (i = 0; i < ev_namecnt; i++) {
		if (!strcmp(name, ev_names[i].name)) 
			return(ev_names[i].value);
	}
	return(DM_EVENT_INVALID);
}

char *
ev_value_to_name(
	dm_eventtype_t	event)
{
	static char		buffer[100];
	int		i;

	for (i = 0; i < ev_namecnt; i++) {
		if (event == ev_names[i].value)
			return(ev_names[i].name);
	}
	sprintf(buffer, "Unknown Event Number %d\n", event);
	return(buffer);
}



struct rt_name_to_value rt_names[] = {
	{ "DM_RIGHT_NULL",		DM_RIGHT_NULL		},
	{ "DM_RIGHT_SHARED",		DM_RIGHT_SHARED		},
	{ "DM_RIGHT_EXCL",		DM_RIGHT_EXCL		}
};

int	rt_namecnt = sizeof(rt_names) / sizeof(rt_names[0]);

int
rt_name_to_value(
	char		*name,
	dm_right_t	*rightp)
{
	int		i;

	for (i = 0; i < rt_namecnt; i++) {
		if (!strcmp(name, rt_names[i].name)) {
			*rightp = rt_names[i].value;
			return(0);
		}
	}
	return(1);
}


char *
rt_value_to_name(
	dm_right_t	right)
{
	int		i;

	for (i = 0; i < rt_namecnt; i++) {
		if (right == rt_names[i].value)
			return(rt_names[i].name);
	}
	return(NULL);
}
 
 
/*
 * Convert a handle from (possibly) binary to ascii. 
 */
void
hantoa(
	void	*hanp,
	size_t	 hlen,
	char	*handle_str)
{
	int	i;
	u_char	*cp;

	cp = (u_char *)hanp;
	for (i=0;i<hlen; i++)  {
		sprintf(handle_str, "%.2x", *cp++);
		handle_str += 2;
	}
	*handle_str = '\0';	/* Null-terminate to make it printable */
}

/*
 * Convert a handle from ascii back to it's native binary representation
 */

int
atohan(
	char	*handle_str,
	void	**hanpp,
	size_t	 *hlenp) 
{
	u_char	handle[HANDLE_LEN];
	char	cur_char[3];
	int	i = 0;
	u_long	num;

	if (strlen(handle_str) > HANDLE_LEN * 2){
		return(EBADF);
	}

	while (*handle_str && *(handle_str + 1)) {
		if (i == HANDLE_LEN){
			return(EBADF);
		}
		if( ! (isxdigit(*handle_str) && (isxdigit(*(handle_str +1))))) {
			return(EBADF);
		}
		cur_char[0] = *handle_str;
		cur_char[1] = *(handle_str + 1);
		cur_char[2] = '\0';
		num = strtol(cur_char, (char **)0, 16);
		handle[i++] = num & 0xff;
		handle_str += 2;
	}
	if (*handle_str){
		return(EBADF);
	}
	*hlenp = i;
	if ((*hanpp = malloc(*hlenp)) == NULL)
		return(ENOMEM);
	memcpy(*hanpp, handle, *hlenp);
	return(0);
}


int
opaque_to_handle(
	char		*name,
	void		**hanpp,
	size_t		*hlenp)
{
	if (atohan(name, hanpp, hlenp)) {
		/* not a handle */
	} else if (dm_handle_is_valid(*hanpp, *hlenp) == DM_FALSE) {
		dm_handle_free(*hanpp, *hlenp);
		/* not a handle */
	} else {
		return(0);
	}

	/* Perhaps it is a pathname */

	if (dm_path_to_handle(name, hanpp, hlenp)) {
		return(errno);
	}
	return(0);
}


void
print_handle(
	void	*hanp,
	size_t	 hlen)
{
	char	handle_str[HANDLE_STR];

	if (hlen > HANDLE_LEN)  {
		printf("-- invalid hlen length %zd --\n", hlen);
		return;
	}

	printf("print_handle: ");
	printf("%zd\t", hlen);
	hantoa(hanp, hlen, handle_str);
	printf("%s\n ", handle_str);
}

void
print_victim(
	void		*hanp, 
	size_t		 hlen,
	dm_off_t	 fsize)
{
	char    handle_str[HANDLE_STR];

	if (hlen > HANDLE_LEN)  {
		printf("-- invalid length --\n");
		return;
	}

	printf("%zd\t", hlen);
	hantoa(hanp, hlen, handle_str);
	printf("%s ", handle_str);
	printf("\t%lld \n", (long long) fsize);
}


/*
 * Print out a simple error message, and include the errno
 * string with it.
 */
void
errno_msg(char *fmt, ...)
{
	va_list		ap;
	int		old_errno;

	old_errno = errno;
	fprintf(stderr, "%s: ", Progname);

	va_start(ap, fmt );
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	errno = old_errno;
	perror("\n\tError");
}

/*
 * Print out a simple error message
 */
void
err_msg(char *fmt, ...)
{
	va_list		ap;
	
	fprintf(stderr, "%s: ", Progname);

	va_start(ap, fmt );
	vfprintf(stderr, fmt, ap);
	va_end(ap);

}


/*
 * Initialize the interface to the DMAPI
 */
int
setup_dmapi(dm_sessid_t	 *sidp)
{
	char	*cp;

	if (dm_init_service(&cp) == -1)  {
		err_msg("%s/%d: Can't init dmapi\n", __FILE__, __LINE__);
		return(1);
	}
	if (strcmp(cp, DM_VER_STR_CONTENTS)) {
		err_msg("%s/%d: Compiled for a different version\n", __FILE__, __LINE__);
		return(1);
	}

	find_test_session(sidp);
	return(0);
}

/*
 * Get the file's change indicator
 */
int
get_dmchange(
	dm_sessid_t	 sid,
	void		*hanp, 
	size_t	 	 hlen, 
	dm_token_t	 token,
	u_int		*change_start)
{
	int		error;
	dm_stat_t	statbuf;


	error = dm_get_fileattr(sid, hanp, hlen, token, DM_AT_CFLAG, &statbuf);
	if (error == -1) {
		errno_msg("%s/%d: Can't stat file (%d)", __FILE__, __LINE__, errno);
		return(1);
	}
	*change_start = statbuf.dt_change;
	return(0);
}


/*
 * Write a file's data to the staging file. We write the file out
 * in 4meg chunks.
 */
int
save_filedata(
	dm_sessid_t	 sid, 
	void		*hanp, 
	size_t		 hlen, 
	int		 stg_fd, 
	dm_size_t	 fsize)
{
	char		*filebuf;
	int		 i, nbufs;
	int		 retval;
	dm_ssize_t	 nread, lastbuf;
	ssize_t		 nwrite;
	dm_off_t	 off;

        nbufs  = fsize / CHUNKSIZE;
	off    = 0;
	retval = 0;
	filebuf = malloc(CHUNKSIZE);
	if (filebuf == NULL) {
		err_msg("%s/%d: Can't alloc memory for file buffer\n", __FILE__, __LINE__);
		goto out;
	}

        for (i = 0; i<nbufs; i++) {
		nread = dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN, off, 
					(dm_ssize_t)CHUNKSIZE, filebuf);
		if (nread != CHUNKSIZE) {
			errno_msg("%s/%d: invis read err: got %d, expected %d, buf %d",
				__FILE__, __LINE__,
				nread, (dm_ssize_t)CHUNKSIZE, i);
			retval = 1;
			goto out;
		}
		off += nread;

                nwrite = write(stg_fd, filebuf, CHUNKSIZE);
		if (nwrite != CHUNKSIZE) {
			errno_msg("%s/%d: write err %d, expected %d, buf %d",
				  __FILE__, __LINE__,
				  nwrite, CHUNKSIZE, i);
			retval = 1;
			goto out;
		}
        }

        lastbuf = fsize % CHUNKSIZE;
	nread  = dm_read_invis(sid, hanp, hlen, DM_NO_TOKEN, off, 
				(dm_ssize_t)lastbuf, filebuf);
	if (nread != lastbuf) {
		errno_msg("%s/%d: invis read error- got %d, expected %d, last buf",
				__FILE__, __LINE__,
				nread, lastbuf);
		retval = 1;
		goto out;
	}

	nwrite = write(stg_fd, filebuf, (int)lastbuf);
	if (nwrite != lastbuf) {
		errno_msg("%s/%d: write error %d, expected %d, last buffer", 
				__FILE__, __LINE__,
				nwrite, lastbuf);
		retval = 1;
	}
out:
	if (filebuf)
		free(filebuf);

	close(stg_fd);
	return(retval);
}



/*
 * Read a file's data from the staging file. 
 * Since we only have the staging file handle (not a file descriptor)
 * we use dm_read_invis() to read the data. 
 *
 * We stage the entire file in, regardless of how much was asked for, 
 * starting at the faulting offset.
 */
int
restore_filedata(
	dm_sessid_t	 sid, 
	void		*hanp, 
	size_t		 hlen, 
	dm_token_t	 token,
	void		*stg_hanp,
	size_t		 stg_hlen,
	dm_off_t	 off)
{
	char		*filebuf;
	int		 i, nbufs;
	int		 error, retval;
	dm_ssize_t	 nread, nwrite, lastbuf;
	dm_off_t         fsize;
	dm_stat_t        dm_statbuf;

	error = dm_get_fileattr(sid, hanp, hlen, token, DM_AT_STAT,
				&dm_statbuf);
	if (error == -1) {
		errno_msg("%s/%d: Can't get dm stats of file (%d)", __FILE__, __LINE__, errno);
		return(1);
	}

	fsize  = dm_statbuf.dt_size;
        nbufs  = fsize / CHUNKSIZE;
	retval = 0;

	filebuf = malloc(CHUNKSIZE);
	if (filebuf == NULL) {
		err_msg("%s/%d: Can't alloc memory for file buffer\n", __FILE__, __LINE__);
		goto out;
	}

        for (i = 0; i<nbufs; i++) {
		nread = dm_read_invis(sid, stg_hanp, stg_hlen, DM_NO_TOKEN, 
					off, (dm_ssize_t)CHUNKSIZE, filebuf);
		if (nread != CHUNKSIZE) {
			errno_msg("%s/%d: invis read err: got %d, expected %d, buf %d",
				  __FILE__, __LINE__,
				nread, CHUNKSIZE, i);
			retval = 1;
			goto out;
		}

                nwrite = dm_write_invis(sid, hanp, hlen, token, 0, off, nread,
					filebuf);
		if (nwrite != nread) {
			errno_msg("%s/%d: write error -got %d, expected %d, buf %d", 
				  __FILE__, __LINE__,
				nwrite, nread, i);
			retval = 1;
			goto out;
		}
		off += CHUNKSIZE;
        }

        lastbuf = fsize % CHUNKSIZE;
	nread  = dm_read_invis(sid, stg_hanp, stg_hlen, DM_NO_TOKEN, off, 
				(dm_ssize_t)lastbuf, filebuf);
	if (nread != lastbuf) {
		errno_msg("%s/%d: invis read error- got %d, expected %d, last buf",
				__FILE__, __LINE__,
				nread, lastbuf);
		retval = 1;
		goto out;
	}

	nwrite = dm_write_invis(sid, hanp, hlen, token, 0, off, lastbuf, filebuf);
	if (nwrite != lastbuf) {
		errno_msg("%s/%d: write error - got %d, expected %d, last buffer", 
				__FILE__, __LINE__,
				nwrite, lastbuf);
		retval = 1;
	}
out:
	if (filebuf)
		free(filebuf);
	return(retval);
}


extern mode_t
field_to_mode(
	mode_t	mode)
{
	switch (mode & S_IFMT) {

	case S_IFBLK:
		return(S_IFBLK);

	case S_IFREG:
		return(S_IFREG);

	case S_IFDIR:
		return(S_IFDIR);

	case S_IFCHR:
		return(S_IFCHR);

	case S_IFIFO:
		return(S_IFIFO);

	case S_IFLNK:
		return(S_IFLNK);

	case S_IFSOCK:
		return(S_IFSOCK);

	default:
		return(0);
	}
}


extern int
validate_state(
	dm_stat_t	*dmstat,
	char		*pathname,
	int		report_errors)
{
	struct	stat	statb;
	int		errors = 0;

	/* Get the stat block for the file. */

	if (lstat(pathname, &statb)) {
		perror("stat failed");
		exit(1);
	}

	/* Compare its fields against the dm_stat_t structure. */

	if (dmstat->dt_dev != statb.st_dev) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_dev 0x%llx, "
				"statb.st_dev 0x%llx\n",
				(unsigned long long) dmstat->dt_dev,
				(unsigned long long) statb.st_dev);
		}
		errors++;
	}
	if (dmstat->dt_ino != statb.st_ino) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_ino %llx, "
				"statb.st_ino %llx\n",
				(unsigned long long) dmstat->dt_ino,
				(unsigned long long) statb.st_ino);
		}
		errors++;
	}
	if ((dmstat->dt_mode & S_IFMT) != field_to_mode(statb.st_mode)) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_mode (mode) %s, "
				"statb.st_mode (mode) %s\n",
				mode_to_string(dmstat->dt_mode),
				mode_to_string(field_to_mode(statb.st_mode)));
		}
		errors++;
	}
	if ((dmstat->dt_mode & S_MASK) != (statb.st_mode & S_MASK)) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_mode (perm) 0%o, "
				"statb.st_mode (perm) 0%o\n",
				dmstat->dt_mode & S_MASK,
				statb.st_mode & S_MASK);
		}
		errors++;
	}
	if (dmstat->dt_nlink != statb.st_nlink) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_nlink %u, "
				"statb.st_nlink %u\n",
				(unsigned int) dmstat->dt_nlink,
				(unsigned int) statb.st_nlink);
		}
		errors++;
	}
	if (dmstat->dt_uid !=  statb.st_uid) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_uid %d, "
				"statb.st_uid %d\n", dmstat->dt_uid,
				statb.st_uid);
		}
		errors++;
	}
	if (dmstat->dt_gid != statb.st_gid) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_gid %d, "
				"statb.st_gid %d\n", dmstat->dt_gid,
				statb.st_gid);
		}
		errors++;
	}
	if (dmstat->dt_rdev != statb.st_rdev) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_rdev 0x%llx, "
				"statb.st_rdev 0x%llx\n",
				(unsigned long long) dmstat->dt_rdev,
				(unsigned long long) statb.st_rdev);
		}
		errors++;
	}
	if (dmstat->dt_size != statb.st_size) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_size %lld, "
				"statb.st_size %lld\n",
				(long long) dmstat->dt_size,
				(long long) statb.st_size);
		}
		errors++;
	}
	if (dmstat->dt_atime != statb.st_atime) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_atime %ld, "
				"statb.st_atime %ld\n", dmstat->dt_atime,
				statb.st_atime);
		}
		errors++;
	}
	if (dmstat->dt_mtime != statb.st_mtime) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_mtime %ld, "
				"statb.st_mtime %ld\n", dmstat->dt_mtime,
				statb.st_mtime);
		}
		errors++;
	}
	if (dmstat->dt_ctime != statb.st_ctime) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_ctime %ld, "
				"statb.st_ctime %ld\n", dmstat->dt_ctime,
				statb.st_ctime);
		}
		errors++;
	}
	if (dmstat->dt_dtime != statb.st_ctime) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_dtime %ld, "
				"statb.st_ctime %ld\n", dmstat->dt_dtime,
				statb.st_ctime);
		}
		errors++;
	}
	if (dmstat->dt_blksize != statb.st_blksize) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_blksize %d, "
				"statb.st_blksize %ld\n", dmstat->dt_blksize,
				statb.st_blksize);
		}
		errors++;
	}
	if (dmstat->dt_blocks != statb.st_blocks) {
		if (report_errors) {
			fprintf(stdout, "ERROR:dmstat->dt_blocks %lld, "
				"statb.st_blocks %lld\n",
				(long long) dmstat->dt_blocks,
				(long long) statb.st_blocks);
		}
		errors++;
	}

	if (errors && report_errors)
		fprintf(stdout, "There were %d differences\n", errors);
	return(errors);
}


extern char *
date_to_string(
	time_t		timeval)
{
static	char		buffer[21];
	char    	*tmstr;

	if (timeval == (time_t)0) {
		strcpy(buffer, "0");
	} else {
		tmstr = asctime(localtime(&timeval));
		tmstr += 4;
		strncpy(buffer, tmstr, 20);
		buffer[20] = '\0';
	}
	return(buffer);
}


extern char *
mode_to_string(
	mode_t	mode)
{
static	char		buffer[256];

	switch (mode & S_IFMT) {

	case S_IFBLK:
		return("S_IFBLK");

	case S_IFREG:
		return("S_IFREG");

	case S_IFDIR:
		return("S_IFDIR");

	case S_IFCHR:
		return("S_IFCHR");

	case S_IFIFO:
		return("S_IFIFO");

	case S_IFLNK:
		return("S_IFLNK");

	case S_IFSOCK:
		return("S_IFSOCK");

	default:
		sprintf(buffer, "Invalid mode 0%o", mode & S_IFMT);
		return(buffer);
	}
}


extern char *
emask_to_string(
	dm_eventset_t	emask)
{
static	char		buffer[256];
	char		*name;
	int		len = 0;
	int		i;

	for (i = 0; i < 32; i++) {
		if (!DMEV_ISSET(i, emask))
			continue;

		switch (i) {
		case DM_EVENT_CREATE:
			name = "DM_EVENT_CREATE";
			break;
		case DM_EVENT_POSTCREATE:
			name = "DM_EVENT_POSTCREATE";
			break;
		case DM_EVENT_REMOVE:
			name = "DM_EVENT_REMOVE";
			break;
		case DM_EVENT_POSTREMOVE:
			name = "DM_EVENT_POSTREMOVE";
			break;
		case DM_EVENT_RENAME:
			name = "DM_EVENT_RENAME";
			break;
		case DM_EVENT_POSTRENAME:
			name = "DM_EVENT_POSTRENAME";
			break;
		case DM_EVENT_LINK:
			name = "DM_EVENT_LINK";
			break;
		case DM_EVENT_POSTLINK:
			name = "DM_EVENT_POSTLINK";
			break;
		case DM_EVENT_SYMLINK:
			name = "DM_EVENT_SYMLINK";
			break;
		case DM_EVENT_POSTSYMLINK:
			name = "DM_EVENT_POSTSYMLINK";
			break;
		case DM_EVENT_READ:
			name = "DM_EVENT_READ";
			break;
		case DM_EVENT_WRITE:
			name = "DM_EVENT_WRITE";
			break;
		case DM_EVENT_TRUNCATE:
			name = "DM_EVENT_TRUNCATE";
			break;
		case DM_EVENT_ATTRIBUTE:
			name = "DM_EVENT_ATTRIBUTE";
			break;
		case DM_EVENT_DESTROY:
			name = "DM_EVENT_DESTROY";
			break;
		default:
			fprintf(stderr, "Unknown event type %d\n", i);
			exit(1);
		}
		sprintf(buffer + len, "%c%s", (len ? '|' : '('), name);
		len = strlen(buffer);
	}

	if (len == 0) {
		sprintf(buffer, "(none)");
	} else {
		sprintf(buffer + len, ")");
	}
	return(buffer);
}


#if defined(linux)

extern char *
xflags_to_string(
	u_int		xflags)
{
static	char		buffer[256];
	int		len = 0;

	if (xflags & ~(DM_XFLAG_REALTIME|DM_XFLAG_PREALLOC|DM_XFLAG_HASATTR)) {
		sprintf(buffer, "Invalid xflags 0%o", xflags);
		return(buffer);
	}

	if (xflags & DM_XFLAG_REALTIME) {
		sprintf(buffer + len, "%cREALTIME", (len ? '|' : '('));
		len = strlen(buffer);
	}
	if (xflags & DM_XFLAG_PREALLOC) {
		sprintf(buffer + len, "%cPREALLOC", (len ? '|' : '('));
		len = strlen(buffer);
	}
	if (xflags & DM_XFLAG_HASATTR) {
		sprintf(buffer + len, "%cHASATTR", (len ? '|' : '('));
		len = strlen(buffer);
	}
	if (len == 0) {
		sprintf(buffer, "(none)");
	} else {
		sprintf(buffer + len, ")");
	}
	return(buffer);
}

#endif


extern void
print_state(
	dm_stat_t	*dmstat)
{
	/* Print all the stat block fields. */

	fprintf(stdout, "dt_dev         0x%llx\n",
		(unsigned long long) dmstat->dt_dev);
	fprintf(stdout, "dt_ino         %llx\n",
		(unsigned long long) dmstat->dt_ino);
	fprintf(stdout, "dt_mode (type) %s\n",
		mode_to_string(dmstat->dt_mode));
	fprintf(stdout, "dt_mode (perm) 0%o\n", dmstat->dt_mode & S_MASK);
	fprintf(stdout, "dt_nlink       %d\n",  dmstat->dt_nlink);
	fprintf(stdout, "dt_uid         %d\n",  dmstat->dt_uid);
	fprintf(stdout, "dt_gid         %d\n", dmstat->dt_gid);
	fprintf(stdout, "dt_rdev        0x%llx\n",
		(unsigned long long) dmstat->dt_rdev);
	fprintf(stdout, "dt_size        %lld\n",
		(unsigned long long) dmstat->dt_size);

	fprintf(stdout, "dt_atime       %s\n",
		date_to_string(dmstat->dt_atime));
	fprintf(stdout, "dt_mtime       %s\n",
		date_to_string(dmstat->dt_mtime));
	fprintf(stdout, "dt_ctime       %s\n",
		date_to_string(dmstat->dt_ctime));

	fprintf(stdout, "dt_blksize     %d\n", dmstat->dt_blksize);
	fprintf(stdout, "dt_blocks      %lld\n", (long long) dmstat->dt_blocks);

#if defined(linux)
	fprintf(stdout, "dt_xfs_igen    %d\n",  dmstat->dt_xfs_igen);
	fprintf(stdout, "dt_xfs_xflags  %s\n",
		xflags_to_string(dmstat->dt_xfs_xflags));
	fprintf(stdout, "dt_xfs_extsize %d\n", dmstat->dt_xfs_extsize);
	fprintf(stdout, "dt_xfs_extents %d\n", dmstat->dt_xfs_extents);
	fprintf(stdout, "dt_xfs_aextents %d\n", dmstat->dt_xfs_aextents);
#endif

	fputc('\n', stdout);

	/* Print all other fields. */

	fprintf(stdout, "emask           %s\n",
		emask_to_string(dmstat->dt_emask));
	fprintf(stdout, "nevents         %d\n", dmstat->dt_nevents);
	fprintf(stdout, "pmanreg         %d\n", dmstat->dt_pmanreg);
	fprintf(stdout, "pers            %d\n", dmstat->dt_pers);
	fprintf(stdout, "dt_dtime        %s\n",
		date_to_string(dmstat->dt_dtime));
	fprintf(stdout, "change          %d\n", dmstat->dt_change);
}


extern void
print_line(
	dm_stat_t	*dmstat)
{
	fprintf(stdout, "0x%llx|",  (unsigned long long) dmstat->dt_dev);
	fprintf(stdout, "%llx|",  (unsigned long long) dmstat->dt_ino);
	fprintf(stdout, "%s|", mode_to_string(dmstat->dt_mode));
	fprintf(stdout, "0%o|", dmstat->dt_mode & S_MASK);
	fprintf(stdout, "%d|",  dmstat->dt_nlink);
	fprintf(stdout, "%d|",  dmstat->dt_uid);
	fprintf(stdout, "%d|", dmstat->dt_gid);
	fprintf(stdout, "0x%llx|", (unsigned long long) dmstat->dt_rdev);
	fprintf(stdout, "%lld|", (long long) dmstat->dt_size);

	fprintf(stdout, "%s|", date_to_string(dmstat->dt_atime));
	fprintf(stdout, "%s|", date_to_string(dmstat->dt_mtime));
	fprintf(stdout, "%s|", date_to_string(dmstat->dt_ctime));

	fprintf(stdout, "%d|", dmstat->dt_blksize);
	fprintf(stdout, "%lld|", (long long) dmstat->dt_blocks);

	fprintf(stdout, "%d|",  dmstat->dt_xfs_igen);
	fprintf(stdout, "%s|", xflags_to_string(dmstat->dt_xfs_xflags));
	fprintf(stdout, "%d|", dmstat->dt_xfs_extsize);
	fprintf(stdout, "%d|", dmstat->dt_xfs_extents);
	fprintf(stdout, "%d|", dmstat->dt_xfs_aextents);

	/* Print all other fields. */

	fprintf(stdout, "%s|", emask_to_string(dmstat->dt_emask));
	fprintf(stdout, "%d|", dmstat->dt_nevents);
	fprintf(stdout, "%d|", dmstat->dt_pmanreg);
	fprintf(stdout, "%d|", dmstat->dt_pers);
	fprintf(stdout, "%s|", date_to_string(dmstat->dt_dtime));
	fprintf(stdout, "%d", dmstat->dt_change);

	fputc('\n', stdout);
}
