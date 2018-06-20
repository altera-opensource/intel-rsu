/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_H__
#define __LIBRSU_H__

#include <linux/types.h>

/*
 * LIBRSU Error Codes
 */
#define ELIB		1
#define ECFG		2
#define ESLOTNUM	3
#define EFORMAT		4
#define EERASE		5
#define EPROGRAM	6
#define ECMP		7
#define ESIZE		8
#define ENAME		9
#define EFILEIO		10
#define ECALLBACK	11
#define ELOWLEVEL	12
#define EWRPROT		13
#define EARGS		14

/*
 * librsu_init() - Load the configuration file and initialize internal data
 * filename: configuration file to load
 *            (if Null or empty string, the default is /etc/librsu.rc)
 *
 * Returns 0 on success, or Error Code
 */
int librsu_init(char *filename);

/*
 * librsu_exit() - cleanup internal data and release librsu
 *
 * Returns nothing
 */
void librsu_exit(void);

/*
 * librsu_slot_count() - get the number of slots defined
 *
 * Returns the number of defined slots
 */
int rsu_slot_count(void);

/*
 * rsu_slot_by_name() - return slot number basd on name
 * name: name of slot
 *
 * Return slot number on success, or Error Code
 */
int rsu_slot_by_name(char *name);

/*
 * rsu_slot_info - structure to capture slot information details
 */
struct rsu_slot_info {
	char name[16];
	__u64 offset;
	int size;
	int priority;
};

/*
 * rsu_slot_get_info() - return the attributes of a slot
 * slot: slot number
 * info: pointer to info structure to be filled in
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_get_info(int slot, struct rsu_slot_info *info);

/*
 * rsu_slot_size() - get the size of a slot
 * slot: slot number
 *
 * Returns the size of the slot in bytes, or Error Code
 */
int rsu_slot_size(int slot);

/*
 * rsu_slot_priority() - get the Decision CMF load priority of a slot
 * slot: slot number
 *
 * NOTE: Priority of zero means the slot has no priority and is disabled.
 *       The slot with priority of one has the highest priority.
 *
 * Returns the priority of the slot, or Error Code
 */
int rsu_slot_priority(int slot);

/*
 * rsu_slot_erase() - erase all data in a slot to prepare for programming
 * slot: slot number
 *
 * Returns 0 on success, or Error Cdoe
 */
int rsu_slot_erase(int slot);

/*
 * rsu_slot_program_buf() - program a slot using data from a buffer
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_buf(int slot, void *buf, int size);

/*
 * rsu_slot_program_file() - program a slot using data from a file
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_file(int slot, char *filename);

/*
 * rsu_slot_verify_buf() - verify the data in a slot compared to a buffer
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_buf(int slot, void *buf, int size);

/*
 * rsu_slot_verify_file() - verify the data in a slot compared to a file
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_file(int slot, char *filename);

/*
 * rsu_data_callback - function pointer type for callback functions below
 */
typedef int (*rsu_data_callback)(void *buf, int size);

/*
 * rsu_slot_program_callback() - program and verify a slot using data
 *                               provided by a callback function.
 * slot: slot number
 * callback: callback function to provide input data
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_callback(int slot, rsu_data_callback callback);

/*
 * rsu_slot_verify_callback() - verify a slot using data provided by a
 *                              callback function.
 * slot: slot number
 * callback: callback function to provide input data
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_callback(int slot, rsu_data_callback callback);

/*
 * rsu_slot_copy_to_file() - read the data in a slot and write to a file
 * slot: slot number
 * filename: output data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_copy_to_file(int slot, char *filename);

/*
 * rsu_slot_enable() - Set the selected slot as the highest prioirity.  It will
 *                     be the first slot tried after a power-on reset
 * slot: slot number
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_enable(int slot);

/*
 * rsu_slot_disable() - Remove the selected slot from the priority scheme, but
 *                      do not erase the slot data so that it can be re-enabled.
 * slot: slot number
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_disable(int slot);

/*
 * rsu_slot_load_after_reboot() - Request that the selected slot be loaded after
 *                                the next reboot, no matter the priority. A
 *                                power-on reset will ignore this request and
 *                                use slot priority to select the first slot.
 * slot: slot number
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_load_after_reboot(int slot);

/*
 * rsu_slot_load_factory_after_reboot() - Request that the factory image be
 *                                        loaded after the next reboot. A
 *                                        power-on reset will ignore this
 *                                        request and use slot priority to
 *                                        select the first slot.
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_load_factory_after_reboot(void);

/*
 * rsu_slot_rename() - Rename the selected slot.
 * slot: slot number
 * name: new name for slot
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_rename(int slot, char *name);

/*
 * rsu_status_info - structure to capture SDM status log information
 */
struct rsu_status_info {
	__u64 version;
	__u64 state;
	__u64 current_image;
	__u64 fail_image;
	__u64 error_location;
	__u64 error_details;
};

/*
 * rsu_status_log() - Copy the Secure Domain Manager status log to info struct
 * info: pointer to info struct to fill in
 *
 * Return 0 on success, or Error Code
 */
int rsu_status_log(struct rsu_status_info *info);

#endif
