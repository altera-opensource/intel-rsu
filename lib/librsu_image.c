// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include "librsu_cfg.h"
#include "librsu_image.h"
#include <zlib.h>
#include <string.h>

static void swap_bits(char *data, int len)
{
	int x, y;
	char tmp;

	for (x = 0; x < len; x++) {
		tmp = 0;
		for (y = 0; y < 8; y++) {
			tmp <<= 1;
			if (data[x] & 1)
				tmp |= 1;
			data[x] >>= 1;
		}
		data[x] = tmp;
	}
}

static __u32 swap_endian32(__u32 val)
{
	__u32 rtn;
	char *from = (char *)&val;
	char *to = (char *)&rtn;
	int x;

	for (x = 0; x < 4; x++)
		to[x] = from[3 - x];

	return rtn;
}

struct pointer_block {
	__u32 RSVD0;
	__u32 RSVD1;
	__u64 ptrs[4];
	__u8 RSVD2[0xd4];
	__u32 crc;
};


/**
 * find_section() - search section in the current list of identified sections
 * @state: current state machine state
 * @section: section to be searched
 *
 * Return: 1 if section is found, 0 if section is not found
 */
static int find_section(struct rsu_image_state *state, __u64 section)
{
	int x;

	for (x = 0; x < state->no_sections; x++)
		if (section == state->sections[x])
			return 1;

	return 0;
}

/**
 * add_section() - add section to the current list of identified sections
 * @state: current state machine state
 * @section: section to be added
 *
 * Return: zero value for success, or negative value on error
 */
static int add_section(struct rsu_image_state *state, __u64 section)
{
	if (find_section(state, section))
		return 0;

	if (state->no_sections >= MAX_SECTIONS)
		return -1;

	state->sections[state->no_sections++] = section;

	return 0;
}

/**
 * sig_block_process() - process signature block
 * @state: current state machine state
 * @block: signature block
 * @info: slot where the data will be written
 *
 * Determine if the signature block is part of an absolute image, and add its
 * section pointers to the list of identified sections.
 *
 * Return: zero value for success, or negative value on error
 */
static int sig_block_process(struct rsu_image_state *state,	void *block,
			     struct rsu_slot_info *info)
{
	char *data = (char *)block;
	struct pointer_block *ptr_blk = (struct pointer_block *)(data
					+ SIG_BLOCK_PTR_OFFS);
	int x;

	/* Determine if absolute image - only done for 2nd block in an image
	 * which is always a signature block
	 */
	if (state->offset == IMAGE_BLOCK_SZ)
		for (x = 0; x < 4; x++)
			if (ptr_blk->ptrs[x] > (__u64)info->size) {
				state->absolute = 1;
				librsu_log(HIGH, __func__,
					   "Identified absolute image.");
				break;
			}

	/* Add pointers to list of identified sections */
	for (x = 0; x < 4; x++)
		if (ptr_blk->ptrs[x]) {
			if (state->absolute)
				add_section(state, ptr_blk->ptrs[x] -
					    info->offset);
			else
				add_section(state, ptr_blk->ptrs[x]);
		}

	return 0;
}

/**
 * sig_block_adjust() - adjust signature block pointers before writing to flash
 * @state: current state machine state
 * @block: signature block
 * @info: slot where the data will be written
 *
 * This function checks that the section pointers are consistent, and for non-
 * absolute images it updates them to match the destination slot, also re-
 * computing the CRC.
 *
 * Return: zero value for success, or negative value on error
 */
static int sig_block_adjust(struct rsu_image_state *state, void *block,
			    struct rsu_slot_info *info)
{
	__u32 calc_crc;
	int x;
	char *data = (char *)block;
	struct pointer_block *ptr_blk = (struct pointer_block *)(data
					+ SIG_BLOCK_PTR_OFFS);

	/*
	 * Check CRC on 4kB block before proceeding.  All bytes must be
	 * bit-swapped before they can used in zlib CRC32 library function.
	 * The CRC value is stored in big endian in the bitstream.
	 */
	swap_bits(block, IMAGE_BLOCK_SZ);
	calc_crc = crc32(0, block, SIG_BLOCK_CRC_OFFS);
	if (swap_endian32(ptr_blk->crc) != calc_crc) {
		librsu_log(LOW, __func__,
			   "Error: Bad CRC32. Calc = %08X / From Block = %08x",
			   calc_crc, swap_endian32(ptr_blk->crc));
		return -1;
	}
	swap_bits(block, IMAGE_BLOCK_SZ);

	/* Check pointers */
	for (x = 0; x < 4; x++) {
		__s64 ptr = ptr_blk->ptrs[x];

		if (!ptr)
			continue;

		if (state->absolute)
			ptr -= info->offset;

		if (ptr > info->size) {
			librsu_log(LOW, __func__,
				   "Error: A pointer not within the slot");
			return -1;
		}
	}

