/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_MISC_H__
#define __LIBRSU_MISC_H__

#include "librsu_ll.h"

int librsu_misc_is_rsvd_name(char *name);

int librsu_misc_is_slot(struct librsu_ll_intf *ll_intf, int part_num);
int librsu_misc_slot2part(struct librsu_ll_intf *ll_intf, int slot);

int librsu_misc_get_devattr(char *attr, __u64 *value);
int librsu_misc_put_devattr(char *attr, __u64 value);

#endif
