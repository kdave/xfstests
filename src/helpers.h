#ifndef _HELPERS_H
#define _HELPERS_H

#include "udf_ecma167.h"
#include "udf.h"

#ifndef howmany
#define howmany(x,y) (((x)+(y)-1)/(y))
#endif

extern uint8_t *test_bitmap;
extern uint8_t *test_disk;
extern uint32_t test_disk_len;
extern uint32_t test_bitmap_len;
extern struct udf_space_bitmap_desc bmap_desc;

/* device values to decide which data to read/write from */
#define UDF_DEV_BITMAP	1
#define UDF_DEV_DISK	2

/* keep it small so that I don't need to alloc much memory :) */
#define UDF_MY_PART_START	2  /* partition start in sectors */

void udf_print_space_bitmap(void);
void setup_space(dev_t dev, udf_mnt_t *udfmp, uint32_t num_sectors);
void setup_inode(
	udf_mnt_t *udfmp, 
	bhv_desc_t *bhv_desc,
	udf_inode_t *inode,
	vnode_t *vnode, 
	cred_t *cred,
	struct udf_xfile_entry *fentry);
void make_disk_ones(void);
void print_disk(void);
void free_all_extents(udf_inode_t *);

#endif
