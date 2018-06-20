// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
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
