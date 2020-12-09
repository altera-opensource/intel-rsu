// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include <getopt.h>
#include <librsu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLOT_NAME	15

/*
 * enum rsu_clinet_command_code - supporting RSU client commands
 * COMMAND_
 */
enum rsu_clinet_command_code {
	COMMAND_NONE = 0,
	COMMAND_SLOT_COUNT,
	COMMAND_SLOT_ATTR,
	COMMAND_SLOT_SIZE,
	COMMAND_SLOT_PRIORITY,
	COMMAND_SLOT_ENABLE,
	COMMAND_SLOT_DISABLE,
	COMMAND_SLOT_LOAD,
	COMMAND_FACTORY_LOAD,
	COMMAND_SLOT_ERASE,
	COMMAND_ADD_IMAGE,
	COMMAND_ADD_FACTORY_UPDATE_IMAGE,
	COMMAND_ADD_RAW_IMAGE,
	COMMAND_VERIFY_IMAGE,
	COMMAND_VERIFY_RAW_IMAGE,
	COMMAND_COPY_TO_FILE,
	COMMAND_STATUS_LOG,
	COMMAND_NOTIFY,
	COMMAND_CLEAR_ERROR_STATUS,
	COMMAND_RESET_RETRY_COUNTER,
	COMMAND_DISPLAY_DCMF_VERSION,
	COMMAND_DISPLAY_DCMF_STATUS,
	COMMAND_DISPLAY_MAX_RETRY,
	COMMAND_SLOT_CREATE,
	COMMAND_SLOT_DELETE,
	COMMAND_RESTORE_SPT,
	COMMAND_SAVE_SPT,
	COMMAND_CREATE_EMPTY_CPB,
	COMMAND_RESTORE_CPB,
	COMMAND_SAVE_CPB,
	COMMAND_CHECK_RUNNING_FACTORY
};

static const struct option opts[] = {
	{"count", no_argument, NULL, 'c'},
	{"log", no_argument, NULL, 'g'},
	{"help", no_argument, NULL, 'h'},
	{"request-factory", no_argument, NULL, 'R'},
	{"list", required_argument, NULL, 'l'},
	{"size", required_argument, NULL, 'z'},
	{"priority", required_argument, NULL, 'p'},
	{"enable", required_argument, NULL, 'E'},
	{"disable", required_argument, NULL, 'D'},
	{"add", required_argument, NULL, 'a'},
	{"add-factory-update", required_argument, NULL, 'u'},
	{"add-raw", required_argument, NULL, 'A'},
	{"slot", required_argument, NULL, 's'},
	{"erase", required_argument, NULL, 'e'},
	{"verify", required_argument, NULL, 'v'},
	{"verify-raw", required_argument, NULL, 'V'},
	{"copy", required_argument, NULL, 'f'},
	{"request", required_argument, NULL, 'r'},
	{"notify", required_argument, NULL, 'n'},
	{"clear-error-status", no_argument, NULL, 'C'},
	{"reset-retry-counter", no_argument, NULL, 'Z'},
	{"display-dcmf-version", no_argument, NULL, 'm'},
	{"display-dcmf-status", no_argument, NULL, 'y'},
	{"display-max-retry", no_argument, NULL, 'x'},
	{"address", required_argument, NULL, 'S'},
	{"length", required_argument, NULL, 'L'},
	{"create-slot", required_argument, NULL, 't'},
	{"delete-slot", required_argument, NULL, 'd'},
	{"restore-spt", required_argument, NULL, 'W'},
	{"save-spt", required_argument, NULL, 'X'},
	{"create-empty-cpb", no_argument, NULL, 'b'},
	{"restore-cpb", required_argument, NULL, 'B'},
	{"save-cpb", required_argument, NULL, 'P'},
	{"check-running-factory", no_argument, NULL, 'k'},
	{NULL, 0, NULL, 0}
};

/*
 * rsu_client_usage() - show the usage of client application
 *
 * This function doesn't have return.
 */
