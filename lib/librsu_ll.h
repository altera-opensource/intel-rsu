/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_LL_H__
#define __LIBRSU_LL_H__

#include <linux/types.h>
#include <mtd/mtd-user.h>

/* Maximum number of QSPI flash supported */
#define QSPI_MAX_DEVICE 4

/* struct for multiflash */
struct spi_flash_list {
	int dev_file[QSPI_MAX_DEVICE];
	struct mtd_info_user dev_info[QSPI_MAX_DEVICE];
	int flash_count;
};

struct spi_flash_info {
	int flash_index[QSPI_MAX_DEVICE];
	char *root_path[QSPI_MAX_DEVICE];
};

struct librsu_ll_intf {
	void (*close)(void);

	struct {
		int (*count)(void);
		char *(*name)(int part_num);
		__s64 (*offset)(int part_num);
		__s64 (*factory_offset)(void);
		int (*size)(int part_num);
		int (*reserved)(int part_num);
		int (*readonly)(int part_num);
		int (*rename)(int part_num, char *name);
		int (*delete)(int part_num);
		int (*create)(char *name, __u64 start, unsigned int size);
	} partition;

	struct {
		int (*get)(int part_num);
		int (*add)(int part_num);
		int (*remove)(int part_num);
	} priority;

	struct {
		int (*read)(int part_num, int offset, int bytes, void *buf);
		int (*write)(int part_num, int offset, int bytes, void *buf);
		int (*erase)(int part_num);
	} data;

	struct {
		int (*restore)(char *name);
		int (*save)(char *name);
		int (*corrupted)(void);
	} spt_ops;

	struct {
		int (*empty)(void);
		int (*restore)(char *name);
		int (*save)(char *name);
		int (*corrupted)(void);
	} cpb_ops;

	struct spi_flash_list flash_list;
	struct spi_flash_info flash_info;
};

int librsu_ll_open_datafile(struct librsu_ll_intf **intf);
int librsu_ll_open_qspi(struct librsu_ll_intf **intf);

#endif
