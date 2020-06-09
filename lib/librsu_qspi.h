/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_QSPI_H__
#define __LIBRSU_QSPI_H__

#include <linux/types.h>

/*
 * Predefined offsets within MTD device node for SPTx tables. By definition,
 * SPT0 it at the start of the MTD device node.
 */
#define SPT0_OFFSET 0
#define SPT1_OFFSET (32 * 1024)

#define SPT_MAGIC_NUMBER 0x57713427
#define SPT_VERSION 0
#define SPT_FLAG_RESERVED 1
#define SPT_FLAG_READONLY 2

#define SPT_MAX_PARTITIONS 127

struct SUB_PARTITION_TABLE {
	__s32 magic_number;
	__s32 version;
	__s32 partitions;
	__s32 RSVD[5];

	struct {
		char name[16];
		__s64 offset;
		__s32 length;
		__s32 flags;
	} partition[SPT_MAX_PARTITIONS];
} __attribute__((__packed__));

#define CPB_MAGIC_NUMBER 0x57789609
#define CPB_HEADER_SIZE 24

union CMF_POINTER_BLOCK {
	struct {
		__s32 magic_number;
		__s32 header_size;
		__s32 cpb_size;
		__s32 cpb_backup_offset;
		__s32 image_ptr_offset;
		__s32 image_ptr_slots;
	} header;
	char data[4 * 1024];
} __attribute__((__packed__));

typedef __s64 CMF_POINTER;

#endif
