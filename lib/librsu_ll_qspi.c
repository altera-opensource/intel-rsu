// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <errno.h>
#include <fcntl.h>
#include "librsu_cfg.h"
#include "librsu_ll.h"
#include "librsu_qspi.h"
#include <mtd/mtd-user.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

static int dev_file = -1;
static struct mtd_info_user dev_info;

static int read_dev(off_t offset, void *buf, int len)
{
	int cnt = 0;
	char *ptr = buf;
	int rtn;

	if (dev_file < 0)
		return -1;

	if (lseek(dev_file, offset, SEEK_SET) != offset)
		return -1;

	while (cnt < len) {
		rtn = read(dev_file, ptr, len - cnt);

		if (rtn < 0) {
			librsu_log(LOW, __func__,
				   "error: Read error (errno=%i)", errno);
			return -1;
		}

		cnt += rtn;
		ptr += rtn;
	}

	return 0;
}

static int write_dev(off_t offset, void *buf, int len)
{
	int cnt = 0;
	char *ptr = buf;
	int rtn;

	if (dev_file < 0)
		return -1;

	if (lseek(dev_file, offset, SEEK_SET) != offset)
		return -1;

	while (cnt < len) {
		rtn = write(dev_file, ptr, len - cnt);

		if (rtn < 0) {
			librsu_log(LOW, __func__,
				   "error: Write error (errno=%i)", errno);
			return -1;
		}

		cnt += rtn;
		ptr += rtn;
	}

	return 0;
}

/*
 * Simulate a flash erase on a datafile by overwriting area with fill data.
 * This is not performed on an MTD device. It is called when erasesize == 0.
 */
static int erase_with_fill(off_t offset, int len)
{
	char fill[4 * 1024];
	int cnt;

	if (lseek(dev_file, offset, SEEK_SET) != offset)
		return -1;

	memset(fill, 0xff, sizeof(fill));

	for (cnt = 0; cnt < len; cnt += sizeof(fill)) {
		if (write_dev(offset + cnt, fill, sizeof(fill)))
			return -1;
	}

	return 0;
}

static int erase_dev(off_t offset, int len)
{
	struct erase_info_user erase;
	int rtn;

	if (dev_file < 0)
		return -1;

	if (dev_info.erasesize == 0)
		return erase_with_fill(offset, len);

	if (offset % dev_info.erasesize) {
		librsu_log(LOW, __func__,
			   "error: Erase offset 0x08%x not erase block aligned",
			   offset);
		return -1;
	}

	if (len % dev_info.erasesize) {
		librsu_log(LOW, __func__,
			   "error: Erase length %i not erase block aligned",
			   offset);
		return -1;
	}

	erase.start = offset;
	erase.length = len;

	rtn = ioctl(dev_file, MEMERASE, &erase);

	if (rtn < 0) {
		librsu_log(LOW, __func__, "error: Erase error (errno=%i)",
			   errno);
		return -1;
	}

	return 0;
}

static struct SUB_PARTITION_TABLE spt;
static __u64 spt0_offset;

/**
 * The SPT offset entry is the partition offset within the flash.  The MTD
 * device node maps a region starting with SPT0 which is not at the beginning
 * of flash. This is done so that data below the SPT0 in flash is not
 * exposed to librsu.  This function finds the partition offset within
 * the device file.
 */
static int get_part_offset(int part_num, off_t *offset)
{
	if (part_num < 0 || part_num >= spt.partitions || spt0_offset == 0)
		return -1;

	if (spt.partition[part_num].offset < (__s64)spt0_offset)
		return -1;

	*offset = (off_t)(spt.partition[part_num].offset - spt0_offset);

	return 0;
}

/**
 * Make sure the SPT names are '\0' terminated. Truncate last byte if the
 * name uses all available bytes.  Perform validity check on entries.
 */
