/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_CB_H__
#define __LIBRSU_CB_H__

int librsu_cb_file_init(char *filename);
void librsu_cb_file_cleanup(void);
int librsu_cb_file(void *buf, int len);

int librsu_cb_buf_init(void *buf, int size);
void librsu_cb_buf_cleanup(void);
int librsu_cb_buf(void *buf, int len);

#endif
