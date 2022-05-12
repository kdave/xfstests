/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __IDMAPPED_MOUNTS_H
#define __IDMAPPED_MOUNTS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils.h"

extern const struct test_suite s_idmapped_mounts;
extern const struct test_suite s_fscaps_in_ancestor_userns;
extern const struct test_suite s_nested_userns;
extern const struct test_suite s_setattr_fix_968219708108;
extern const struct test_suite s_setxattr_fix_705191b03d50;

#endif /* __IDMAPPED_MOUNTS_H */
