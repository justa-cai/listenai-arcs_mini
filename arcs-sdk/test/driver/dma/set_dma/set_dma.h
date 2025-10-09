/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h>
#include <stdbool.h>
#include "nos_timer.h"
#include "arcs_ap.h"
#include <nmsis_core.h> // for Nuclei CORE
#include "dma.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LEN 1024
#define TOTAL_BYTES ((MAX_LEN-1) * 4)

void set_dma_mem_init(uint32_t total_bytes);
void set_dma_mem_uninit(uint32_t total_bytes);
bool set_dma_memcpy(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes, DMA_CACHE_SYNC cache_sync);
bool set_dma_configure(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                         uint32_t dst_width, uint32_t dst_bsize,
                         bool addr_dec, DMA_CACHE_SYNC cache_sync);
bool set_dma_configure_polling(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                                   uint32_t dst_width, uint32_t dst_bsize,
                                   bool addr_dec, DMA_CACHE_SYNC cache_sync);
bool set_dma_configure_srcgather(uint8_t *pch, DMA_CACHE_SYNC cache_sync);
bool set_dma_configure_dstscatter(uint8_t *pch, DMA_CACHE_SYNC cache_sync);
bool set_dma_configure_suspen(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_susp_only);
bool set_dma_configure_suspresm(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_susp_only);
bool set_dma_configure_dis(uint8_t *pch, DMA_CACHE_SYNC cache_sync, bool chk_dis_only);

#ifdef __cplusplus
}
#endif