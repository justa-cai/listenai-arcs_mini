#ifndef _PSRAM_UNIFIED_H__
#define _PSRAM_UNIFIED_H__
#include "Driver_Common.h"

#include <stdio.h>
#include <stdlib.h>
#include "venusa_ap.h"

#define PSRAM_UNIFIED_LOG_CHECK                     1

#define PSRAM_UNIFIED_CONTROLLER_DRV_STR            0

#define PSRAM_UNIFIED_FREQ_CORRECTION               1

typedef enum {
    psram_unified_bus_priority_group0 = 0x0,       ///< Lowest priority group
    psram_unified_bus_priority_group1,             ///< Medium-low priority
    psram_unified_bus_priority_group2,             ///< Medium-high priority
    psram_unified_bus_priority_group3              ///< Highest priority group
} _psram_unified_bus_priority_group_t;

/**
 * @brief Master Device Priority Assignments
 *        Assigns each master device to a fixed priority group for bus contention resolution
 */
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER0          psram_unified_bus_priority_group2 ///< CPU Core 0 priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER1          psram_unified_bus_priority_group2 ///< CPU Core 1 priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER2          psram_unified_bus_priority_group2 ///< Luna Data Master priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER3          psram_unified_bus_priority_group2 ///< Luna Instruction Master priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER4          psram_unified_bus_priority_group2 ///< GPDMA2D Master 0 (Write) priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER5          psram_unified_bus_priority_group2 ///< GPDMA2D Master 1 (Read) priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER6          psram_unified_bus_priority_group2 ///< USB Control Block priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER7          psram_unified_bus_priority_group2 ///< SDIO Host Controller priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER8          psram_unified_bus_priority_group2 ///< Command DMA Master 0 priority
#define PSRAM_UNIFIED_BUS_PRIORITY_MASTER9          psram_unified_bus_priority_group2 ///< Command DMA Master 1 priority

typedef enum {
    __psram_unified_die_any = 0,
    __psram_unified_die_xccela,
    __psram_unified_die_apm,
    __psram_unified_die_winbond,
    __psram_unified_die_esmt,
    __psram_unified_die_unknown,
} __psram_unified_die_type_t;

typedef enum {
    __psram_unified_ahb_master_core_0 = 0x0,               ///< CPU Core 0
    __psram_unified_ahb_master_core_1,                     ///< CPU Core 1
    __psram_unified_ahb_master_luna_dat,                   ///< Luna Data Master
    __psram_unified_ahb_master_luna_ins,                   ///< Luna Instruction Master
    __psram_unified_ahb_master_gpdma2d_m0,                 ///< GPDMA2D Master 0 (Write)
    __psram_unified_ahb_master_gpdma2d_m1,                 ///< GPDMA2D Master 1 (Read)
    __psram_unified_ahb_master_usbc,                       ///< USB Control Block
    __psram_unified_ahb_master_sdioh,                      ///< SDIO Host Controller
    __psram_unified_ahb_master_cmndmac_m0,                 ///< Command MAC Master 0
    __psram_unified_ahb_master_cmndmac_m1,                 ///< Command MAC Master 1
} __psram_unified_ahb_master_id_t;

typedef struct {
    __psram_unified_die_type_t die_type;
    uint32_t* read_delay;
    uint32_t* write_delay;
    __psram_unified_ahb_master_id_t fifo0_master;
    __psram_unified_ahb_master_id_t fifo1_master;
    __psram_unified_ahb_master_id_t fifo2_master;
    uint8_t fifo0_enable;
    uint8_t fifo1_enable;
    uint8_t fifo2_enable;
    uint8_t search;
} __psram_unified_init_t;

int32_t PSRAMUnified_Initialize(__psram_unified_init_t* init_params);

int32_t PSRAMUnified_GetInformation(uint32_t* density, __psram_unified_die_type_t* die_type);

typedef enum {
    __psram_unified_sleep_mode_none = 0x0,           ///< Active mode (no power savings)
    __psram_unified_sleep_mode_half_sleep,           ///< Partial circuit disablement
    __psram_unified_sleep_mode_deep_sleep            ///< Maximal power reduction
} __psram_unified_sleep_mode_t;

int32_t PSRAMUnified_EnterSleepMode(__psram_unified_sleep_mode_t sleep_mode);

int32_t PSRAMUnified_ExitSleepMode(__psram_unified_init_t* init_params, uint32_t density);

#endif /* _PSRAM_UNIFIED_H__ */