static void rsu_client_usage(void)
{
	printf("--- RSU app usage ---\n");
	printf("%-32s  %s", "-c|--count", "get the number of slots\n");
	printf("%-32s  %s", "-l|--list slot_num",
	       "list the attribute info from the selected slot\n");
	printf("%-32s  %s", "-z|--size slot_num",
	       "get the slot size in bytes\n");
	printf("%-32s  %s", "-p|--priority slot_num",
	       "get the priority of the selected slot\n");
	printf("%-32s  %s", "-E|--enable slot_num",
	       "set the selected slot as the highest priority\n");
	printf("%-32s  %s", "-D|--disable slot_num",
	       "disable selected slot but to not erase it\n");
	printf("%-32s  %s", "-r|--request slot_num",
	       "request the selected slot to be loaded after the next reboot\n");
	printf("%-32s  %s", "-R|--request-factory",
	       "request the factory image to be loaded after the next reboot\n");
	printf("%-32s  %s", "-e|--erase slot_num",
	       "erase app image from the selected slot\n");
	printf("%-32s  %s", "-a|--add file_name -s|--slot slot_num",
	       "add a new app image to the selected slot\n");
	printf("%-32s  %s", "-u|--add-factory-update file_name -s|--slot slot_num",
	       "add a new factory update image to the selected slot\n");
	printf("%-32s  %s", "-A|--add-raw file_name -s|--slot slot_num",
	       "add a new raw image to the selected slot\n");
	printf("%-32s  %s", "-v|--verify file_name -s|--slot slot_num",
	       "verify app image on the selected slot\n");
	printf("%-32s  %s", "-V|--verify-raw file_name -s|--slot slot_num",
	       "verify raw image on the selected slot\n");
	printf("%-32s  %s", "-f|--copy file_name -s|--slot slot_num",
	       "read the data in a selected slot then write to a file\n");
	printf("%-32s  %s", "-g|--log", "print the status log\n");
	printf("%-32s  %s", "-n|--notify value", "report software state\n");
	printf("%-32s  %s", "-C|--clear-error-status",
		"clear errors from the log\n");
	printf("%-32s  %s", "-Z|--reset-retry-counter",
		"reset current retry counter\n");
	printf("%-32s  %s", "-m|--display-dcmf-version",
	       "print DCMF version\n");
	printf("%-32s  %s", "-y|--display-dcmf-status",
	       "print DCMF status\n");
	printf("%-32s  %s", "-x|--display-max-retry",
	       "print max_retry parameter\n");
	printf("%-32s  %s", "-t|--create-slot slot_name -S|--address slot_address -L|--length slot_size",
		"create a new slot using unallocated space\n");
	printf("%-32s  %s", "-d|--delete-slot slot_num",
		"delete selected slot, freeing up allocated space\n");
	printf("%-32s  %s", "-W|--restore-spt file_name", "restore spt from a file\n");
	printf("%-32s  %s", "-X|--save-spt file_name", "save spt to a file\n");
	printf("%-32s  %s", "-b|--create-empty-cpb", "create a empty cpb\n");
	printf("%-32s  %s", "-B|--restore-cpb file_name", "restore cpb from a file\n");
	printf("%-32s  %s", "-P|--save-cpb file_name", "save cpb to a file\n");
	printf("%-32s  %s", "-k|--check-running-factory", "check if currently running the factory image\n");
	printf("%-32s  %s", "-h|--help", "show usage message\n");
}

/*
 * rsu_client_slot_count() - get the number of predefined slot
 *
 * Return: number of slots
 */
static int rsu_client_get_slot_count(void)
{
	return rsu_slot_count();
}

/*
 * rsu_client_copy_status_log() - copy status log
 *
 * This function copies the SDM status log info to struct rsu_status_info
 *
 * Return: 0 on success, -1 on error
 */
static int rsu_client_copy_status_log(void)
{
	struct rsu_status_info *info;
	int rtn = -1;

	info = (struct rsu_status_info *)malloc(sizeof(*info));
	if (!info) {
		printf("%s: fail to allocate\n", __func__);
		return rtn;
	}

	if (!rsu_status_log(info)) {
		rtn = 0;
		printf("      VERSION: 0x%08X\n", (int)info->version);
		printf("        STATE: 0x%08X\n", (int)info->state);
		printf("CURRENT IMAGE: 0x%016llX\n", info->current_image);
		printf("   FAIL IMAGE: 0x%016llX\n", info->fail_image);
		printf("    ERROR LOC: 0x%08X\n", (int)info->error_location);
		printf("ERROR DETAILS: 0x%08X\n", (int)info->error_details);
		if (RSU_VERSION_DCMF_VERSION(info->version) &&
		    RSU_VERSION_ACMF_VERSION(info->version))
			printf("RETRY COUNTER: 0x%08X\n",
			       (int)info->retry_counter);
	}

	free(info);

	return rtn;
}

