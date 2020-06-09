// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <fcntl.h>
#include "librsu_cb.h"
#include "librsu_cfg.h"
#include <librsu.h>
#include "librsu_image.h"
#include "librsu_ll.h"
#include "librsu_misc.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DEFAULT_CFG_FILENAME
#define DEFAULT_CFG_FILENAME "/etc/librsu.rc"
#endif


#define RSU_NOTIFY_RESET_RETRY_COUNTER  (1 << 16)
#define RSU_NOTIFY_CLEAR_ERROR_STATUS   (1 << 17)
#define RSU_NOTIFY_IGNORE_STAGE         (1 << 18)
#define RSU_NOTIFY_VALUE_MASK           0xFFFF

static struct librsu_ll_intf *ll_intf;

int librsu_init(char *filename)
{
	FILE *cfg_file;
	char *cfg_filename;
	int rtn;

	if (ll_intf) {
		fprintf(stderr,
			"librsu: %s(): error: Library already initialized\n",
			__func__);
		return -ELIB;
	}

	if (!filename || filename[0] == '\0')
		cfg_filename = DEFAULT_CFG_FILENAME;
	else
		cfg_filename = filename;

	cfg_file = fopen(cfg_filename, "r");
	if (!cfg_file) {
		fprintf(stderr,
			"librsu: %s(): error: Unable to open cfg file '%s'\n",
			__func__, cfg_filename);
		return -EFILEIO;
	}

	librsu_cfg_reset();
	rtn = librsu_cfg_parse(cfg_file);
	fclose(cfg_file);

	if (rtn)
		return -ECFG;

	switch (librsu_cfg_get_roottype()) {
	case DATAFILE:
		rtn = librsu_ll_open_datafile(&ll_intf);
		break;
	case QSPI:
		rtn = librsu_ll_open_qspi(&ll_intf);
		break;
	default:
		rtn = -ECFG;
	}

	if (rtn) {
		librsu_exit();
		return -ECFG;
	}

	return 0;
}

void librsu_exit(void)
{
	if (ll_intf && ll_intf->close)
		ll_intf->close();

	ll_intf = NULL;

	librsu_cfg_reset();
}

int rsu_slot_count(void)
{
	int partitions;
	int cnt = 0;
	int x;

	if (!ll_intf)
		return -ELIB;

	partitions = ll_intf->partition.count();

	for (x = 0; x < partitions; x++) {
		if (librsu_misc_is_slot(ll_intf, x))
			cnt++;
	}

	return cnt;
}

int rsu_slot_by_name(char *name)
{
	int partitions;
	int x;
	int cnt = 0;

	if (!ll_intf)
		return -ELIB;

	if (!name)
		return -EARGS;

	partitions = ll_intf->partition.count();

	for (x = 0; x < partitions; x++) {
		if (librsu_misc_is_slot(ll_intf, x)) {
			if (!strcmp(name, ll_intf->partition.name(x)))
				return cnt;

			cnt++;
		}
	}

	return -ENAME;
}

int rsu_slot_get_info(int slot, struct rsu_slot_info *info)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	if (!info)
		return -EARGS;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	SAFE_STRCPY(info->name, sizeof(info->name),
		    ll_intf->partition.name(part_num), sizeof(info->name));

	info->offset = ll_intf->partition.offset(part_num);
	info->size = ll_intf->partition.size(part_num);
	info->priority = ll_intf->priority.get(part_num);

	return 0;
}

int rsu_slot_size(int slot)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	return ll_intf->partition.size(part_num);
}

int rsu_slot_priority(int slot)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	return ll_intf->priority.get(part_num);
}

int rsu_slot_erase(int slot)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	if (librsu_cfg_writeprotected(slot)) {
		librsu_log(HIGH, __func__,
			   "Trying to erase a write protected slot");
		return -EWRPROT;
	}

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	if (ll_intf->data.erase(part_num))
		return -ELOWLEVEL;

	return 0;
}

int rsu_slot_program_buf(int slot, void *buf, int size)
{
	int rtn;

	if (librsu_cb_buf_init(buf, size)) {
		librsu_log(HIGH, __func__, "Bad buf/size arguments");
		return -EARGS;
	}

	rtn = librsu_cb_program_common(ll_intf, slot, librsu_cb_buf, 0);

	librsu_cb_buf_cleanup();

	return rtn;
}