	/* Absolute images do not require pointer updates */
	if (state->absolute)
		return 0;

	/* Update pointers */
	for (x = 0; x < 4; x++) {
		if (ptr_blk->ptrs[x]) {
			__u64 old =  ptr_blk->ptrs[x];

			ptr_blk->ptrs[x] += info->offset;
			librsu_log(HIGH, __func__,
				   "Adjusting pointer 0x%llx -> 0x%llx.",
				   old, ptr_blk->ptrs[x]);
		}
	}

	/* Update CRC in block */
	swap_bits(block, IMAGE_BLOCK_SZ);
	calc_crc = crc32(0, block, SIG_BLOCK_CRC_OFFS);
	ptr_blk->crc = swap_endian32(calc_crc);
	swap_bits(block, IMAGE_BLOCK_SZ);

	return 0;
}

/**
 * block_compare() - compare two image blocks
 * @state: current state machine state
 * @block: input data provided by user
 * @vblock: verification data read from flash
 *
 * Return: non-negative value for successful comparisor, or negative value on
 * failure or comparison difference found.
 */
static int block_compare(struct rsu_image_state *state, void *block,
			 void *vblock)
{
	char *buf = (char *)block;
	char *vbuf = (char *)vblock;
	int x;

	for (x = 0; x < IMAGE_BLOCK_SZ; x++)
		if (vbuf[x] != buf[x]) {
			librsu_log(HIGH, __func__,
				"Expect %02X, got %02X @0x%08X", buf[x],
				vbuf[x], state->offset + x);
			return -ECMP;
		}

	return 0;
}

/**
 * sig_block_compare() - compare two signature blocks
 * @state: current state machine state
 * @ublock: input data provided by user
 * @vblock: verification data read from flash
 * @info: slot where the verification data was read from
 *
 * Absolute images are compared directly, while for non-absolute images the
 * pointers and associated CRC are re-computed to see if they match.
 *
 * Return: zero for success, or negative value on erorr or finding differences.
 */
static int sig_block_compare(struct rsu_image_state *state, void *ublock,
			     void *vblock, struct rsu_slot_info *info)
{
	__u32 calc_crc;
	int x;
	char block[IMAGE_BLOCK_SZ];
	struct pointer_block *ptr_blk = (struct pointer_block *)(block +
		SIG_BLOCK_PTR_OFFS);

	librsu_log(HIGH, __func__, "Comparing signature block @0x%08x",
		   state->offset);

	/* Make a copy of the data provided by the user */
	memcpy(block, ublock, IMAGE_BLOCK_SZ);

	/* Update signature block to match what we expect in flash */
	if (!state->absolute) {

		/* Update pointers */
		for (x = 0; x < 4; x++)
			if (ptr_blk->ptrs[x])
				ptr_blk->ptrs[x] += info->offset;

		/* Update CRC in block */
		swap_bits(block, IMAGE_BLOCK_SZ);
		calc_crc = crc32(0, (unsigned char *)block, SIG_BLOCK_CRC_OFFS);
		ptr_blk->crc = swap_endian32(calc_crc);
		swap_bits(block, IMAGE_BLOCK_SZ);
	}

	return block_compare(state, block, vblock);
}


int librsu_image_block_init(struct rsu_image_state *state)
{
	librsu_log(HIGH, __func__, "Resetting image block state machine.");

	state->no_sections = 1;
	add_section(state, 0);
	state->block_type = REGULAR_BLOCK;
	state->absolute = 0;
	state->offset = -IMAGE_BLOCK_SZ;

	return 0;
}

int librsu_image_block_process(struct rsu_image_state *state, void *block,
			       void *vblock, struct rsu_slot_info *info)
{
	__u32 magic;

	state->offset += IMAGE_BLOCK_SZ;

	if (find_section(state, state->offset))
		state->block_type = SECTION_BLOCK;

	switch (state->block_type) {

	case SECTION_BLOCK:
		magic = *(__u32 *)block;
		if (magic == CMF_MAGIC) {
			librsu_log(HIGH, __func__, "Found CMF section @0x%08x.",
				   state->offset);
			state->block_type = SIGNATURE_BLOCK;
		} else {
			state->block_type = REGULAR_BLOCK;
		}

		if (vblock)
			return block_compare(state, block, vblock);
		break;

	case SIGNATURE_BLOCK:
		librsu_log(HIGH, __func__, "Found signature block @0x%08x.",
			   state->offset);

		if (sig_block_process(state, block, info))
			return -1;

		state->block_type = REGULAR_BLOCK;

		if (vblock)
			return sig_block_compare(state, block, vblock, info);

		if (sig_block_adjust(state, block, info))
			return -1;

		break;

	case REGULAR_BLOCK:
		break;
	}

	if (vblock)
		return block_compare(state, block, vblock);

	return 0;
}
