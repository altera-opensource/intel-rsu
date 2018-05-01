// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <fcntl.h>
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