static int check_spt(void)
{
	int x;
	unsigned int max_len = sizeof(spt.partition[0].name);

	int spt0_found = 0;
	int spt1_found = 0;
	int cpb0_found = 0;
	int cpb1_found = 0;

	librsu_log(HIGH, __func__, "MAX length of a name = %i bytes",
		   max_len - 1);

	if (spt.version > SPT_VERSION) {
		librsu_log(LOW, __func__,
			   "warning: SPT version %i is greater than %i",
			   spt.version, SPT_VERSION);
		librsu_log(LOW, __func__,
			   "LIBRSU Version %i - update to enable newer features",
			   LIBRSU_VER);
	}

	for (x = 0; x < spt.partitions; x++) {
		if (strnlen(spt.partition[x].name, max_len) >= max_len)
			spt.partition[x].name[max_len - 1] = '\0';

		librsu_log(HIGH, __func__, "%-16s %016llX - %016llX (%X)",
			   spt.partition[x].name, spt.partition[x].offset,
			   (spt.partition[x].offset + spt.partition[x].length -
			    1), spt.partition[x].flags);

		if (strcmp(spt.partition[x].name, "SPT0") == 0)
			spt0_found = 1;
		else if (strcmp(spt.partition[x].name, "SPT1") == 0)
			spt1_found = 1;
		else if (strcmp(spt.partition[x].name, "CPB0") == 0)
			cpb0_found = 1;
		else if (strcmp(spt.partition[x].name, "CPB1") == 0)
			cpb1_found = 1;
	}

	if (!spt0_found || !spt1_found || !cpb0_found || !cpb1_found) {
		librsu_log(MED, __func__,
			   "Missing a critical entry in the SPT");
		return -1;
	}

	return 0;
}

static int load_spt0_offset(void)
{
	int x;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "SPT0") == 0) {
			spt0_offset = spt.partition[x].offset;
			return 0;
		}
	}

	librsu_log(MED, __func__, "SPT0 entry not found in table");

	return -1;
}

/**
 * Check SPT1 and then SPT0. If they both pass checks, use SPT0.
 * If only one passes, retore the bad one. If both are bad, fail.
 */
static int load_spt(void)
{
	int spt0_good = 0;
	int spt1_good = 0;
	spt0_offset = 0;

	librsu_log(HIGH, __func__, "SPT1");
	if (read_dev(SPT1_OFFSET, &spt, sizeof(spt)) == 0 &&
	    spt.magic_number == SPT_MAGIC_NUMBER) {
		if (check_spt() == 0 && load_spt0_offset() == 0)
			spt1_good = 1;
		else
			librsu_log(MED, __func__, "SPT1 validity check failed");
	} else {
		librsu_log(MED, __func__, "Bad SPT1 magic number 0x%08X",
			   spt.magic_number);
	}

	librsu_log(HIGH, __func__, "SPT0");
	if (read_dev(SPT0_OFFSET, &spt, sizeof(spt)) == 0 &&
	    spt.magic_number == SPT_MAGIC_NUMBER) {
		if (check_spt() == 0 && load_spt0_offset() == 0)
			spt0_good = 1;
		else
			librsu_log(MED, __func__, "SPT0 validitiy check fail");
	} else {
		librsu_log(MED, __func__, "Bad SPT0 magic number 0x%08X",
			   spt.magic_number);
	}

	if (spt0_good && spt1_good)
		return 0;

	if (spt0_good) {
		librsu_log(LOW, __func__, "warning: Restoring SPT1");

		if (erase_dev(SPT1_OFFSET, 32 * 1024)) {
			librsu_log(LOW, __func__,
				   "error: Erase SPT1 region failed");
			return -1;
		}

		spt.magic_number = (__s32)0xFFFFFFFF;
		if (write_dev(SPT1_OFFSET, &spt, sizeof(spt))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write SPT1 table");
			return -1;
		}

		spt.magic_number = (__s32)SPT_MAGIC_NUMBER;
		if (write_dev(SPT1_OFFSET, &spt, sizeof(spt.magic_number))) {
			librsu_log(LOW, __func__,
				   "error: Unable to wr SPT1 magic #");
			return -1;
		}

		return 0;
	}

	if (spt1_good) {
		if (read_dev(SPT1_OFFSET, &spt, sizeof(spt)) ||
		    spt.magic_number != SPT_MAGIC_NUMBER ||
		    check_spt() || load_spt0_offset()) {
			librsu_log(MED, __func__, "error: Failed to load SPT1");
			return -1;
		}

		librsu_log(LOW, __func__, "warning: Restoring SPT0");

		if (erase_dev(SPT0_OFFSET, 32 * 1024)) {
			librsu_log(LOW, __func__,
				   "error: Erase SPT0 region failed");
			return -1;
		}

		spt.magic_number = (__s32)0xFFFFFFFF;
		if (write_dev(SPT0_OFFSET, &spt, sizeof(spt))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write SPT0 table");
			return -1;
		}

		spt.magic_number = (__s32)SPT_MAGIC_NUMBER;
		if (write_dev(SPT0_OFFSET, &spt, sizeof(spt.magic_number))) {
			librsu_log(LOW, __func__,
				   "error: Unable to wr SPT0 magic #");
			return -1;
		}

		return 0;
	}

	librsu_log(LOW, __func__, "error: No valid SPT0 or SPT1 found");
	return -1;
}

