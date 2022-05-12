/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __IDMAPPED_MOUNTS_H
#define __IDMAPPED_MOUNTS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils.h"

extern const struct test_suite s_idmapped_mounts;
extern const struct test_suite s_fscaps_in_ancestor_userns;

#endif /* __IDMAPPED_MOUNTS_H */
