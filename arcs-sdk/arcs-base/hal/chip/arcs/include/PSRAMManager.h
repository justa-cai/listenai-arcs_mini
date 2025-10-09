/*
 * PSRAMManager.h
 *
 *  Created on: 2020年11月17日
 *      Author: USER
 */

#ifndef INCLUDE_DRIVER_PSRAMMANAGER_H_
#define INCLUDE_DRIVER_PSRAMMANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include "arcs_ap.h"

#ifndef PSRAM_LOG_CHECK
#define PSRAM_LOG_CHECK                     1
#endif

#define PSRAM_BASE_ADDRESS                  0x28000000
#define PSRAM_MEM_512Mb_DIE                 64*1024*1024
#define PSRAM_MEM_256Mb_DIE                 32*1024*1024
#define PSRAM_MEM_128Mb_DIE                 16*1024*1024
#define PSRAM_MEM_64Mb_DIE                  8*1024*1024
#define PSRAM_MEM_32Mb_DIE                  4*1024*1024

#define PSRAM_DIE_TYPE_APM                  1202
#define PSRAM_DIE_TYPE_XCCELA               1203

// Configure PSRAM DIE type
#define PSRAM_DIE_TYPE                      PSRAM_DIE_TYPE_XCCELA

// Search Delay Cell
#define PSRAM_INNER_SEARCH_LOOP_NUM         64
#define PSRAM_SEARCH_DQS_NUM                128             // word
#define PSRAM_REFERENCE_ADDRESS             0x20000000UL

// Pre-fetch
#define PSRAM_PREFETCH_EN                   1
typedef enum {
    ahb_master_mcu_cp             = 0, // id-0
    ahb_master_dma_cp             = 1, // id-1
    ahb_master_usbc               = 2, // id-2
    ahb_master_usbd               = 3, // id-3
    ahb_master_smid,                   // id-4
    ahb_master_mcu_ap,                 // id-5
    ahb_master_luna_ins,               // id-6
    ahb_master_luna_dat,               // id-7
    ahb_master_crypto,                 // id-8
    ahb_master_dmac_gp0,               // id-9
    ahb_master_dmac_gp1,               // id-10
    ahb_master_sdio_h,                 // id-11    
    ahb_master_wifi,                   // id-12
    ahb_master_chiper,                 // id-13
} _psram_ahb_master_id_t;
#define PSRAM_PREFETCH_FIFO0_HM              ahb_master_dma_cp
#define PSRAM_PREFETCH_FIFO0_HM_EN           1
#define PSRAM_PREFETCH_FIFO1_HM              ahb_master_luna_dat
#define PSRAM_PREFETCH_FIFO1_HM_EN           1

#define PSRAM_RX_DIFF_EN                     0
#define PSRAM_DRV_STR_EN                     1

// reduce standby current by refreshing only that
// part of the memory array required by the host system
typedef enum {
    full_array_refresh = 0x0,
    bottom_1_2_array_refresh = 0x1,
    bottom_1_4_array_refresh,
    bottom_1_8_array_refresh,
    none_refresh,
    top_1_2_array_refresh,
    top_1_4_array_refresh,
    top_1_8_array_refresh,
} _psram_stanby_ref_pasr_t;

#define PSRAM_REFRESH_PARA                    full_array_refresh

int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search);

uint32_t PSRAM_GetDensity(void);

#endif /* INCLUDE_DRIVER_PSRAMMANAGER_H_ */
