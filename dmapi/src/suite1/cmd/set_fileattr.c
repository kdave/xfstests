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

#include <ctype.h>
#include <time.h>

#include <lib/hsm.h>

#include <getopt.h>
#include <string.h>



extern	int	optind;
extern	int	opterr;
extern	char	*optarg;

char		*Progname;

#define MIN_HD_DATE     19800101
#define MIN_HD_TMSTAMP  315554400       /* timestamp of 19800101 */

static	int	dayno, day_of_week;
static	int	leap_year;

/*
 * The following table is used for USA daylight savings time and
 * gives the day number of the first day after the Sunday of the
 * change.
 */
static struct {
	int	yrbgn;
	int	daylb;
	int	dayle;
} daytab[] = {
	{1987,	96,	303},	/* new legislation - 1st Sun in April */
	{1976,	119,	303},	/* normal Last Sun in Apr - last Sun in Oct */
	{1975,	58,	303},	/* 1975: Last Sun in Feb - last Sun in Oct */
	{1974,	5,	333},	/* 1974: Jan 6 - last Sun. in Nov */
	{1970,	119,	303},	/* start GMT */
};
#define DAYTABSIZE (sizeof(daytab)/sizeof(daytab[0]))


/******************************************************************************
* NAME
*	dysize
*
* DESCRIPTION
*	Return number of days in year y.
*
******************************************************************************/

int
dysize(int y)
{
	int	temp;
	temp = (y%4)==0;
	if (temp) {
		if ( (y%100)==0) 
			temp = ( (y%400) != 0);
	}
	return(365+temp);
}

/******************************************************************************
* NAME
*	sunday
*
* DESCRIPTION
*	sunday - return sunday daynumber.  Argument d is the day number of the
*	first Sunday on or before the special day.  Variables leap_year, dayno,
*	and day_of_week must have been set before sunday is called.
*
* RETURN VALUE
*	The sunday daynumber.
*
******************************************************************************/

static int
sunday(int d)
{
	if(d >= 58)
		d += leap_year;
	return(d - (d - dayno + day_of_week + 700) % 7);
}


extern	long 
cnvdate(int mon, int mday, int year, int hour, int min, int sec)
{
	int	t, i;
	int	daylbegin, daylend;
	int	ly_correction;	/* Leap Year Correction */
	int	dl_correction;	/* Daylight Savings Time Correction */
	long	s;
	static int days[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

	days[2] = 28;

	/* Verify Input Parameters. */
 
	/*	Set  year. */

	if( year < 0) {
		return(-1);
	}
	if( year < 100) {
		if (year < 70) year += 2000;
		else year += 1900;
	}
	if (year < 1970) {
		return(-1);
	}
	if( year>2099 ) {
		return(-1);
	}

	if (dysize(year) == 366) {
		leap_year = 1;
		days[2]++;
	} else	
		leap_year = 0;
	/*
	 * Set ly_correction = number of leap year days from 1/1/1970 to
	 * 1/1/year.
	 */
	ly_correction =  ((year-1969) / 4);

	/* Check Month */

	if( (mon < 1) || (mon > 12)) {
		return(-1);
	}

	/* Check Day */

	if ( (mday < 1) || (mday > days[mon]) ) {
		return(-1);
	}

	/* Check Time */

	if( (hour<0) || (hour>23)) {
		return(-1);
	}
	if( (min<0) || (min>59)) {
		return(-1);
	}
	if( (sec<0) || (sec>59)) {
		return(-1);
	}

	/* Calculate Correction for Daylight Savings Time (U.S.) */

	dayno = mday-1;
	for (t=0; t<mon;)
		dayno += days[t++];
	s = (year-1970)*365L + ly_correction + dayno;
	day_of_week = (s + 4) % 7;

	i = 0;
	while (year < daytab[i].yrbgn) { 
		/* fall through when in correct interval */ 
		i++;
		if (i>DAYTABSIZE)
			return(-1);
	}
	daylbegin = daytab[i].daylb; 
	daylend = daytab[i].dayle;

	daylbegin = sunday(daylbegin);
	daylend = sunday(daylend);
	if(daylight &&
	    (dayno>daylbegin || (dayno==daylbegin && hour>=2)) &&
	    (dayno<daylend || (dayno==daylend && hour<1)))
		dl_correction = -1*60*60;
	else
		dl_correction = 0;

	/* Calculate seconds since 00:00:00 1/1/1970. */

	s = ( ( s*24 +hour)*60 +min)*60 + sec + dl_correction;
	return(s+timezone);
}


/* Cracks dates in the form YYYYMMDD and YYYYMMDDHHMMSS.  */

static int
get_absolute_date(
	char	*ptr,
	time_t	*timestamp)
{
	int	l;
	int	yr;
	int	mon;
	int	day;
	int	hr;
	int	mn;
	int	sec;
	char	date[9];
	char	*last_char;

	if (strlen(ptr) != 8 && strlen(ptr) != 14)
		return(0);
	strncpy(date, ptr, 8);
	date[8] = '\0';
	l = atol(date);
	if (l < MIN_HD_DATE)
		return(0);
	yr = l / 10000;
	l = l % 10000;
	mon = l / 100;
	if (mon < 0 || mon > 12)
		return(0);
	day = l % 100;

	/* Note: invalid day numbers are caught in cnvdate */

	ptr+=8;

	l = strtol(ptr, &last_char, 10);
	if (l < 0 || l>235959 || *last_char != '\0')
		return(0);
	hr = l / 10000;
	if (hr > 23)
		return(0);
	l = l % 10000;
	mn = l / 100;
	if (mn > 59)
		return(0);
	sec = l % 100;
	if (sec > 59)
		return(0);

	/* Get timestamp. */

	(void)tzset();
	if ((*timestamp = cnvdate(mon, day, yr, hr, mn, sec)) < 0) {
		return(0);
	}

	return(1);
}


/* Cracks dates in the form:   NNs, NNm, NNh, or NNd which are interpreted
   as NN seconds, minutes, hours, or days prior to the current time, 
   respectively.
*/

static int
get_relative_date(
	char	*ptr,
	time_t	*timestamp)
{
	int	l;
	char	*last_char;

	if (!isdigit(*ptr))
		return(0);
	l = strtol (ptr, &last_char, 10);
	(void) time(timestamp);
	if (strcmp(last_char, "s") == 0)
		/* do nothing */;
	else if (strcmp(last_char, "m") == 0)
		l = l * 60;
	else if (strcmp(last_char, "h") == 0)
		l = l * 60 * 60;
	else if (strcmp(last_char, "d") == 0)
		l = l * 60 * 60 * 24;
	else
		return(0);
	*timestamp -= l;
	if (*timestamp < MIN_HD_TMSTAMP)
		return(0);
	return(1);
}


static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-M mode] [-u uid] [-g gid] [-a atime] \\\n"
		"\t[-m mtime] [-c ctime] [-d dtime] [-S size] [-s sid] pathname\n",
		Progname);
	fprintf(stderr, "\nDates can either be absolute:\n");
	fprintf(stderr, "\t\tYYYYMMDD or YYYYMMDDHHMMSS\n");
	fprintf(stderr, "or relative (prior to) the current time:\n");
	fprintf(stderr, "\tNNs (seconds), NNm (minutes), NNh (hours), "
		" or NNd (days)\n");
	exit(1);
}


int
main(
	int	argc,
	char	**argv)
{
	dm_sessid_t	sid = DM_NO_SESSION;
	void		*hanp;
	size_t		hlen;
	dm_fileattr_t	fileattr;
	u_int		mask = 0;
	char		*pathname;
	char		*name;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	opterr = 0;
	while ((opt = getopt(argc, argv, "M:u:g:a:m:c:d:S:s:")) != EOF) {
		switch (opt) {
		case 'M':
			mask |= DM_AT_MODE;
			fileattr.fa_mode = strtol (optarg, NULL, 8);
			break;
		case 'u':
			mask |= DM_AT_UID;
			fileattr.fa_uid = atol(optarg);
			break;
		case 'g':
			mask |= DM_AT_GID;
			fileattr.fa_gid = atol(optarg);
			break;
		case 'a':
			mask |= DM_AT_ATIME;
			if (get_absolute_date(optarg, &fileattr.FA_ATIME))
				break;
			if (get_relative_date(optarg, &fileattr.FA_ATIME))
				break;
			usage();
		case 'm':
			mask |= DM_AT_MTIME;
			if (get_absolute_date(optarg, &fileattr.FA_MTIME))
				break;
			if (get_relative_date(optarg, &fileattr.FA_MTIME))
				break;
			usage();
		case 'c':
			mask |= DM_AT_CTIME;
			if (get_absolute_date(optarg, &fileattr.FA_CTIME))
				break;
			if (get_relative_date(optarg, &fileattr.FA_CTIME))
				break;
			usage();
		case 'd':
			mask |= DM_AT_DTIME;
			if (get_absolute_date(optarg, &fileattr.FA_DTIME))
				break;
			if (get_relative_date(optarg, &fileattr.FA_DTIME))
				break;
			usage();
		case 'S':
			mask |= DM_AT_SIZE;
			fileattr.fa_size = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc) {
		usage();
	}
	pathname = argv[optind];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}
	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	if (dm_path_to_handle(pathname, &hanp, &hlen)) {
		fprintf(stderr, "dm_path_to_handle failed, %s\n",
			strerror(errno));
		exit(1);
	}

	if (dm_set_fileattr(sid, hanp, hlen, DM_NO_TOKEN, mask, &fileattr)) {
		fprintf(stderr, "dm_set_fileattr failed, %s\n",
			strerror(errno));
		exit(1);
	}
	exit(0);
}