/*
 * rsu_client_request_slot_be_loaded() - Request the selected slot be loaded
 * slot_num: the slected slot
 *
 * This function requests that the selected slot be loaded after the next
 * reboot.
 *
 * Return: 0 on success, or negative value on error
 */
static int rsu_client_request_slot_be_loaded(int slot_num)
{
	return rsu_slot_load_after_reboot(slot_num);
}

/*
 * rsu_client_request_factory_be_loaded() - Request the factory image be loaded
 *
 * Return: 0 on success, or negative value on error
 */
static int rsu_client_request_factory_be_loaded(void)
{
	return rsu_slot_load_factory_after_reboot();
}

/*
 * rsu_client_list_slot_attribute() - list the attribute of a slot
 * slot_num: the slected slot
 *
 * This function lists the attributes of a selected slot. The attributes
 * are image name, offset, size and priority level.
 *
 * Return: 0 on success, or -1 on error
 */
static int rsu_client_list_slot_attribute(int slot_num)
{
	struct rsu_slot_info *info;
	int rtn = -1;

	info = (struct rsu_slot_info *)malloc(sizeof(*info));
	if (!info) {
		printf("%s: fail to allocate\n", __func__);
		return rtn;
	}

	if (!rsu_slot_get_info(slot_num, info)) {
		rtn = 0;
		printf("      NAME: %s\n", info->name);
		printf("    OFFSET: 0x%016llX\n", info->offset);
		printf("      SIZE: 0x%08X\n", info->size);

		if (info->priority)
			printf("  PRIORITY: %i\n", info->priority);
		else
			printf("  PRIORITY: [disabled]\n");
	}

	free(info);

	return rtn;
}

/*
 * rsu_client_get_slot_size() - get the size for a selected slot
 * slot_num: a selected slot
 *
 * Return: size of the selected slot on success, or negative value on error
 */
static int rsu_client_get_slot_size(int slot_num)
{
	return rsu_slot_size(slot_num);
}

/*
 * rsu_client_get_priority() - get the priority for a selected slot
 * slot_num: a selected slot
 *
 * Return: 0 on success, or negative value on error
 */
static int rsu_client_get_priority(int slot_num)
{
	return rsu_slot_priority(slot_num);
}

/*
 * rsu_add_app_image() - add a new application image
 * image_name: name of the application image
 * slot_name: the selected slot
 * raw: raw file if set, app file if cleared
 *
 * Return: 0 on success, or negative value on error
 */
static int rsu_client_add_app_image(char *image_name, int slot_num, int raw)
{
	if (raw)
		return rsu_slot_program_file_raw(slot_num, image_name);

	return rsu_slot_program_file(slot_num, image_name);
}

/*
 * rsu_client_add_factory_update_image() - add a new factory update image
 * image_name: name of the factory update image
 * slot_name: the selected slot
 * raw: raw file if set, app file if cleared
 *
 * Return: 0 on success, or negative value on error
 */
static int rsu_client_add_factory_update_image(char *image_name, int slot_num)
{
	return rsu_slot_program_factory_update_file(slot_num, image_name);
}

/*
 * rsu_client_erase_image() - erase the application image from a selected slot
 * slot_num: the slot number
 *
 * Return: 0 on success, or negative value on error.
 */
static int rsu_client_erase_image(int slot_num)
{
	return rsu_slot_erase(slot_num);
}

/*
 * rsu_client_verify_data() - verify the data in selected slot compared to file
 * file_name: file name
 * slot_num: the selected slot
 * raw: raw file if set, app file if cleared
 *
 * Return: 0 on success, or negativer on error.
 */
static int rsu_client_verify_data(char *file_name, int slot_num, int raw)
{
	if (raw)
		return rsu_slot_verify_file_raw(slot_num, file_name);

	return rsu_slot_verify_file(slot_num, file_name);
}

/*
 * rsu_client_copy_to_file() - read the data from a slot then write to file
 * file_name: number of file which store the data
 * slot_num: the selected slot
 *
 * Return: 0 on success, or negative on error
 */
