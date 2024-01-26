/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_CFG_H__
#define __LIBRSU_CFG_H__

#include <stdbool.h>
#include <stdio.h>
#include "librsu_ll.h"

void SAFE_STRCPY(char *dst, int dsz, char *src, int ssz);

void librsu_cfg_reset(void);
int librsu_cfg_parse(FILE *input);

enum RSU_LOG_LEVEL { OFF = 0, LOW, MED, HIGH };

void librsu_log(const enum RSU_LOG_LEVEL level, const char *func,
		const char *format, ...);

enum RSU_LL_TYPE { INVALID = 0, DATAFILE, QSPI, NAND, SDMMC };

enum RSU_LL_TYPE librsu_cfg_get_roottype(void);

int librsu_cfg_get_rootpath(struct spi_flash_info *flash_info);

void librsu_cfg_parse_rootpath(char *rootpath);

char *librsu_cfg_get_rsu_dev(void);

int librsu_cfg_writeprotected(int slot);

int librsu_cfg_spt_checksum_enabled(void);
#endif
