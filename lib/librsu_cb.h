/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_CB_H__
#define __LIBRSU_CB_H__

#include "librsu_ll.h"
#include <librsu.h>

int librsu_cb_file_init(char *filename);
void librsu_cb_file_cleanup(void);
int librsu_cb_file(void *buf, int len);

int librsu_cb_buf_init(void *buf, int size);
void librsu_cb_buf_cleanup(void);
int librsu_cb_buf(void *buf, int len);

int librsu_cb_program_common(struct librsu_ll_intf *ll_intf, int slot,
			     rsu_data_callback callback, int rawdata);

int librsu_cb_verify_common(struct librsu_ll_intf *ll_intf, int slot,
			    rsu_data_callback callback, int rawdata);

#endif
