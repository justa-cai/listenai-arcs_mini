/*
 * @file PSRAMManager.h
 *
 * @date Created on: 2025.06.27
 *      Author: USER
 *
 *  @brief PSRAM (Pseudo-Static RAM) Management Driver Head File
 *         This file contains configuration parameters, data structures, and API functions
 *         for managing PSRAM hardware resources including initialization, power management,
 *         bus arbitration, and memory density detection.
 */

#ifndef _DRIVER_PSRAMMANAGER_H_
#define _DRIVER_PSRAMMANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include "venusa_ap.h"

 /** @defgroup PSRAM
   * @brief PSRAM HAL module driver
   * @{
   */

/* Exported macro ------------------------------------------------------------*/
/** @defgroup PSRAM_Exported_Macro PSRAM Exported Macro
  * @{
  */
/**
 * @brief Log verification enable flag
 *        When set to 1, enables additional log checks during PSRAM operations
 */
#ifndef PSRAM_LOG_CHECK
#define PSRAM_LOG_CHECK                     1
#endif

/**
 * @brief Base physical address of PSRAM memory region
 *        All PSRAM accesses start from this memory mapped location
 */
#define PSRAM_BASE_ADDRESS                  0x38000000

/**
 * @brief Supported PSRAM Die Manufacturers ID Codes
 *        Each code corresponds to a specific silicon vendor's implementation
 */
#define PSRAM_DIE_TYPE_APM                  1202          // Apmemory Technology
#define PSRAM_DIE_TYPE_XCCELA               1203          // Xcela Technologies
#define PSRAM_DIE_TYPE_WINBOND              1204          // Winbond Electronics
#define PSRAM_DIE_TYPE_ESMT                 1205          // ESMT Corporation

/**
 * @brief Currently selected PSRAM die type configuration
 *       Must match one of the defined manufacturer ID codes above
 */
#define PSRAM_DIE_TYPE                      PSRAM_DIE_TYPE_APM

/**
 * @brief Timing Configuration Parameters
 *        Controls search algorithm behavior during initialization phase
 */
#define PSRAM_INNER_SEARCH_LOOP_NUM         64           // Number of internal search iterations
#define PSRAM_SEARCH_DQS_NUM                128          // Data strobe samples per word during search
/** @} */ /* End of group PSRAM_Exported_Macro */

/** @defgroup PSRAM_Exported_Types PSRAM Exported Types
  * @{
  */
/**
 * @brief AHB Master Device IDs
 *        Identifies system components that can act as bus masters to access PSRAM
 */
typedef enum {
    ahb_master_core_0 = 0x0,               ///< CPU Core 0
    ahb_master_core_1,                     ///< CPU Core 1
    ahb_master_luna_dat,                   ///< Luna Data Master
    ahb_master_luna_ins,                   ///< Luna Instruction Master
    ahb_master_gpdma2d_m0,                 ///< GPDMA2D Master 0 (Write)
    ahb_master_gpdma2d_m1,                 ///< GPDMA2D Master 1 (Read)
    ahb_master_usbc,                       ///< USB Control Block
    ahb_master_sdioh,                      ///< SDIO Host Controller
    ahb_master_cmndmac_m0,                 ///< Command DMA Master 0
    ahb_master_cmndmac_m1,                 ///< Command DMA Master 1
} _psram_ahb_master_id_t;

/**
 * @brief Prefetch FIFO Master Assignments
 *        Maps high-speed FIFOs to specific master devices for performance optimization
 */
#define PSRAM_MASTER_FIFO0_HM               ahb_master_luna_dat   ///< Luna Data uses FIFO0
#define PSRAM_PREFETCH_FIFO0_HM_EN          1                    ///< Enable prefetch for FIFO0
#define PSRAM_MASTER_FIFO1_HM               ahb_master_cmndmac_m0 ///< CMDMAC M0 uses FIFO1
#define PSRAM_PREFETCH_FIFO1_HM_EN          1                    ///< Enable prefetch for FIFO1
#define PSRAM_MASTER_FIFO2_HM               ahb_master_gpdma2d_m1 ///< GPDMA2D M1 uses FIFO2
#define PSRAM_PREFETCH_FIFO2_HM_EN          1                    ///< Enable prefetch for FIFO2

/**
 * @brief PSRAM Bus Priority Groups
 *        Defines static priority levels for different master devices sharing the bus
 */
typedef enum {
    psram_bus_priority_group0 = 0x0,       ///< Lowest priority group
    psram_bus_priority_group1,             ///< Medium-low priority
    psram_bus_priority_group2,             ///< Medium-high priority
    psram_bus_priority_group3              ///< Highest priority group
} _psram_bus_priority_group_t;