static int read_part(int part_num, int offset, void *buf, int len)
{
	off_t part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;

	if (offset < 0 || len < 0 ||
	    (offset + len) > spt.partition[part_num].length)
		return -1;

	return read_dev(part_offset + (off_t)offset, buf, len);
}

static int write_part(int part_num, int offset, void *buf, int len)
{
	off_t part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;

	if (offset < 0 || len < 0 ||
	    (offset + len) > spt.partition[part_num].length)
		return -1;

	return write_dev(part_offset + (off_t)offset, buf, len);
}

static int erase_part(int part_num)
{
	off_t part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;

	return erase_dev(part_offset, spt.partition[part_num].length);
}

static int writeback_spt(void)
{
	int x;
	int updates = 0;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "SPT0") &&
		    strcmp(spt.partition[x].name, "SPT1"))
			continue;

		if (erase_part(x)) {
			librsu_log(LOW, __func__,
				   "error: Unable to ease SPTx");
			return -1;
		}

		spt.magic_number = (__s32)0xFFFFFFFF;
		if (write_part(x, 0, &spt, sizeof(spt))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write SPTx table");
			return -1;
		}
		spt.magic_number = (__s32)SPT_MAGIC_NUMBER;
		if (write_part(x, 0, &spt, sizeof(spt.magic_number))) {
			librsu_log(LOW, __func__,
				   "error: Unable to wr SPTx magic #");
			return -1;
		}

		updates++;
	}

	if (updates != 2) {
		librsu_log(LOW, __func__, "error: Did not find two SPTs");
		return -1;
	}

	return 0;
}

static union CMF_POINTER_BLOCK cpb;
static CMF_POINTER *cpb_slots;

#define ERASED_ENTRY ((__s64)-1)
#define SPENT_ENTRY ((__s64)0)

static int check_cpb(void)
{
	int x, y;

	if (cpb.header.header_size > CPB_HEADER_SIZE) {
		librsu_log(LOW, __func__,
			   "warning: CPB header is larger than expected");
		librsu_log(LOW, __func__,
			   "LIBRSU Version %i - update to enable newer features",
			   LIBRSU_VER);
	}

	for (x = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] != ERASED_ENTRY &&
		    cpb_slots[x] != SPENT_ENTRY) {
			for (y = 0; y < spt.partitions; y++) {
				if (cpb_slots[x] == spt.partition[y].offset) {
					librsu_log(HIGH, __func__,
						   "cpb_slots[%i] = %s", x,
						   spt.partition[y].name);
					break;
				}
			}
			if (y >= spt.partitions)
				librsu_log(HIGH, __func__,
					   "cpb_slots[%i] = %016llX ???", x,
					   cpb_slots[x]);
		}
	}

	return 0;
}

/**
 * Check CPB1 and then CPB0. If they both pass checks, use CPB0.
 * If only one passes, retore the bad one. If both are bad, fail.
 */