/*
 * This API was added to force users to use the updated image handling
 * algorithm, introduced at the same time, which deals properly with both
 * regular and factory update images.
 */
int rsu_slot_program_factory_update_buf(int slot, void *buf, int size)
{
	return rsu_slot_program_buf(slot, buf, size);
}

int rsu_slot_program_file(int slot, char *filename)
{
	int rtn;

	if (librsu_cb_file_init(filename)) {
		librsu_log(HIGH, __func__, "Unable to open file '%s'",
			   filename);
		return -EFILEIO;
	}

	rtn = librsu_cb_program_common(ll_intf, slot, librsu_cb_file, 0);

	librsu_cb_file_cleanup();

	return rtn;
}

/*
 * This API was added to force users to use the updated image handling
 * algorithm, introduced at the same time, which deals properly with both
 * regular and factory update images.
 */
int rsu_slot_program_factory_update_file(int slot, char *filename)
{
	return rsu_slot_program_file(slot, filename);
}

int rsu_slot_program_buf_raw(int slot, void *buf, int size)
{
	int rtn;

	if (librsu_cb_buf_init(buf, size)) {
		librsu_log(HIGH, __func__, "Bad buf/size arguments");
		return -EARGS;
	}

	rtn = librsu_cb_program_common(ll_intf, slot, librsu_cb_buf, 1);

	librsu_cb_buf_cleanup();

	return rtn;
}

int rsu_slot_program_file_raw(int slot, char *filename)
{
	int rtn;

	if (librsu_cb_file_init(filename)) {
		librsu_log(HIGH, __func__, "Unable to open file '%s'",
			   filename);
		return -EFILEIO;
	}

	rtn = librsu_cb_program_common(ll_intf, slot, librsu_cb_file, 1);

	librsu_cb_file_cleanup();

	return rtn;
}

int rsu_slot_verify_buf(int slot, void *buf, int size)
{
	int rtn;

	if (librsu_cb_buf_init(buf, size)) {
		librsu_log(HIGH, __func__, "Bad buf/size arguments");
		return -EARGS;
	}

	rtn = librsu_cb_verify_common(ll_intf, slot, librsu_cb_buf, 0);

	librsu_cb_buf_cleanup();

	return rtn;
}

int rsu_slot_verify_file(int slot, char *filename)
{
	int rtn;

	if (librsu_cb_file_init(filename)) {
		librsu_log(HIGH, __func__, "Unable to open file '%s'",
			   filename);
		return -EFILEIO;
	}

	rtn = librsu_cb_verify_common(ll_intf, slot, librsu_cb_file, 0);

	librsu_cb_file_cleanup();

	return rtn;
}

int rsu_slot_verify_buf_raw(int slot, void *buf, int size)
{
	int rtn;

	if (librsu_cb_buf_init(buf, size)) {
		librsu_log(HIGH, __func__, "Bad buf/size arguments");
		return -EARGS;
	}

	rtn = librsu_cb_verify_common(ll_intf, slot, librsu_cb_buf, 1);

	librsu_cb_buf_cleanup();

	return rtn;
}

int rsu_slot_verify_file_raw(int slot, char *filename)
{
	int rtn;

	if (librsu_cb_file_init(filename)) {
		librsu_log(HIGH, __func__, "Unable to open file '%s'",
			   filename);
		return -EFILEIO;
	}

	rtn = librsu_cb_verify_common(ll_intf, slot, librsu_cb_file, 1);

	librsu_cb_file_cleanup();

	return rtn;
}

int rsu_slot_program_callback(int slot, rsu_data_callback callback)
{
	return librsu_cb_program_common(ll_intf, slot, callback, 0);
}

int rsu_slot_program_callback_raw(int slot, rsu_data_callback callback)
{
	return librsu_cb_program_common(ll_intf, slot, callback, 1);
}

int rsu_slot_verify_callback(int slot, rsu_data_callback callback)
{
	return librsu_cb_verify_common(ll_intf, slot, callback, 0);
}

int rsu_slot_verify_callback_raw(int slot, rsu_data_callback callback)
{
	return librsu_cb_verify_common(ll_intf, slot, callback, 1);
}