static int rsu_client_copy_to_file(char *file_name, int slot_num)
{
	return rsu_slot_copy_to_file(slot_num, file_name);
}

/*
 * rsu_client_display_dcmf_version() - display the version of each of the four
 *				       DCMF copies in flash
 *
 * Return: 0 on success, or negative on error
 */
static int rsu_client_display_dcmf_version(void)
{
	__u32 versions[4];
	int i, ret;

	ret = rsu_dcmf_version(versions);
	if (ret)
		return ret;

	for (i = 0; i < 4; i++)
		printf("DCMF%d version = %d.%d.%d\n", i,
			   (int)DCMF_VERSION_MAJOR(versions[i]),
			   (int)DCMF_VERSION_MINOR(versions[i]),
			   (int)DCMF_VERSION_UPDATE(versions[i]));
	return 0;
}

/*
 * rsu_client_display_dcmf_status() - display the status of each of the four
 *				      DCMF copies in flash
 *
 * Return: 0 on success, or negative on error
 */
static int rsu_client_display_dcmf_status(void)
{
	int status[4];
	int i, ret;

	ret = rsu_dcmf_status(status);
	if (ret)
		return ret;

	for (i = 0; i < 4; i++)
		printf("DCMF%d: %s\n", i, status[i] ? "Corrupted" : "OK");

	return 0;
}

/*
 * rsu_client_display_max_retry() - display the max_retry parameter
 *
 * Return: 0 on success, or negative on error
 */
static int rsu_client_display_max_retry(void)
{
	__u8 value;
	int ret;

	ret = rsu_max_retry(&value);
	if (ret)
		return ret;

	printf("max_retry = %d\n", (int)value);
	return 0;
}

/*
 * rsu_client_check_running_factory() - check if running the factory image
 *
 * Return: 0 on success, or negative on error
 */
static int rsu_client_check_running_factory(void)
{
	int factory;
	int ret;

	ret = rsu_running_factory(&factory);
	if (ret)
		return ret;

	printf("Running factory image: %s\n", factory ? "yes" : "no");
	return 0;
}

