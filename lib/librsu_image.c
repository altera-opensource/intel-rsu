// SPDX-License-Identifier: BSD-2-Clause

/* Intel Copyright 2018 */

#include "librsu_cfg.h"
#include "librsu_image.h"
#include <zlib.h>

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
	__u32 num_ptrs;
	__u32 RSVD0;
	__u64 ptrs[4];
	__u8 RSVD1[0xd4];
	__u32 crc;
};

int librsu_image_adjust(void *block, struct rsu_slot_info *info)
{
	__u32 calc_crc;
	int x;
	char *data = (char *)block;
	struct pointer_block *ptr_blk = (struct pointer_block *)(data
					+ IMAGE_PTR_START
					- IMAGE_PTR_BLOCK);

	/*
	 * Check CRC on 4kB block before proceeding.  All bytes must be
	 * bit-swapped before they can used in zlib CRC32 library function.
	 * The CRC value is stored in big endian in the bitstream.
	 */
	swap_bits(block, IMAGE_BLOCK_SZ);
	calc_crc = crc32(0, block, IMAGE_PTR_CRC - IMAGE_BLOCK_SZ);
	if (swap_endian32(ptr_blk->crc) != calc_crc) {
		librsu_log(LOW, __func__,
			   "error: Bad CRC32. Calc = %08X / From Block = %08x",
			   calc_crc, swap_endian32(ptr_blk->crc));
		return -1;
	}
	swap_bits(block, IMAGE_BLOCK_SZ);

	/* Adjust main image pointers after they pass a validity test.
	 * Return -1 if an error is found, or 0 if the block looks OK
	 * (adjusted or not adjusted)
	 */
	if (ptr_blk->num_ptrs == 0)
		return 0;

	if (ptr_blk->num_ptrs > 4) {
		librsu_log(LOW, __func__,
			   "Invalid number of pointers in block");
		return -1;
	}

	for (x = 0; x < ptr_blk->num_ptrs; x++) {
		if (ptr_blk->ptrs[x] > (__u64)info->size) {
			librsu_log(MED, __func__,
				   "A pointer is > 0x%llX, so not adjusting",
				   (__u64)info->size);

			for (x = 0; x < ptr_blk->num_ptrs; x++) {
				if (ptr_blk->ptrs[x] < info->offset ||
				    ptr_blk->ptrs[x] >=
				    (info->offset + info->size)) {
					librsu_log(LOW, __func__,
						   "error: A pointer is not within the slot");
					return -1;
				}
			}

			return 0;
		}
	}

	for (x = 0; x < ptr_blk->num_ptrs; x++)
		ptr_blk->ptrs[x] += info->offset;

	/* Update CRC in block */
	swap_bits(block, IMAGE_BLOCK_SZ);
	calc_crc = crc32(0, block, IMAGE_PTR_CRC - IMAGE_BLOCK_SZ);
	ptr_blk->crc = swap_endian32(calc_crc);
	swap_bits(block, IMAGE_BLOCK_SZ);

	return 0;
}