int rsu_slot_copy_to_file(int slot, char *filename)
{
	int part_num;
	int df;
	int offset;
	char buf[0x1000];
	char fill[0x1000];
	int last_write;
	unsigned int x;

	if (!ll_intf)
		return -ELIB;

	if (!filename)
		return -EARGS;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.get(part_num) <= 0) {
		librsu_log(HIGH, __func__, "Trying to read an erased slot");
		return -EERASE;
	}

	df = open(filename, O_WRONLY | O_CREAT, 0600);
	if (df < 0) {
		librsu_log(HIGH, __func__,
			   "Unable to open output file '%s'", filename);
		return -EFILEIO;
	}

	if (ftruncate(df, 0)) {
		librsu_log(HIGH, __func__,
			   "Unable to truncate file '%s' to length zero",
			   filename);
		close(df);
		return -EFILEIO;
	}

	offset = 0;
	last_write = 0;

	memset(fill, 0xff, sizeof(fill));

	/* Read buf sized chunks from slot and write to file */
	while (offset < ll_intf->partition.size(part_num)) {
		/* Read a buffer size chunk from slot */
		if (ll_intf->data.read(part_num, offset, sizeof(buf), buf)) {
			librsu_log(HIGH, __func__,
				   "Unable to rd slot %i, offs 0x%08x, cnt %i",
				   slot, offset, sizeof(buf));
			close(df);
			return -ELOWLEVEL;
		}
		/* Scan buffer to see if we have all 0xff's. Don't write to
		 * file if we do.  If we skipped some chunks because they
		 * were all 0xff's and then find one that is not, we fill
		 * the file with 0xff chunks up to the current position.
		 */
		for (x = 0; x < sizeof(buf); x++) {
			if (buf[x] != (char)0xFF)
				break;
		}
		if (x < sizeof(buf)) {
			while (last_write < offset) {
				if (write(df, fill, sizeof(fill)) !=
				    sizeof(fill)) {
					librsu_log(HIGH, __func__,
						   "Unable to wr to '%s'",
						   filename);
					close(df);
					return -EFILEIO;
				}
				last_write += sizeof(fill);
			}

			if (write(df, buf, sizeof(buf)) != sizeof(buf)) {
				librsu_log(HIGH, __func__,
					   "Unable to wr to file '%s'",
					   filename);
				close(df);
				return -EFILEIO;
			}

			last_write += sizeof(buf);
		}

		offset += sizeof(buf);
	}

	close(df);

	return 0;
}

int rsu_slot_disable(int slot)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	return 0;
}

int rsu_slot_enable(int slot)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	if (ll_intf->priority.add(part_num))
		return -ELOWLEVEL;

	return 0;
}

int rsu_slot_load_after_reboot(int slot)
{
	int part_num;
	__u64 offset;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	offset = ll_intf->partition.offset(part_num);

	if (ll_intf->priority.get(part_num) <= 0) {
		librsu_log(HIGH, __func__,
			   "Trying to reboot to an erased slot");
		return -EERASE;
	}

	if (librsu_misc_put_devattr("reboot_image", offset))
		return -EFILEIO;

	return 0;
}

int rsu_slot_load_factory_after_reboot(void)
{
	int part_num;
	int partitions;
	__u64 offset;
	char name[] = "FACTORY_IMAGE";

	if (!ll_intf)
		return -ELIB;

	partitions = ll_intf->partition.count();

	for (part_num = 0; part_num < partitions; part_num++) {
		if (!strcmp(name, ll_intf->partition.name(part_num)))
			break;
	}

	if (part_num >= partitions) {
		librsu_log(MED, __func__, "No FACTORY_IMAGE partition defined");
		return -EFORMAT;
	}

	offset = ll_intf->partition.offset(part_num);

	if (librsu_misc_put_devattr("reboot_image", offset))
		return -EFILEIO;

	return 0;
}

int rsu_slot_rename(int slot, char *name)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	if (!name)
		return -EARGS;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (librsu_misc_is_rsvd_name(name)) {
		librsu_log(LOW, __func__,
			   "error: Partition rename uses a reserved name");
		return -ENAME;
	}

	if (ll_intf->partition.rename(part_num, name))
		return -ENAME;

	return 0;
}

