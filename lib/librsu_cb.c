// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <fcntl.h>
#include "librsu_cb.h"
#include "librsu_cfg.h"
#include "librsu_image.h"
#include "librsu_ll.h"
#include "librsu_misc.h"
#include <string.h>
#include <unistd.h>

static int cb_datafile = -1;

int librsu_cb_file_init(char *filename)
{
	if (cb_datafile >= 0)
		close(cb_datafile);

	if (!filename)
		return -1;

	cb_datafile = open(filename, O_RDONLY);

	if (cb_datafile < 0)
		return -1;

	return 0;
}

void librsu_cb_file_cleanup(void)
{
	if (cb_datafile >= 0)
		close(cb_datafile);

	cb_datafile = -1;
}

int librsu_cb_file(void *buf, int len)
{
	if (cb_datafile < 0)
		return -1;

	return read(cb_datafile, buf, len);
}

static char *cb_buffer;
static int cb_buffer_togo;

int librsu_cb_buf_init(void *buf, int size)
{
	if (!buf || size <= 0)
		return -1;

	cb_buffer = (char *)buf;
	cb_buffer_togo = size;

	return 0;
}

void librsu_cb_buf_cleanup(void)
{
	cb_buffer = NULL;
	cb_buffer_togo = -1;
}

int librsu_cb_buf(void *buf, int len)
{
	int read_len;

	if (!cb_buffer_togo)
		return 0;

	if (!cb_buffer || cb_buffer_togo < 0 || !buf || len < 0)
		return -1;

	if (cb_buffer_togo < len)
		read_len = cb_buffer_togo;
	else
		read_len = len;

	memcpy(buf, cb_buffer, read_len);

	cb_buffer += read_len;
	cb_buffer_togo -= read_len;

	if (!cb_buffer_togo)
		cb_buffer = NULL;

	return read_len;
}

int librsu_cb_program_common(struct librsu_ll_intf *ll_intf, int slot,
			     rsu_data_callback callback, int rawdata)
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

		if (!rawdata && offset == IMAGE_PTR_BLOCK &&
		    cnt == IMAGE_BLOCK_SZ && librsu_image_adjust(buf, &info))
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

	if (!rawdata && ll_intf->priority.add(part_num))
		return -ELOWLEVEL;

	return 0;
}

int librsu_cb_verify_common(struct librsu_ll_intf *ll_intf, int slot,
			    rsu_data_callback callback, int rawdata)
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

	if (!rawdata && ll_intf->priority.get(part_num) <= 0) {
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
			if (!rawdata && (offset + x) >= IMAGE_PTR_START &&
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
