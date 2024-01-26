// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include "librsu_cfg.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef DEFAULT_RSU_DEV
#define DEFAULT_RSU_DEV "/sys/devices/platform/stratix10-rsu.0"
#endif

static enum RSU_LOG_TYPE { STDERR = 0, LOGFILE } logtype = STDERR;
static enum RSU_LOG_LEVEL loglevel = LOW;
static FILE *logfile;

static enum RSU_LL_TYPE roottype = INVALID;
static char *parsed_rootpath[QSPI_MAX_DEVICE];
static char rootpath[128];
static char rsu_dev[128] = DEFAULT_RSU_DEV;
static int writeprotect;
static int spt_checksum_enabled;
static int total_num_flash_devices = 0;

void SAFE_STRCPY(char *dst, int dsz, char *src, int ssz)
{
	int len;

	if (!dst || dsz <= 0)
		return;

	if (!src || ssz <= 0) {
		dst[0] = '\0';
		return;
	}

	/*
	 * calculate len to be smaller of source string len or destination
	 * buffer size, then copy len bytes and terminate string.
	 */

	len = strnlen(src, ssz);

	if (len >= dsz)
		len = dsz - 1;

	memcpy(dst, src, len);
	dst[len] = '\0';
}

void librsu_cfg_reset(void)
{
	if (logfile)
		fclose(logfile);

	logtype = STDERR;
	loglevel = LOW;
	logfile = NULL;

	roottype = INVALID;
	rootpath[0] = '\0';
	SAFE_STRCPY(rsu_dev, sizeof(rsu_dev), DEFAULT_RSU_DEV,
		    sizeof(DEFAULT_RSU_DEV));
	writeprotect = 0;
	spt_checksum_enabled = 0;

	/* free the memory for rsu multiflash rootpath */
	for (int i = 0; i < QSPI_MAX_DEVICE; i++) {
		free(parsed_rootpath[i]);
	}
}

/*
 * split_line() - Split a line buffer into words, leaving the words null
 *                terminated in place in the buffer.
 * line - Pointer to line buffer
 * words - array of pointers to store start of words
 * cnt - number of pointers in the words array
 *
 * Returns the number of words found
 */
static int split_line(char *line, char **words, int cnt)
{
	char *c = line;
	int y = 0;

	while ((*c != '\0') && (y < cnt)) {
		/* Look for start of a word */
		while (*c <= ' ') {
			if (*c == '\0')
				return y;
			c++;
		}

		/* Remember start address of word and look for the end */
		words[y++] = c;
		while (*c > ' ')
			c++;

		/* '\0' terminate words that are not at the end of the str */
		if (*c != '\0') {
			*c = '\0';
			c++;
		}
	}
	return y;
}

int librsu_cfg_parse(FILE *input)
{
	char linebuf[256];
	char *argv[16];
	int argc;
	int linenum;
	int x;

	linenum = 0;
	while (fgets(linebuf, sizeof(linebuf), input)) {
		linenum++;
		argc = split_line(linebuf, argv, 16);

		if (argc < 1)
			continue;

		if (argv[0][0] == '#')
			continue;

		if ((argv[0][0] == '/') && (argv[0][1] == '/'))
			continue;

		if (strcmp(argv[0], "root") == 0) {
			if (argc != 3) {
				librsu_log(LOW, __func__,
					   "error: Wrong number of param for '%s' @%i",
					   argv[0], linenum);
				return -1;
			}

			if (roottype != INVALID) {
				librsu_log(LOW, __func__,
					   "error: Redefinition of root @%i",
					   linenum);
				return -1;
			}

			if (strcmp(argv[1], "datafile") == 0) {
				roottype = DATAFILE;
			} else if (strcmp(argv[1], "qspi") == 0) {
				roottype = QSPI;
			} else {
				roottype = INVALID;
				librsu_log(LOW, __func__,
					   "error: Invalid parameter '%s' for '%s' @%i",
					   argv[1], argv[0], linenum);
				return -1;
			}

			librsu_cfg_parse_rootpath(argv[2]);
		} else if (strcmp(argv[0], "rsu-dev") == 0) {
			if (argc != 2) {
				librsu_log(LOW, __func__,
					   "error: Wrong number of param for '%s' @%i",
					   argv[0], linenum);
				return -1;
			}

			SAFE_STRCPY(rsu_dev, sizeof(rsu_dev), argv[1],
				    sizeof(rsu_dev));
		} else if (strcmp(argv[0], "log") == 0) {
			if (argc < 2) {
				librsu_log(LOW, __func__,
					   "error: Wrong number of param for '%s' @%i",
					   argv[0], linenum);
				return -1;
			}

			if (logfile) {
				librsu_log(LOW, __func__,
					   "Logfile already open - closing @%i",
					   linenum);
				fclose(logfile);
				logfile = NULL;
			}

			if (strcmp(argv[1], "off") == 0) {
				loglevel = OFF;
				continue;
			} else if (strcmp(argv[1], "low") == 0) {
				loglevel = LOW;
			} else if (strcmp(argv[1], "med") == 0) {
				loglevel = MED;
			} else if (strcmp(argv[1], "high") == 0) {
				loglevel = HIGH;
			} else {
				librsu_log(LOW, __func__,
					   "error: Invalid parameter '%s' for '%s' @%i",
					   argv[1], argv[0], linenum);
				return -1;
			}

			if (argc < 3 || strcmp(argv[2], "stderr") == 0) {
				logtype = STDERR;
			} else {
				logtype = LOGFILE;
				logfile = fopen(argv[2], "a");
				if (!logfile) {
					librsu_log(LOW, __func__,
						   "Unable to open logfile '%s' @%i",
						   argv[2], linenum);
					continue;
				} else {
					fprintf(logfile,
						"\n---- START SESSION ----\n");
				}
			}
		} else if (strcmp(argv[0], "write-protect") == 0) {
			if (argc != 2) {
				librsu_log(LOW, __func__,
					   "error: Wrong number of parameters for '%s' @%i",
					   argv[0], linenum);
				return -1;
			}

			x = strtol(argv[1], NULL, 10);
			if (x < 0 || x > 31) {
				librsu_log(LOW, __func__,
					   "error: Write Prot only works on first 32 slots @%i",
					   linenum);
				return -1;
			}
			writeprotect |= (1 << x);
		} else if (strcmp(argv[0], "rsu-spt-checksum") == 0) {
			if (argc != 2) {
				librsu_log(LOW, __func__,
					   "Wrong number of parameters for '%s' @%i",
					   argv[0], linenum);
				return -1;
			}

			spt_checksum_enabled = strtol(argv[1], NULL, 10);
		} else {
			librsu_log(LOW, __func__,
				   "error: Invalid cfg file option '%s' @%i",
				   argv[0], linenum);
			return -1;
		}
	}

	if (roottype == 0) {
		librsu_log(LOW, __func__,
			   "error: Missing 'root' spec in configuration file");
		return -1;
	}

	return 0;
}