static int load_cpb(void)
{
	int x;
	int cpb0_part = -1;
	int cpb0_good = 0;
	int cpb1_part = -1;
	int cpb1_good = 0;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "CPB0") == 0)
			cpb0_part = x;
		else if (strcmp(spt.partition[x].name, "CPB1") == 0)
			cpb1_part = x;

		if (cpb0_part >= 0 && cpb1_part >= 0)
			break;
	}

	if (cpb0_part < 0 || cpb1_part < 0) {
		librsu_log(LOW, __func__, "error: Missing CPB0/1 partition");
		return -1;
	}

	librsu_log(HIGH, __func__, "CPB1");
	if (read_part(cpb1_part, 0, &cpb, sizeof(cpb)) == 0 &&
	    cpb.header.magic_number == CPB_MAGIC_NUMBER) {
		cpb_slots = (CMF_POINTER *)
			&cpb.data[cpb.header.image_ptr_offset];
		if (check_cpb() == 0)
			cpb1_good = 1;
	} else {
		librsu_log(MED, __func__, "Bad CPB1 is bad");
	}

	librsu_log(HIGH, __func__, "CPB0");
	if (read_part(cpb0_part, 0, &cpb, sizeof(cpb)) == 0 &&
	    cpb.header.magic_number == CPB_MAGIC_NUMBER) {
		cpb_slots = (CMF_POINTER *)
			&cpb.data[cpb.header.image_ptr_offset];
		if (check_cpb() == 0)
			cpb0_good = 1;
	} else {
		librsu_log(MED, __func__, "Bad CPB0 is bad");
	}

	if (cpb0_good && cpb1_good) {
		cpb_slots = (CMF_POINTER *)
		    &cpb.data[cpb.header.image_ptr_offset];
		return 0;
	}

	if (cpb0_good) {
		librsu_log(LOW, __func__, "warning: Restoring CPB1");
		if (erase_part(cpb1_part)) {
			librsu_log(LOW, __func__,
				   "error: Failed erase CPB1");
			return -1;
		}

		cpb.header.magic_number = (__s32)0xFFFFFFFF;
		if (write_part(cpb1_part, 0, &cpb, sizeof(cpb))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write CPB1 table");
			return -1;
		}
		cpb.header.magic_number = (__s32)CPB_MAGIC_NUMBER;
		if (write_part(cpb1_part, 0, &cpb,
			       sizeof(cpb.header.magic_number))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write CPB1 magic number");
			return -1;
		}

		cpb_slots = (CMF_POINTER *)
		    &cpb.data[cpb.header.image_ptr_offset];
		return 0;
	}

	if (cpb1_good) {
		if (read_part(cpb1_part, 0, &cpb, sizeof(cpb)) ||
		    cpb.header.magic_number != CPB_MAGIC_NUMBER) {
			librsu_log(MED, __func__, "error: Unable to load CPB1");
			return -1;
		}

		librsu_log(LOW, __func__, "warning: Restoring CPB0");
		if (erase_part(cpb0_part)) {
			librsu_log(LOW, __func__,
				   "error: Failed erase CPB0");
			return -1;
		}

		cpb.header.magic_number = (__s32)0xFFFFFFFF;
		if (write_part(cpb0_part, 0, &cpb, sizeof(cpb))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write CPB0 table");
			return -1;
		}
		cpb.header.magic_number = (__s32)CPB_MAGIC_NUMBER;
		if (write_part(cpb0_part, 0, &cpb,
			       sizeof(cpb.header.magic_number))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write CPB0 magic number");
			return -1;
		}

		cpb_slots = (CMF_POINTER *)
		    &cpb.data[cpb.header.image_ptr_offset];
		return 0;
	}

	librsu_log(LOW, __func__, "error: No valid CPB0 or CPB1 found");

	return -1;
}

