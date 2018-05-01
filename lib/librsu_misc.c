// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <fcntl.h>
#include "librsu_cfg.h"
#include "librsu_misc.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DEFAULT_SYS_DRIVER
#define DEFAULT_SYS_DRIVER "/sys/bus/platform/drivers/intel-rsu/"
#endif

static char *reserved_names[] = {
	"BOOT_INFO",
	"FACTORY_IMAGE",
	"SPT",
	"SPT0",
	"SPT1",
	"CPB",
	"CPB0",
	"CPB1",
	""	/* Terminating table entry */
};

int librsu_misc_is_rsvd_name(char *name)
{
	int x;

	for (x = 0; reserved_names[x][0] != '\0'; x++)
		if (strcmp(name, reserved_names[x]) == 0)
			return 1;

	return 0;
}

int librsu_misc_is_slot(struct librsu_ll_intf *ll_intf, int part_num)
{
	if (ll_intf->partition.readonly(part_num) ||
	    ll_intf->partition.reserved(part_num))
		return 0;

	if (librsu_misc_is_rsvd_name(ll_intf->partition.name(part_num)))
		return 0;

	return 1;
}

int librsu_misc_slot2part(struct librsu_ll_intf *ll_intf, int slot)
{
	int partitions;
	int cnt = 0;
	int x;

	partitions = ll_intf->partition.count();

	for (x = 0; x < partitions; x++) {
		if (librsu_misc_is_slot(ll_intf, x)) {
			if (slot == cnt)
				return x;
			cnt++;
		}
	}

	return -1;
}

int librsu_misc_get_devattr(char *attr, __u64 *value)
{
	FILE *attr_file;
	char buf[256];

	SAFE_STRCPY(buf, sizeof(buf), DEFAULT_SYS_DRIVER,
		    sizeof(DEFAULT_SYS_DRIVER));
	strcat(buf, librsu_cfg_get_rsu_dev());
	strcat(buf, "/");
	strcat(buf, attr);

	attr_file = fopen(buf, "r");
	if (!attr_file) {
		librsu_log(LOW, __func__,
			   "error: Unable to open device attribute file '%s'",
			   buf);
		return -1;
	}

	if (fgets(buf, sizeof(buf), attr_file)) {
		*value = strtol(buf, NULL, 10);
		fclose(attr_file);
		return 0;
	}

	fclose(attr_file);
	return -1;
}

int librsu_misc_put_devattr(char *attr, __u64 value)
{
	FILE *attr_file;
	char buf[256];

	SAFE_STRCPY(buf, sizeof(buf), DEFAULT_SYS_DRIVER,
		    sizeof(DEFAULT_SYS_DRIVER));
	strcat(buf, librsu_cfg_get_rsu_dev());
	strcat(buf, "/");
	strcat(buf, attr);

	attr_file = fopen(buf, "w");
	if (!attr_file) {
		librsu_log(LOW, __func__,
			   "error: Unable to open device attribute file '%s'",
			   buf);
		return -1;
	}

	sprintf(buf, "%lli", value);

	if (fputs(buf, attr_file) > 0) {
		fclose(attr_file);
		return 0;
	}

	fclose(attr_file);
	return -1;
}
