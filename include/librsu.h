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
#define ECORRUPTED_CPB	15
#define ECORRUPTED_SPT	16

/*
 * Macros for extracting version fields
 */
#define RSU_VERSION_ERROR_SOURCE(v) (((v) & 0xFFFF0000) >> 16)
#define RSU_VERSION_ACMF_VERSION(v) (((v) & 0xFF00) >> 8)
#define RSU_VERSION_DCMF_VERSION(v) ((v) & 0xFF)

/* Macros for extracting DCMF version fields */
#define DCMF_VERSION_MAJOR(v)  (((v) & 0xFF000000) >> 24)
#define DCMF_VERSION_MINOR(v)  (((v) & 0x00FF0000) >> 16)
#define DCMF_VERSION_UPDATE(v) (((v) & 0x0000FF00) >> 8)

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
 * rsu_slot_erase() - erase all data in a slot to prepare for programming.
 *                    Remove the slot if it is in the CPB.
 * slot: slot number
 *
 * Returns 0 on success, or Error Cdoe
 */
int rsu_slot_erase(int slot);

/*
 * rsu_slot_program_buf() - program a slot using FPGA config data from a buffer
 *                          and enter slot into CPB
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_buf(int slot, void *buf, int size);

/*
 * rsu_slot_program_factory_update_buf() - program a slot using factory update
 *                                         data from a buffer and enter slot
 *                                         into CPB
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_factory_update_buf(int slot, void *buf, int size);

/*
 * rsu_slot_program_file() - program a slot using FPGA config data from a file
 *                           and enter slot into CPB
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_file(int slot, char *filename);

/*
 * rsu_slot_program_factory_update_file() - program a slot using factory update
 *                                          data from a file and enter slot
 *                                          into CPB
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_factory_update_file(int slot, char *filename);

/*
 * rsu_slot_program_buf_raw() - program a slot using raw data from a buffer.
 *                              The slot is not entered into the CPB
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_buf_raw(int slot, void *buf, int size);

/*
 * rsu_slot_program_file_raw() - program a slot using raw data from a file.
 *                               The slot is not entered into the CPB
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_file_raw(int slot, char *filename);

/*
 * rsu_slot_verify_buf() - verify FPGA config data in a slot against a buffer
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_buf(int slot, void *buf, int size);

/*
 * rsu_slot_verify_file() - verify FPGA config data in a slot against a file
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_file(int slot, char *filename);

/*
 * rsu_slot_verify_buf_raw() - verify raw data in a slot against a buffer
 * slot: slot number
 * buf: pointer to data buffer
 * size: bytes to read from buffer
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_buf_raw(int slot, void *buf, int size);

/*
 * rsu_slot_verify_file_raw() - verify raw data in a slot against a file
 * slot: slot number
 * filename: input data file
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_file_raw(int slot, char *filename);

/*
 * rsu_data_callback - function pointer type for callback functions below
 */
typedef int (*rsu_data_callback)(void *buf, int size);

/*
 * rsu_slot_program_callback() - program and verify a slot using FPGA config
 *                               data provided by a callback function. Enter
 *                               the slot into the CPB
 * slot: slot number
 * callback: callback function to provide input data
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_callback(int slot, rsu_data_callback callback);

/*
 * rsu_slot_program_callback_raw() - program and verify a slot using raw data
 *                                   provided by a callback function.  The slot
 *                                   is not entered into the CPB
 * slot: slot number
 * callback: callback function to provide input data
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_program_callback_raw(int slot, rsu_data_callback callback);

/*
 * rsu_slot_verify_callback() - verify a slot using FPGA configdata provided by
 *                              a callback function.
 * slot: slot number
 * callback: callback function to provide input data
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_callback(int slot, rsu_data_callback callback);

/*
 * rsu_slot_verify_callback_raw() - verify a slot using raw data provided by a
 *                                  callback function.
 * slot: slot number
 * callback: callback function to provide input data
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_verify_callback_raw(int slot, rsu_data_callback callback);

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
 * rsu_slot_delete() - Delete the selected slot.
 * slot: slot number
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_delete(int slot);

/*
 * rsu_slot_create() - Create a new slot.
 * name: slot name
 * address: slot start address
 * size: slot size
 *
 * Returns 0 on success, or Error Code
 */
int rsu_slot_create(char *name, __u64 address, unsigned int size);


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
	__u64 retry_counter;
};

/*
 * rsu_status_log() - Copy the Secure Domain Manager status log to info struct
 * info: pointer to info struct to fill in
 *
 * Return 0 on success, or Error Code
 */
int rsu_status_log(struct rsu_status_info *info);

/*
 * rsu_notify() - report HPS software execution stage as a 16bit number
 * stage: software execution stage
 *
 * Returns 0 on success, or Error Code
 */
int rsu_notify(int value);

/*
 * rsu_clear_error_status() - clear errors from the current status log
 *
 * Returns 0 on success, or Error Code
 */
int rsu_clear_error_status(void);

/*
 * rsu_reset_retry_counter() - reset the retry counter, so that the currently
 *                             running image may be tried again after a watchdog
 *                             timeout.
 *
 * Returns 0 on success, or Error Code
 */
int rsu_reset_retry_counter(void);

/*
 * rsu_dcmf_version() - retrieve the decision firmware version
 * @versions: pointer to where the four DCMF versions will be stored
 *
 * This function is used to retrieve the version of each of the four DCMF copies
 * in flash.
 *
 * Returns: 0 on success, or error code
 */
int rsu_dcmf_version(__u32 *versions);

/*
 * rsu_max_retry() - retrieve the max_retry parameter
 * @value: pointer to where the max_retry will be stored
 *
 * This function is used to retrieve the max_retry parameter from flash.
 *
 * Returns: 0 on success, or error code
 */
int rsu_max_retry(__u8 *value);

/*
 * rsu_dcmf_status() - retrieve the decision firmware status
 * @status: pointer to where the status values will be stored
 *
 * This function is used to determine whether decision firmware copies are
 * corrupted in flash, with the currently used decision firmware being used as
 * reference. The status is an array of 4 values, one for each decision
 * firmware copy. A 0 means the copy is fine, anything else means the copy is
 * corrupted.
 *
 * Returns: 0 on success, or error code
 */
int rsu_dcmf_status(int *status);

/*
 * rsu_save_spt() - save spt to the file
 * @name: file name which SPT will be saved to
 *
 * This function is used to save the working SPT to a file.
 *
 * Returns: 0 on success, or error code
 */
int rsu_save_spt(char *name);

/*
 * rsu_restore_spt() - restore spt from the file
 * @name: file name which SPT will be restored from
 *
 * This function is used to restore the SPT from a saved file.
 *
 * Returns: 0 on success, or error code
 */
int rsu_restore_spt(char *name);

/*
 * rsu_save_cpb() - save cpb to the file
 * @name: the name of file which cpb is saved to
 *
 * This function is used to save the working CPB to a file.
 *
 * Returns: 0 on success, or error code
 */
int rsu_save_cpb(char *name);

/*
 * rsu_create_empty_cpb() - create a header only cpb
 *
 * This function is used to create a empty CPB, which include CPB header
 * only.
 *
 * Returns: 0 on success, or error code
 */
int rsu_create_empty_cpb(void);

/*
 * rsu_restore_cpb() - restore the cpb
 * @name: the name of file which cpb is restored from
 *
 * This function is used to restore the CPB from a saved file
 *
 * Returns: 0 on success, or error code
 */
int rsu_restore_cpb(char *name);
#endif