void librsu_log(enum RSU_LOG_LEVEL level, const char *func, const char *format,
		...)
{
	va_list arg;
	char *level_name;

	if (loglevel == OFF || loglevel < level)
		return;

	if (level == LOW)
		level_name = "LOW";
	else if (level == MED)
		level_name = "MED";
	else if (level == HIGH)
		level_name = "HIGH";
	else
		level_name = "???";

	if (logtype == STDERR) {
		fprintf(stderr, "librsu: %s(): ", func);

		va_start(arg, format);
		vfprintf(stderr, format, arg);
		va_end(arg);

		fprintf(stderr, " [%s]\n", level_name);
		fflush(stderr);
	} else if (logtype == LOGFILE) {
		if (!logfile)
			return;

		fprintf(logfile, "%s(): ", func);

		va_start(arg, format);
		vfprintf(logfile, format, arg);
		va_end(arg);

		fprintf(logfile, " [%s]\n", level_name);
		fflush(logfile);
	}
}

enum RSU_LL_TYPE librsu_cfg_get_roottype(void)
{
	return roottype;
}

void librsu_cfg_parse_rootpath(char *rootpath)
{
	char *token;
	char *delimiter = ",";

	if (!rootpath) {
		librsu_log(LOW, __func__, "error: rootpath is NULL. Exiting.");
		return;
	}

	/* allocate tmp string for parsing */
	char *tmp = strdup(rootpath);

	/* get first mtd token */
	token = strtok(tmp, delimiter);

	/* parse other mtds */
	while(token != NULL && total_num_flash_devices < QSPI_MAX_DEVICE) {
		parsed_rootpath[total_num_flash_devices] = strdup(token);
		if (!parsed_rootpath[total_num_flash_devices]) {
			/* free tmp string */
			free(tmp);
			librsu_log(LOW, __func__, "error: token is NULL. Exiting.");
			return;
		}

		token = strtok(NULL, delimiter);
		total_num_flash_devices++;
	}

	/* free tmp string */
	free(tmp);
}

int librsu_cfg_get_rootpath(struct spi_flash_info *flash_info)
{
	/* retrieve the rootpath from parsed mtd */
	for (int i = 0; i < total_num_flash_devices; i++) {
		flash_info->root_path[i] = strdup(parsed_rootpath[i]);
		flash_info->flash_index[i] = i;
	}

	/* return the num of flash */
	if (roottype != 0)
		return total_num_flash_devices;
	else
		return 0;
}

char *librsu_cfg_get_rsu_dev(void)
{
	return rsu_dev;
}

int librsu_cfg_writeprotected(int slot)
{
	if (slot < 0)
		return -1;

	if (slot > 31)
		return 0;

	if (writeprotect & (1 << slot))
		return 1;

	return 0;
}

int librsu_cfg_spt_checksum_enabled(void)
{
	if (spt_checksum_enabled)
		return 1;

	return 0;
}
