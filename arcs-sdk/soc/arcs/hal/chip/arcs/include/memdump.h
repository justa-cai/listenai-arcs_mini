/**
 ****************************************************************************************
 *
 * @file memdump.h
 *
 * @brief Memory dump modules
 *
 * Copyright (C) Listenai.com 2023
 *
 *
 ****************************************************************************************
 */

#ifndef _MEM_DUMP_H_
#define _MEM_DUMP_H_

/**
 ****************************************************************************************
 * @addtogroup MEMDUMP
 * @{
 * @name mem dump api
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "arcs_ap.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
#ifndef _EXT_RAM
#define _EXT_RAM __attribute__ ((section (".ramcode")))
#endif

//#define WIFI_MAC_CORE_BASE     0x4B700000
#define WIFI_MAC_CORE_SIZE       0x1000
//#define WIFI_MAC_PL_BASE       0x4B708000
#define WIFI_MAC_PL_SIZE         0x1000
//#define NEW_DFE_BASE           0x4BA00000
#define NEW_DFE_SIZE             0x1000
#define WIFI_MDMCFG_BASE         0x4B800000
#define WIFI_MDMCFG_SIZE         0x1000
//#define RF_IF_BASE             0x47A00000
#define RF_IF_SIZE               0x1000
//#define WIFI_MACBYPASS_BASE    0x4B900000
#define WIFI_MACBYPASS_SIZE      0x1000
//#define WF_CTRL_BASE           0x4BB00000
#define WF_CTRL_SIZE             0x1000
//#define WIFI_CRM_BASE          0x4B400008
#define WIFI_CRM_SIZE            0x1000

#define DEV_FLASH_SIZE             0x400000
#define MEM_DUMP_SIZE              (CMN_RAM0_REGION_SIZE + CMN_RAM1_REGION_SIZE + \
                                    WIFI_RAM_REGION_SIZE + \
                                    WIFI_MAC_CORE_SIZE + WIFI_MAC_PL_SIZE + NEW_DFE_SIZE + WIFI_MDMCFG_SIZE + \
                                    RF_IF_SIZE + WIFI_MACBYPASS_SIZE + WF_CTRL_SIZE + WIFI_CRM_SIZE)

#define FLASH_MDUMP_BASE    (CMN_FLASH_REGION + DEV_FLASH_SIZE - MEM_DUMP_SIZE)

#define BUNDLE_SIZE_IN_LOG2 3
#define MAGIC_PATTERN (0xd47f2583)

/*
 * DEFINES
 ****************************************************************************************
 */
typedef enum {
    MDUMP_PATH_UART = 0,        // dump memory via UART
    MDUMP_PATH_FLASH,           // dump memory via FLASH
    MDUMP_PATH_SDIO,            // dump memory via SDIO
    MDUMP_PATH_USB,             // dump memory via USB
} MDUMP_PATH;

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t len;
} MDUMP_ENTRY;
/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
int32_t memdump_process(MDUMP_PATH path);
/// @} mem dump api
/// @} MEMDUMP

#endif // _MEM_DUMP_H_