static int update_cpb(int slot, __s64 ptr)
{
	int x;
	int updates = 0;

	if (slot < 0 || slot > cpb.header.image_ptr_slots)
		return -1;

	if ((cpb_slots[slot] & ptr) != ptr)
		return -1;

	cpb_slots[slot] = ptr;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "CPB0") &&
		    strcmp(spt.partition[x].name, "CPB1"))
			continue;

		if (write_part(x, 0, &cpb, sizeof(cpb)))
			return -1;
		updates++;
	}

	if (updates != 2) {
		librsu_log(LOW, __func__, "error: Did not find two CPBs");
		return -1;
	}

	return 0;
}

static int writeback_cpb(void)
{
	int x;
	int updates = 0;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "CPB0") &&
		    strcmp(spt.partition[x].name, "CPB1"))
			continue;

		if (erase_part(x)) {
			librsu_log(LOW, __func__,
				   "error: Unable to ease CPBx");
			return -1;
		}

		cpb.header.magic_number = (__s32)0xFFFFFFFF;
		if (write_part(x, 0, &cpb, sizeof(cpb))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write CPBx table");
			return -1;
		}
		cpb.header.magic_number = (__s32)CPB_MAGIC_NUMBER;
		if (write_part(x, 0, &cpb, sizeof(cpb.header.magic_number))) {
			librsu_log(LOW, __func__,
				   "error: Unable to write CPBx magic number");
			return -1;
		}

		updates++;
	}

	if (updates != 2) {
		librsu_log(LOW, __func__, "error: Did not find two CPBs");
		return -1;
	}

	return 0;
}

static void ll_close(void)
{
	if (dev_file >= 0)
		close(dev_file);

	dev_file = -1;
	spt0_offset = 0;
	spt.partitions = 0;
	cpb.header.image_ptr_slots = 0;
}

static int partition_count(void)
{
	return spt.partitions;
}

static char *partition_name(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return "BAD";

	return spt.partition[part_num].name;
}

static __s64 partition_offset(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	return spt.partition[part_num].offset;
}

static int partition_size(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	return spt.partition[part_num].length;
}

static int partition_reserved(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return 0;

	return (spt.partition[part_num].flags & SPT_FLAG_RESERVED) ? 1 : 0;
}

static int partition_readonly(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return 0;

	return (spt.partition[part_num].flags & SPT_FLAG_READONLY) ? 1 : 0;
}

static int priority_get(int part_num)
{
	int x;
	int priority = 0;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	for (x = cpb.header.image_ptr_slots; x > 0; x--) {
		if (cpb_slots[x - 1] != ERASED_ENTRY &&
		    cpb_slots[x - 1] != SPENT_ENTRY) {
			priority++;
			if (cpb_slots[x - 1] == spt.partition[part_num].offset)
				return priority;
		}
	}

	return 0;
}

static int priority_add(int part_num)
{
	int x;
	int y;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	for (x = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] == ERASED_ENTRY) {
			if (update_cpb(x, spt.partition[part_num].offset)) {
				load_cpb();
				return -1;
			}
			return load_cpb();
		}
	}

	librsu_log(MED, __func__, "Compressing CPB");

	for (x = 0, y = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] != ERASED_ENTRY &&
		    cpb_slots[x] != SPENT_ENTRY) {
			cpb_slots[y++] = cpb_slots[x];
		}
	}

	if (y < cpb.header.image_ptr_slots)
		cpb_slots[y++] = spt.partition[part_num].offset;
	else
		return -1;

	while (y < cpb.header.image_ptr_slots)
		cpb_slots[y++] = ERASED_ENTRY;

	if (writeback_cpb() || load_cpb())
		return -1;

	return 0;
}

static int priority_remove(int part_num)
{
	int x;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	for (x = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] == spt.partition[part_num].offset)
			if (update_cpb(x, SPENT_ENTRY)) {
				load_cpb();
				return -1;
			}
	}

	return load_cpb();
}

static int data_read(int part_num, int offset, int bytes, void *buf)
{
	return read_part(part_num, offset, buf, bytes);
}

static int data_write(int part_num, int offset, int bytes, void *buf)
{
	return write_part(part_num, offset, buf, bytes);
}

static int data_erase(int part_num)
{
	return erase_part(part_num);
}

