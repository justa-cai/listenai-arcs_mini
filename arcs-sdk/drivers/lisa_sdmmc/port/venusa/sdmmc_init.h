/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDMMC_SECTOR_SIZE 512U

int sdmmc_platform_init(void);
void sdmmc_platform_reset(void);
int sdmmc_hard_init(void);
bool sdmmc_card_exists(void);

int sdmmc_read_sectors(uint32_t sector, uint32_t count, void *buffer);
int sdmmc_write_sectors(uint32_t sector, uint32_t count, const void *buffer);
int sdmmc_get_sector_count(uint32_t *sector_count);
int sdmmc_get_erase_block_size(uint32_t *erase_block_sectors);

void sdmmc_cache_invalidate(void *buffer, uint32_t length);
void sdmmc_cache_flush(const void *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif
