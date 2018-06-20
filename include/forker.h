// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef _FORKER_H_
#define _FORKER_H_

#define FORKER_MAX_PIDS	4098

extern int Forker_pids[FORKER_MAX_PIDS];      /* holds pids of forked processes */
extern int Forker_npids;               /* number of entries in Forker_pids */

/*
 * This function will fork and the parent will exit zero and
 * the child will return.  This will orphan the returning process
 * putting it in the background.
 */
int background( char * );

/*
 * Forker will fork ncopies-1 copies of self. 
 *
 * arg 1: Number of copies of the process to be running after return.
 *        This value minus one is the number of forks performed. 
 * arg 2: mode: 0 - all children are first generation descendents.
 *              1 - each subsequent child is a descendent of another
 *              descendent, resulting in only one direct descendent of the
 *              parent and each other child is a child of another child in
 *              relation to the parent.
 * arg 3: prefix: string to preceed any error messages.  A value of NULL
 *              results in no error messages on failure.
 * returns: returns to parent the number of children forked.
 */
int forker( int , int , char * );

#endif /* _FORKER_H_ */
