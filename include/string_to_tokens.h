/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
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
#ifndef _STRING_TO_TOKENS_H_
#define _STRING_TO_TOKENS_H_

/*
 * string_to_tokens() 
 *
 * This function parses the string 'arg_string', placing pointers to
 * the 'separator' separated tokens into the elements of 'arg_array'.
 * The array is terminated with a null pointer.
 *
 * NOTE: This function uses strtok() to parse 'arg_string', and thus
 * physically alters 'arg_string' by placing null characters where the
 * separators originally were.
 */
int string_to_tokens(char *, char **, int, char *);

#endif
