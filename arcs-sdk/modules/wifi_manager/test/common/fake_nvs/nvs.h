/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nvs_fs {
	 /** File system offset in flash **/
	off_t offset;
	/** Allocation table entry write address.
	 * Addresses are stored as uint32_t:
	 * - high 2 bytes correspond to the sector
	 * - low 2 bytes are the offset in the sector
	 */
	uint32_t ate_wra;
	/** Data write address */
	uint32_t data_wra;
	/** File system is split into sectors, each sector must be multiple of erase-block-size */
	uint16_t sector_size;
	/** Number of sectors in the file system */
	uint16_t sector_count;
	/** Flag indicating if the file system is initialized */
	bool ready;
	/** Mutex */
	// struct k_mutex nvs_lock;
	/** Flash device runtime structure */
	struct FLASH_DEV *flash_device;
	/** Flash memory parameters structure */
	const struct flash_parameters *flash_parameters;
};



#ifdef __cplusplus
}
#endif