static int partition_rename(int part_num, char *name)
{
	int x;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	if (strnlen(name, sizeof(spt.partition[0].name)) >=
	    sizeof(spt.partition[0].name)) {
		librsu_log(LOW, __func__,
			   "error: Partition name is too long - limited to %i",
			   sizeof(spt.partition[0].name) - 1);
		return -1;
	}

	for (x = 0; x < spt.partitions; x++) {
		if (strncmp(spt.partition[x].name, name,
			    sizeof(spt.partition[0].name) - 1) == 0) {
			librsu_log(LOW, __func__,
				   "error: Partition rename already in use");
			return -1;
		}
	}

	SAFE_STRCPY(spt.partition[part_num].name, sizeof(spt.partition[0].name),
		    name, sizeof(spt.partition[0].name));

	if (writeback_spt())
		return -1;

	if (load_spt())
		return -1;

	return 0;
}

/*
 * partition_delete() - Delete a partition.
 * part_num: partition number
 *
 * Returns 0 on success, or Error Code
 */
static int partition_delete(int part_num)
{
	int x;

	if (part_num < 0 || part_num >= spt.partitions) {
		librsu_log(LOW, __func__,
			   "error: Invalid partition number");
		return -1;
	}

	for (x = part_num; x < spt.partitions; x++)
		spt.partition[x] = spt.partition[x + 1];

	spt.partitions--;

	if (writeback_spt())
		return -1;

	if (load_spt())
		return -1;

	return 0;
}

/*
 * partition_create() - Create a new partition.
 * name: partition name
 * start: partition start address
 * size: partition size
 *
 * Returns 0 on success, or Error Code
 */
static int partition_create(char *name, __u64 start, unsigned int size)
{
	int x;
	__u64 end = start + size;

	if (size % dev_info.erasesize) {
		librsu_log(LOW, __func__, "error: Invalid partition size");
		return -1;
	}

	if (start % dev_info.erasesize) {
		librsu_log(LOW, __func__, "error: Invalid partition address");
		return -1;
	}

	if (strnlen(name, sizeof(spt.partition[0].name)) >=
	    sizeof(spt.partition[0].name)) {
		librsu_log(LOW, __func__,
			   "error: Partition name is too long - limited to %i",
			   sizeof(spt.partition[0].name) - 1);
		return -1;
	}

	for (x = 0; x < spt.partitions; x++) {
		if (strncmp(spt.partition[x].name, name,
			    sizeof(spt.partition[0].name) - 1) == 0) {
			librsu_log(LOW, __func__,
				   "error: Partition name already in use");
			return -1;
		}
	}

	if (spt.partitions == SPT_MAX_PARTITIONS) {
		librsu_log(LOW, __func__, "error: Partition table is full");
		return -1;
	}

	for (x = 0; x < spt.partitions; x++) {
		__u64 pstart = spt.partition[x].offset;
		__u64 pend = spt.partition[x].offset +
			     spt.partition[x].length;

		if ((start < pend) && (end > pstart)) {
			librsu_log(LOW, __func__, "error: Partition overlap");
			return -1;
		}
	}

	SAFE_STRCPY(spt.partition[spt.partitions].name,
		    sizeof(spt.partition[0].name), name,
		    sizeof(spt.partition[0].name));
	spt.partition[spt.partitions].offset = start;
	spt.partition[spt.partitions].length = size;
	spt.partition[spt.partitions].flags = 0;

	spt.partitions++;

	if (writeback_spt())
		return -1;

	if (load_spt())
		return -1;

	return 0;
}

static struct librsu_ll_intf qspi_ll_intf = {
	.close = ll_close,

	.partition.count = partition_count,
	.partition.name = partition_name,
	.partition.offset = partition_offset,
	.partition.size = partition_size,
	.partition.reserved = partition_reserved,
	.partition.readonly = partition_readonly,
	.partition.rename = partition_rename,
	.partition.delete = partition_delete,
	.partition.create = partition_create,

	.priority.get = priority_get,
	.priority.add = priority_add,
	.priority.remove = priority_remove,

	.data.read = data_read,
	.data.write = data_write,
	.data.erase = data_erase
};

