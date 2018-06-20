// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef _DATAASCII_H_
#define _DATAASCII_H_

/***********************************************************************
 * int dataasciigen(listofchars, buffer, size, offset)
 *
 * This function fills buffer with ascii characters.
 * The ascii characters are obtained from listofchars or the CHARS array
 *  if listofchars is NULL.
 * Each char is selected by an index.  The index is the remainder
 *  of count divided by the array size. 
 * This method allows more than one process to write to a location
 *  in a file without corrupting it for another process' point of view.
 *
 * The return value will be the number of character written in buffer
 *  (size).
 *
 ***********************************************************************/
int dataasciigen(char *, char *, int, int);

/***********************************************************************
 * int dataasciichk(listofchars, buffer, size, count, errmsg)
 *
 * This function checks the contents of a buffer produced by
 *  dataasciigen.
 *
 * return values:
 *	>= 0 : error at character count
 *	< 0  : no error
 ***********************************************************************/

int dataasciichk(char *, char *, int, int, char**);

#endif