static void error_exit(char *msg)
{
	printf("ERROR: %s\n", msg);
	librsu_exit();
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	int index = 0;
	int slot_num = -1;
	int slot_address = -1;
	int slot_size = -1;
	char slot_name[MAX_SLOT_NAME + 1] = "";
	char *endptr;
	int notify_value = -1;
	enum rsu_clinet_command_code command = COMMAND_NONE;
	char *filename = NULL;
	int ret;

	if (argc == 1) {
		rsu_client_usage();
		exit(1);
	}

	ret = librsu_init("");
	if (ret) {
		printf("librsu_init return %d\n", ret);
		return ret;
	}

	while ((c = getopt_long(argc, argv,
				"cghRl:z:p:t:a:u:A:s:e:v:V:f:r:E:D:n:CZmyxd:W:X:bB:P:S:L:k",
				opts, &index)) != -1) {
		switch (c) {
		case 'c':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_SLOT_COUNT;
			break;
		case 'l':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_ATTR;
			slot_num = atoi(optarg);
			break;
		case 'z':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_SIZE;
			slot_num = atoi(optarg);
			break;
		case 'p':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_PRIORITY;
			slot_num = atoi(optarg);
			break;
		case 'E':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_ENABLE;
			slot_num = atoi(optarg);
			break;
		case 'D':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_DISABLE;
			slot_num = atoi(optarg);
			break;
		case 'r':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_LOAD;
			slot_num = atoi(optarg);
			break;
		case 'R':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_FACTORY_LOAD;
			break;
		case 'e':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_ERASE;
			slot_num = atoi(optarg);
			break;
		case 's':
			if (slot_num >= 0)
				error_exit("Slot number already set");
			slot_num = atoi(optarg);
			break;
		case 'a':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_ADD_IMAGE;
			filename = optarg;
			break;
		case 'u':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_ADD_FACTORY_UPDATE_IMAGE;
			filename = optarg;
			break;
		case 'A':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_ADD_RAW_IMAGE;
			filename = optarg;
			break;
		case 'v':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_VERIFY_IMAGE;
			filename = optarg;
			break;
		case 'V':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_VERIFY_RAW_IMAGE;
			filename = optarg;
			break;
		case 'f':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_COPY_TO_FILE;
			filename = optarg;
			break;
		case 'g':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_STATUS_LOG;
			break;
		case 'n':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_NOTIFY;
			notify_value = strtol(optarg, NULL, 0);
			break;
		case 'C':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_CLEAR_ERROR_STATUS;
			break;
		case 'Z':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_RESET_RETRY_COUNTER;
			break;
		case 'm':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_DISPLAY_DCMF_VERSION;
			break;
		case 'y':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_DISPLAY_DCMF_STATUS;
			break;
		case 'x':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_DISPLAY_MAX_RETRY;
			break;
		case 't':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_SLOT_CREATE;
			strncpy(slot_name, optarg, MAX_SLOT_NAME);
			slot_name[MAX_SLOT_NAME] = '\0';
			break;
		case 'd':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			if (slot_num >= 0)
				error_exit("Slot number already set");
			command = COMMAND_SLOT_DELETE;
			slot_num = strtol(optarg, &endptr, 0);
			if (*endptr)
				error_exit("Invalid slot number");
			break;
		case 'S':
			if (slot_address >= 0)
				error_exit("Slot address already set");
			slot_address = strtol(optarg, &endptr, 0);
			if (*endptr)
				error_exit("Invalid slot address");
			break;
		case 'L':
			if (slot_size >= 0)
				error_exit("Slot size already set");
			slot_size = strtol(optarg, &endptr, 0);
			if (*endptr)
				error_exit("Invalid slot size");
			break;
		case 'W':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_RESTORE_SPT;
			filename = optarg;
			break;
		case 'X':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_SAVE_SPT;
			filename = optarg;
			break;
		case 'b':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_CREATE_EMPTY_CPB;
			break;
		case 'B':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_RESTORE_CPB;
			filename = optarg;
			break;
		case 'P':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_SAVE_CPB;
			filename = optarg;
			break;
		case 'k':
			if (command != COMMAND_NONE)
				error_exit("Only one command allowed");
			command = COMMAND_CHECK_RUNNING_FACTORY;
			break;
		case 'h':
			rsu_client_usage();
			librsu_exit();
			exit(0);

		default:
			error_exit("Invalid argument: try -h for help");
		}
	}

	switch (command) {
	case COMMAND_SLOT_COUNT:
		if (slot_num >= 0)
			error_exit("Slot number should not be set");
		ret = rsu_client_get_slot_count();
		if (ret < 0)
			error_exit("Failed to get number of slots");
		printf("number of slots is %d\n", ret);
		break;
	case COMMAND_SLOT_ATTR:
		ret = rsu_client_list_slot_attribute(slot_num);
		if (ret)
			error_exit("Failed to get slot attributes");
		break;
	case COMMAND_SLOT_SIZE:
		ret = rsu_client_get_slot_size(slot_num);
		if (ret < 0)
			error_exit("Failed to get slot size");
		printf("size of slot %d is %d\n", slot_num, ret);
		break;
	case COMMAND_SLOT_PRIORITY:
		ret = rsu_client_get_priority(slot_num);
		if (ret < 0)
			error_exit("Failed to get slot priority");
		printf("priority of slot %d is %d\n", slot_num, ret);
		break;
	case COMMAND_SLOT_ENABLE:
		if (rsu_slot_enable(slot_num))
			error_exit("Failed to enable slot");
		break;
	case COMMAND_SLOT_DISABLE:
		if (rsu_slot_disable(slot_num))
			error_exit("Failed to disable slot");
		break;
	case COMMAND_SLOT_LOAD:
		ret = rsu_client_request_slot_be_loaded(slot_num);
		if (ret)
			error_exit("Failed to request slot loaded");
		break;
	case COMMAND_FACTORY_LOAD:
		if (slot_num >= 0)
			error_exit("Slot number should not be set");
		ret = rsu_client_request_factory_be_loaded();
		if (ret)
			error_exit("Failed to request factory image load");
		break;
	case COMMAND_SLOT_ERASE:
		ret = rsu_client_erase_image(slot_num);
		if (ret)
			error_exit("Failed to erase slot");
		break;
	case COMMAND_ADD_IMAGE:
		if (slot_num < 0)
			error_exit("Slot number must be set");
		ret = rsu_client_add_app_image(filename, slot_num, 0);
		if (ret < 0)
			error_exit("Failed to add application image");
		break;
	case COMMAND_ADD_FACTORY_UPDATE_IMAGE:
		if (slot_num < 0)
			error_exit("Slot number must be set");
		ret = rsu_client_add_factory_update_image(filename, slot_num);
		if (ret < 0)
			error_exit("Failed to add factory update image");
		break;
	case COMMAND_ADD_RAW_IMAGE:
		if (slot_num < 0)
			error_exit("Slot number must be set");
		ret = rsu_client_add_app_image(filename, slot_num, 1);
		if (ret < 0)
			error_exit("Failed to add application image");
		break;
	case COMMAND_VERIFY_IMAGE:
		if (slot_num < 0)
			error_exit("Slot number must be set");
		ret = rsu_client_verify_data(filename, slot_num, 0);
		if (ret < 0)
			error_exit("Failed to verify application image");
		break;
	case COMMAND_VERIFY_RAW_IMAGE:
		if (slot_num < 0)
			error_exit("Slot number must be set");
		ret = rsu_client_verify_data(filename, slot_num, 1);
		if (ret < 0)
			error_exit("Failed to verify application image");
		break;
	case COMMAND_COPY_TO_FILE:
		if (slot_num < 0)
			error_exit("Slot number must be set");
		ret = rsu_client_copy_to_file(filename, slot_num);
		if (ret < 0)
			error_exit("Failed to copy app image to file");
		break;
	case COMMAND_STATUS_LOG:
		if (slot_num >= 0)
			error_exit("Slot number should not be set");
		ret = rsu_client_copy_status_log();
		if (ret)
			error_exit("Failed to read status log");
		break;
	case COMMAND_NOTIFY:
		if (notify_value < 0)
			error_exit("Notify value must be set");
		ret = rsu_notify(notify_value);
		if (ret < 0)
			error_exit("Failed to notify");
		break;
	case COMMAND_CLEAR_ERROR_STATUS:
		ret = rsu_clear_error_status();
		if (ret)
			error_exit("Failed to clear the error status");
		break;
	case COMMAND_RESET_RETRY_COUNTER:
		ret = rsu_reset_retry_counter();
		if (ret)
			error_exit("Failed to reset the retry counter");
		break;
	case COMMAND_DISPLAY_DCMF_VERSION:
		ret = rsu_client_display_dcmf_version();
		if (ret)
			error_exit("Failed to display the dcmf version");
		break;
	case COMMAND_DISPLAY_DCMF_STATUS:
		ret = rsu_client_display_dcmf_status();
		if (ret)
			error_exit("Failed to display the dcmf status");
		break;
	case COMMAND_DISPLAY_MAX_RETRY:
		ret = rsu_client_display_max_retry();
		if (ret)
			error_exit("Failed to display the max_retry parameter");
		break;
	case  COMMAND_SLOT_CREATE:
		if (slot_address < 0)
			error_exit("Slot address value must be set");
		if (slot_size < 0)
			error_exit("Slot size value must be set");
		ret = rsu_slot_create(slot_name, slot_address, slot_size);
		if (ret)
			error_exit("Failed to create the slot");
		break;
	case  COMMAND_SLOT_DELETE:
		ret = rsu_slot_delete(slot_num);
		if (ret)
			error_exit("Failed to delete the slot");
		break;
	case COMMAND_RESTORE_SPT:
		ret = rsu_restore_spt(filename);
		if (ret)
			error_exit("Failed to restore spt from a file");
		break;
	case COMMAND_SAVE_SPT:
		ret = rsu_save_spt(filename);
		if (ret)
			error_exit("Failed to save spt to a file");
		break;
	case COMMAND_CREATE_EMPTY_CPB:
		ret = rsu_create_empty_cpb();
		if (ret)
			error_exit("Failed to create a empty cpb");
		break;
	case COMMAND_RESTORE_CPB:
		ret = rsu_restore_cpb(filename);
		if (ret)
			error_exit("Failed to restore cpb");
		break;
	case COMMAND_SAVE_CPB:
		ret = rsu_save_cpb(filename);
		if (ret)
			error_exit("Failed to save cpb");
		break;
	case COMMAND_CHECK_RUNNING_FACTORY:
		ret = rsu_client_check_running_factory();
		if (ret)
			error_exit("Failed to check if running factory image");
		break;
	default:
		error_exit("No command: try -h for help");
	}

	printf("Operation completed\n");

	librsu_exit();
	return 0;
}
