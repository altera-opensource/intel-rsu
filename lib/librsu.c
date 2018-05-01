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

	rtn = rsu_slot_program_callback(slot, librsu_cb_buf);

	librsu_cb_buf_cleanup();

	return rtn;
}

int rsu_slot_program_file(int slot, char *filename)
{
	int rtn;

	if (librsu_cb_file_init(filename)) {
		librsu_log(HIGH, __func__, "Unable to open file '%s'",
			   filename);
		return -EFILEIO;
	}

	rtn = rsu_slot_program_callback(slot, librsu_cb_file);

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

	rtn = rsu_slot_verify_callback(slot, librsu_cb_buf);

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

	rtn = rsu_slot_verify_callback(slot, librsu_cb_file);

	librsu_cb_file_cleanup();

	return rtn;
}

int rsu_slot_program_callback(int slot, rsu_data_callback callback)
{
	int part_num;
	int offset;
	unsigned char buf[IMAGE_BLOCK_SZ];
	unsigned char vbuf[IMAGE_BLOCK_SZ];
	int cnt, c, done;
	int x;
	struct rsu_slot_info info;

	if (!ll_intf)
		return -ELIB;

	if (librsu_cfg_writeprotected(slot)) {
		librsu_log(HIGH, __func__,
			   "Trying to program a write protected slot");
		return -EWRPROT;
	}

	if (rsu_slot_get_info(slot, &info)) {
		librsu_log(HIGH, __func__, "Unable to read slot info");
		return -ESLOTNUM;
	}

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.get(part_num) > 0) {
		librsu_log(HIGH, __func__,
			   "Trying to program a slot already in use");
		return -EPROGRAM;
	}

	if (!callback)
		return -EARGS;

	offset = 0;
	done = 0;

	while (!done) {
		cnt = 0;
		while (cnt < IMAGE_BLOCK_SZ) {
			c = callback(buf + cnt, IMAGE_BLOCK_SZ - cnt);
			if (c == 0) {
				done = 1;
				break;
			} else if (c < 0) {
				return -ECALLBACK;
			}

			cnt += c;
		}

		if (cnt == 0)
			break;

		if (offset == IMAGE_PTR_BLOCK && cnt == IMAGE_BLOCK_SZ &&
		    librsu_image_adjust(buf, &info))
			return -EPROGRAM;

		if ((offset + cnt) > ll_intf->partition.size(part_num)) {
			librsu_log(HIGH, __func__,
				   "Trying to program too much data into slot");
			return -ESIZE;
		}

		if (ll_intf->data.write(part_num, offset, cnt, buf))
			return -ELOWLEVEL;

		if (ll_intf->data.read(part_num, offset, cnt, vbuf))
			return -ELOWLEVEL;

		for (x = 0; x < cnt; x++)
			if (vbuf[x] != buf[x]) {
				librsu_log(HIGH, __func__,
					   "Expect %02X, got %02X @ 0x%08X",
					   buf[x], vbuf[x], offset + x);
				return -ECMP;
			}

		offset += cnt;
	}

	if (ll_intf->priority.add(part_num))
		return -ELOWLEVEL;

	return 0;
}

int rsu_slot_verify_callback(int slot, rsu_data_callback callback)
{
	int part_num;
	int offset;
	unsigned char buf[IMAGE_BLOCK_SZ];
	unsigned char vbuf[IMAGE_BLOCK_SZ];
	int cnt, c, done;
	int x;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.get(part_num) <= 0) {
		librsu_log(HIGH, __func__,
			   "Trying to verify a slot not in use");
		return -EERASE;
	}

	if (!callback)
		return -EARGS;

	offset = 0;
	done = 0;

	while (!done) {
		cnt = 0;
		while (cnt < IMAGE_BLOCK_SZ) {
			c = callback(buf + cnt, IMAGE_BLOCK_SZ - cnt);
			if (c == 0) {
				done = 1;
				break;
			} else if (c < 0) {
				return -ECALLBACK;
			}

			cnt += c;
		}

		if (cnt == 0)
			break;

		if (ll_intf->data.read(part_num, offset, cnt, vbuf))
			return -ELOWLEVEL;

		for (x = 0; x < cnt; x++) {
			if ((offset + x) >= IMAGE_PTR_START &&
			    (offset + x) <= IMAGE_PTR_END)
				continue;

			if (vbuf[x] != buf[x]) {
				librsu_log(HIGH, __func__,
					   "Expect %02X, got %02X @ 0x%08X",
					   buf[x], vbuf[x], offset + x);
				return -ECMP;
			}
		}

		offset += cnt;
	}

	return 0;
}

int rsu_slot_copy_to_file(int slot, char *filename)
{
	int part_num;
	int df;
	int offset;
	char buf[0x1000];
	char fill[0x1000];
	int last_write;
	int x;

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
	struct rsu_slot_info info;

	if (!ll_intf)
		return -ELIB;

	part_num = librsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (rsu_slot_get_info(slot, &info)) {
		librsu_log(HIGH, __func__, "Unable to read slot info");
		return -ESLOTNUM;
	}

	if (ll_intf->priority.get(part_num) <= 0) {
		librsu_log(HIGH, __func__,
			   "Trying to reboot to an erased slot");
		return -EERASE;
	}

	if (librsu_misc_put_devattr("reboot_image", info.offset))
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

int rsu_status_log(struct rsu_status_info *info)
{
	char tmp[64];
	__u64 value;

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

	return 0;
}