/**
 * @brief Master Device Priority Assignments
 *        Assigns each master device to a fixed priority group for bus contention resolution
 */
#define PSRAM_BUS_PRIORITY_MASTER0          psram_bus_priority_group2 ///< CPU Core 0
#define PSRAM_BUS_PRIORITY_MASTER1          psram_bus_priority_group2 ///< CPU Core 1
#define PSRAM_BUS_PRIORITY_MASTER2          psram_bus_priority_group2 ///< Luna Data Master
#define PSRAM_BUS_PRIORITY_MASTER3          psram_bus_priority_group2 ///< Luna Instruction Master
#define PSRAM_BUS_PRIORITY_MASTER4          psram_bus_priority_group2 ///< GPDMA2D Master 0 (Write)
#define PSRAM_BUS_PRIORITY_MASTER5          psram_bus_priority_group2 ///< GPDMA2D Master 1 (Read)
#define PSRAM_BUS_PRIORITY_MASTER6          psram_bus_priority_group2 ///< USB Control Block
#define PSRAM_BUS_PRIORITY_MASTER7          psram_bus_priority_group2 ///< SDIO Host Controller
#define PSRAM_BUS_PRIORITY_MASTER8          psram_bus_priority_group2 ///< Command DMA Master 0
#define PSRAM_BUS_PRIORITY_MASTER9          psram_bus_priority_group2 ///< Command DMA Master 1
/**
 * @brief Differential Signaling Control
 *        Disables receiver differential signaling path (single-ended operation)
 */
#define PSRAM_RX_DIFF_EN                     0

/**
 * @brief Driver Strength Control
 *        Disables programmable driver strength adjustment
 */
#define PSRAM_DRV_STR_EN                     0

/**
 * @brief Standby Refresh Mode Options
 *        Controls memory refresh strategy during low-power modes to minimize current consumption
 */
typedef enum {
    full_array_refresh = 0x0,              ///< Full memory array refresh
    bottom_1_2_array_refresh,              ///< Bottom half of array refresh
    bottom_1_4_array_refresh,              ///< Bottom quarter of array refresh
    bottom_1_8_array_refresh,              ///< Bottom eighth of array refresh
    none_refresh,                          ///< No automatic refresh
    top_1_2_array_refresh,                 ///< Top half of array refresh
    top_1_4_array_refresh,                 ///< Top quarter of array refresh
    top_1_8_array_refresh                  ///< Top eighth of array refresh
} _psram_stanby_ref_pasr_t;

/**
 * @brief Default standby refresh configuration
 *       Determines which portion of memory gets refreshed during sleep modes
 */
#define PSRAM_REFRESH_PARA                  full_array_refresh

/**
 * @brief PSRAM Sleep Mode Options
 *        Allows progressive power saving by disabling unused memory subsystems
 */
typedef enum {
    PSRAM_SLEEP_MODE_NONE = 0x0,           ///< Active mode (no power savings)
    PSRAM_SLEEP_MODE_HALF_SLEEP,           ///< Partial circuit disablement
    PSRAM_SLEEP_MODE_DEEP_SLEEP            ///< Maximal power reduction
} _psram_sleep_mode_t;
/** @} */ /* End of group PSRAM_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup PSRAM_Exported_Functions PSRAM Exported Functions
  * @{
  */
/**
 * @brief Initialize PSRAM subsystem
 *        Performs hardware initialization with configurable timing parameters
 *        @param[out] read_delay Pointer to store calculated read latency cycles
 *        @param[out] write_delay Pointer to store calculated write latency cycles
 *        @param[in] search Search depth parameter for timing optimization
 *        @return int32_t Returns 0 on success, negative error code otherwise
 */
int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search);

/**
 * @brief Get total PSRAM memory density
 *        Returns detected total memory size in bytes
 *        @return uint32_t Total usable PSRAM size in bytes
 */
uint32_t PSRAM_GetDensity(void);

/**
 * @brief Enter specified PSRAM sleep mode
 *        Gradually reduces power consumption based on selected sleep level
 *        @param[in] sleep_mode Desired sleep mode from _psram_sleep_mode_t enumeration
 */
void PSRAM_EnterSleepMode(_psram_sleep_mode_t sleep_mode);

/**
 * @brief Exit PSRAM sleep mode
 *        Restores full functionality after low-power state
 */
void PSRAM_ExitSleepMode(void);
/** @} */ // end of PSRAM_Exported_Functions group
/** @} */ /* End of group PSRAM */

#endif /* _DRIVER_PSRAMMANAGER_H_ */
