/* SPDX-License-Identifier: BSD-2-Clause */

/* Intel Copyright 2018 */

#ifndef __LIBRSU_IMAGE_H__
#define __LIBRSU_IMAGE_H__

#include <librsu.h>

/*
 * A bitstream is broken down into 4kB blocks.  The second block ("PTR BLOCK")
 * contains absolute address pointers to the start of the main images within
 * the bitstream.  The second block is protected by a 32-bit CRC that covers
 * the entire second block (4kB).
 *
 * The slot size is used to determine if the bitstream was generated using a
 * slot offset address of zero. The absolute address pointers to the main
 * images are updated when programming into a slot if all of the pointers are
 * less than the slot size.
 */
#define IMAGE_BLOCK_SZ		0x1000	   /* Size of a bitstream data block */
#define IMAGE_PTR_BLOCK		0x1000	   /* Offset to the PTR BLOCK */
#define IMAGE_PTR_START		0x1F00	   /* Offset to main image pointers */
#define IMAGE_PTR_CRC		0x1FFC	   /* Offset to CRC for PTR BLOCK */
#define IMAGE_PTR_END		0x1FFF	   /* End of PTR BLOCK */

/*
 * librsu_image_adjust - Adjust values in the 256 byte pointer
 *                       block for the offset of the slot being
 *                       programmed.  The pointer block is in
 *                       the second 4kB block of the image.  The
 *                       pointer block contains a CRC of the entire 4kB
 *                       block.
 *
 * block - pointer to the start of the second 4kB block of an image
 * info - rsu_slot_info structure for target slot
 *
 * Returns 0 on success and block was adjusted, or -1 on error
 */
int librsu_image_adjust(void *block, struct rsu_slot_info *info);

#endif