/*
 * rsu_slot_delete() - Delete the selected slot.
 * slot: slot number
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_delete(int slot)
{
	int part_num;

	if (!ll_intf)
		return -ELIB;

	if (librsu_cfg_writeprotected(slot)) {
		librsu_log(HIGH, __func__,
			   "Trying to delete a write protected slot");
		return -EWRPROT;
	}

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	if (ll_intf->data.erase(part_num))
		return -ELOWLEVEL;

	if (ll_intf->partition.delete(part_num))
		return -ELOWLEVEL;

	return 0;
}

/*
 * rsu_slot_create() - Create a new slot.
 * name: slot name
 * address: slot start address
 * size: slot size
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_create(char *name, __u64 address, unsigned int size)
{
	if (!ll_intf)
		return -ELIB;

	if (librsu_misc_is_rsvd_name(name)) {
		librsu_log(LOW, __func__,
			   "error: Partition create uses a reserved name");
		return -ENAME;
	}

	if (ll_intf->partition.create(name, address, size))
		return -ELOWLEVEL;

	return 0;
}

int rsu_status_log(struct rsu_status_info *info)
{
	if (!ll_intf)
		return -ELIB;

	if (!info)
		return -EARGS;

	if (librsu_misc_get_devattr("version", &info->version))
		return -EFILEIO;

	if (librsu_misc_get_devattr("state", &info->state))
		return -EFILEIO;

	if (librsu_misc_get_devattr("current_image", &info->current_image))
		return -EFILEIO;

	if (librsu_misc_get_devattr("fail_image", &info->fail_image))
		return -EFILEIO;

	if (librsu_misc_get_devattr("error_location", &info->error_location))
		return -EFILEIO;

	if (librsu_misc_get_devattr("error_details", &info->error_details))
		return -EFILEIO;

	info->retry_counter = 0;

	if (!RSU_VERSION_ACMF_VERSION(info->version) ||
	    !RSU_VERSION_DCMF_VERSION(info->version))
		return 0;

	if (librsu_misc_get_devattr("retry_counter", &info->retry_counter)) {
		librsu_log(HIGH, __func__,
			   "retry_counter could not be retrieved");
		info->retry_counter = 0;
	}

	return 0;
}

int rsu_notify(int value)
{
	__u64 notify_value;

	notify_value = value & RSU_NOTIFY_VALUE_MASK;

	if (librsu_misc_put_devattr("notify", notify_value))
		return -EFILEIO;

	return 0;
}

int rsu_clear_error_status(void)
{
	struct rsu_status_info info;
	__u64 notify_value;

	if (rsu_status_log(&info))
		return -EFILEIO;


	if (!RSU_VERSION_ACMF_VERSION(info.version))
		return -EFILEIO;

	notify_value = RSU_NOTIFY_IGNORE_STAGE | RSU_NOTIFY_CLEAR_ERROR_STATUS;

	if (librsu_misc_put_devattr("notify", notify_value))
		return -EFILEIO;

	return 0;
}

int rsu_reset_retry_counter(void)
{
	struct rsu_status_info info;
	__u64 notify_value;

	if (rsu_status_log(&info))
		return -EFILEIO;

	if (!RSU_VERSION_ACMF_VERSION(info.version) ||
	    !RSU_VERSION_DCMF_VERSION(info.version))
		return -EFILEIO;

	notify_value = RSU_NOTIFY_IGNORE_STAGE | RSU_NOTIFY_RESET_RETRY_COUNTER;

	if (librsu_misc_put_devattr("notify", notify_value))
		return -EFILEIO;

	return 0;
}

/*
 * rsu_dcmf_version() - retrieve the decision firmware version
 * @versions: pointer to where the four DCMF versions will be stored
 *
 * This function is used to retrieve the version of each of the four DCMF copies
 * in flash.
 *
 * Returns: 0 on success, or error code
 */
int rsu_dcmf_version(__u32 *versions)
{
	__u64 version;
	int i;
	char dcmf_str[6] = {'d', 'c', 'm', 'f', '0', '\0'};

	for (i = 0; i < 4; i++) {
		if (librsu_misc_get_devattr(dcmf_str, &version))
			return -EFILEIO;

		versions[i] = version;
		dcmf_str[4]++;
	}

	return 0;
}

/*
 * rsu_max_retry() - retrieve the max_retry parameter
 * @value: pointer to where the max_retry will be stored
 *
 * This function is used to retrieve the max_retry parameter from flash.
 *
 * Returns: 0 on success, or error code
 */
int rsu_max_retry(__u8 *value)
{
	__u64 max_retry;

	if (librsu_misc_get_devattr("max_retry", &max_retry))
		return -EFILEIO;

	*value = (__u8)max_retry;
	return 0;
}
