// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef _FILE_LOCK_H_
#define _FILE_LOCK_H_

extern char Fl_syscall_str[128];

int file_lock( int , int, char ** );
int record_lock( int , int , int , int , char ** );

#endif /* _FILE_LOCK_H_ */