int librsu_ll_open_qspi(struct librsu_ll_intf **intf)
{
	char *type_str;
	char *rootpath = librsu_cfg_get_rootpath();

	if (!rootpath) {
		librsu_log(LOW, __func__, "error: No root specified");
		return -1;
	}

	dev_file = open(rootpath, O_RDWR | O_SYNC);

	if (dev_file < 0) {
		librsu_log(LOW, __func__, "error: Unable to open '%s'",
			   rootpath);
		return -1;
	}

	if (ioctl(dev_file, MEMGETINFO, &dev_info)) {
		librsu_log(LOW, __func__,
			   "error: Unable to find mtd info for '%s'",
			   librsu_cfg_get_rootpath());
		ll_close();
		return -1;
	}

	if (dev_info.type == MTD_NORFLASH)
		type_str = "NORFLASH";
	else if (dev_info.type == MTD_NANDFLASH)
		type_str = "NANDFLASH";
	else if (dev_info.type == MTD_RAM)
		type_str = "RAM";
	else if (dev_info.type == MTD_ROM)
		type_str = "ROM";
	else if (dev_info.type == MTD_DATAFLASH)
		type_str = "DATAFLASH";
	else if (dev_info.type == MTD_UBIVOLUME)
		type_str = "UBIVOLUME";
	else
		type_str = "[UNKNOWN]";

	librsu_log(HIGH, __func__, "MTD flash type is (%i) %s",
		   dev_info.type, type_str);
	librsu_log(HIGH, __func__, "MTD flash size = %i", dev_info.size);
	librsu_log(HIGH, __func__, "MTD flash erase size = %i",
		   dev_info.erasesize);
	librsu_log(HIGH, __func__, "MTD flash write size = %i",
		   dev_info.writesize);

	if (dev_info.flags & MTD_WRITEABLE)
		librsu_log(HIGH, __func__, "MTD flash is MTD_WRITEABLE");

	if (dev_info.flags & MTD_BIT_WRITEABLE)
		librsu_log(HIGH, __func__,
			   "MTD flash is MTD_BIT_WRITEABLE");

	if (dev_info.flags & MTD_NO_ERASE)
		librsu_log(HIGH, __func__, "MTD flash is MTD_NO_ERASE");

	if (dev_info.flags & MTD_POWERUP_LOCK)
		librsu_log(HIGH, __func__, "MTD flash is MTD_POWERUP_LOCK");

	if (load_spt()) {
		librsu_log(LOW, __func__, "error: Bad SPT");
		ll_close();
		return -1;
	}

	if (load_cpb()) {
		librsu_log(LOW, __func__, "error: Bad CPB");
		ll_close();
		return -1;
	}

	*intf = &qspi_ll_intf;

	return 0;
}

/**
 * The datafile root type is for testing the QSPI code using a regular file as
 * the data source.  Erase ops just write FF's to the file with no IOCTL needed.
 */
int librsu_ll_open_datafile(struct librsu_ll_intf **intf)
{
	char *rootpath = librsu_cfg_get_rootpath();

	if (!rootpath) {
		librsu_log(LOW, __func__, "error: No root specified");
		return -1;
	}

	dev_file = open(rootpath, O_RDWR | O_SYNC);

	if (dev_file < 0) {
		librsu_log(LOW, __func__,
			   "error: Unable to open dev_file '%s'", rootpath);
		return -1;
	}

	dev_info.type = MTD_ABSENT;
	dev_info.erasesize = 0;
	dev_info.writesize = 1;
	dev_info.oobsize = 0;

	if (load_spt()) {
		librsu_log(LOW, __func__, "error: Bad SPT in dev_file '%s'",
			   librsu_cfg_get_rootpath());
		ll_close();
		return -1;
	}

	if (load_cpb()) {
		librsu_log(LOW, __func__, "error: Bad CPB in dev_file '%s'",
			   librsu_cfg_get_rootpath());
		ll_close();
		return -1;
	}

	*intf = &qspi_ll_intf;

	return 0;
}
