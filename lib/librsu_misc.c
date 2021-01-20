// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <fcntl.h>
#include "librsu_cfg.h"
#include "librsu_misc.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
	char *buf;
	int size = 256;

	buf = (char *)calloc(1, size);
	if (!buf) {
		librsu_log(LOW, __func__,
			   "error: failed to alloc buf\n");
		return -1;
	}

	strncat(buf, librsu_cfg_get_rsu_dev(), size - strlen(buf) - 1);
	strncat(buf, "/", size - strlen(buf) - 1);
	strncat(buf, attr, size - strlen(buf) - 1);

	attr_file = fopen(buf, "r");
	if (!attr_file) {
		librsu_log(LOW, __func__,
			   "error: Unable to open device attribute file '%s'",
			   buf);
		free(buf);
		return -1;
	}

	if (fgets(buf, size, attr_file)) {
		*value = strtol(buf, NULL, 0);
		fclose(attr_file);
		free(buf);
		return 0;
	}

	fclose(attr_file);
	free(buf);
	return -1;
}

int librsu_misc_put_devattr(char *attr, __u64 value)
{
	FILE *attr_file;
	char *buf;
	int size = 256;

	buf = (char *)calloc(1, size);
	if (!buf) {
		librsu_log(LOW, __func__,
			   "error: failed to alloc buf\n");
		return -1;
	}

	strncat(buf, librsu_cfg_get_rsu_dev(), size - strlen(buf) - 1);
	strncat(buf, "/", size - strlen(buf) - 1);
	strncat(buf, attr, size - strlen(buf) - 1);

	attr_file = fopen(buf, "w");
	if (!attr_file) {
		librsu_log(LOW, __func__,
			   "error: Unable to open device attribute file '%s'",
			   buf);
		free(buf);
		return -1;
	}

	snprintf(buf, size, "%lli", value);

	if (fputs(buf, attr_file) > 0) {
		fclose(attr_file);
		free(buf);
		return 0;
	}

	fclose(attr_file);
	free(buf);
	return -1;
}
