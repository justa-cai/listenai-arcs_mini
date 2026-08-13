/**
 * @file    Driver_WDT.h
 * @brief    Watchdog Timer (WDT) Driver Header File
 * @details This file contains the API definitions and configuration options for the CSK WDT driver.
 *          It includes control flags, status structures, event definitions, and function prototypes.
 * @copyright (c) 2025 ListenAI. All rights reserved.
 */

#ifndef _DRIVER_WDT_H_
#define _DRIVER_WDT_H_

#include "Driver_Common.h"
#include "venusa_ap.h"

/** @defgroup WDT
  * @brief WDT HAL module driver
  * @{
  */
/** @defgroup WDT_Exported_Constants WDT Exported Constants
  * @{
  */
/** @defgroup wdt_flags WDT State Flags
 *  Bitmask flags representing WDT operational states
 *  @{
 */
// WDT flags
#define WDT_STATE_INITIALIZED                (1U << 0)  ///< Device has been initialized
#define WDT_STATE_POWERED                    (1U << 1)  ///< Power supply is enabled
#define WDT_STATE_CONFIGURED                 (1U << 2)  ///< Device has been configured

/** Magic numbers used for special operations */
#define WDT_MAGIC_WRITE_PROTECTION           (0x5AA5)    ///< Write protection magic value
#define WDT_MAGIC_RESTART_VALUE              (0xCAFE)    ///< Restart trigger magic value
/** @} */ /* End of group wdt_flags */
/** @} */ /* End of group WDT_Exported_Constants */
/** @defgroup WDT_Exported_Types WDT Exported Types
  * @{
  */
/** Typedef for WDT event callback function */
typedef void (*HAL_WDT_SignalEvent_t)(void* workspace);  ///< Callback type for WDT events

typedef enum {
    WDT_UNINITIALIZED = 0,          ///< Initial uninitialized state
    WDT_INITIALIZED   = (1U << 0), ///< Successfully initialized
    WDT_POWERED       = (1U << 1), ///< Power domain active
    WDT_CONFIGURED    = (1u << 2), ///< Fully configured state
} WDT_State_t;

typedef struct {
    WDT_State_t state;               ///< Current operational state
    uint8_t busy;                    ///< Busy flag during operations
    uint8_t int_stage;               ///< Interrupt stage counter
    uint8_t reset_stage;             ///< Reset sequence stage
    HAL_WDT_SignalEvent_t callback; ///< Event callback function
    void* workspace;                ///< User context pointer
} WDT_Info_t;

typedef struct {
    WDT_RegDef *reg;                ///< Register block base address
    uint32_t irq_num;               ///< Interrupt number
    void (*irq_handler)(void);     ///< Interrupt handler function
    WDT_Info_t *info;              ///< Pointer to info structure
} WDT_Resources_t;

#define CHECK_RESOURCES(res)    do{ \
    if(res != &wdt0_resources){ \
        return CSK_DRIVER_ERROR_PARAMETER;  \
    }   \
} while(0)

typedef enum {
    hal_driver_wdt_clk_src_32K = 0x0, ///< 32kHz low-power clock source
} HAL_DRIVER_WDT_Clk_Src_t;

typedef enum {
    hal_driver_wdt_rst_time_7 = 0x0, // clock_period * 2^7
    hal_driver_wdt_rst_time_8,       ///< Longer reset time option
    hal_driver_wdt_rst_time_9,
    hal_driver_wdt_rst_time_10,
    hal_driver_wdt_rst_time_11,
    hal_driver_wdt_rst_time_12,
    hal_driver_wdt_rst_time_13,
    hal_driver_wdt_rst_time_14,
} HAL_DRIVER_WDT_Rst_Time_t;

typedef enum {
    hal_driver_wdt_int_time_6 = 0x0, // clock_period * 2^6
    hal_driver_wdt_int_time_8 = 0x1,
    hal_driver_wdt_int_time_10 = 0x2,
    hal_driver_wdt_int_time_11,
    hal_driver_wdt_int_time_12,
    hal_driver_wdt_int_time_13,
    hal_driver_wdt_int_time_14,
    hal_driver_wdt_int_time_15,
    hal_driver_wdt_int_time_17,
    hal_driver_wdt_int_time_19,
    hal_driver_wdt_int_time_21,
    hal_driver_wdt_int_time_23,
    hal_driver_wdt_int_time_25,
    hal_driver_wdt_int_time_27,
    hal_driver_wdt_int_time_29,
    hal_driver_wdt_int_time_31, // clock_period * 2^31
} HAL_DRIVER_WDT_Int_Time_t;

typedef struct {
    HAL_DRIVER_WDT_Clk_Src_t  clk_src;  ///< Clock source selection
    HAL_DRIVER_WDT_Rst_Time_t rst_time; ///< Reset timing configuration
    HAL_DRIVER_WDT_Int_Time_t int_time; ///< Interrupt timing configuration
} HAL_DRIVER_WDT_Cfg_t;
/** @} */ // end of WDT_Exported_Types

/* Exported functions --------------------------------------------------------*/
/** @defgroup WDT_Exported_Functions WDT Exported Functions
  * @{
  */
/** Get WDT instance pointer */
void* WDT(void);

/**
 * @fn WDT_Initialize
 * @brief Initialize WDT device with callback
 * @param[in] res Device instance handle
 * @param[in] callback Event callback function
 * @param[in] workspace User context pointer
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_Initialize(void* res, HAL_WDT_SignalEvent_t callback, void* workspace);

/**
 * @fn WDT_Uninitialize
 * @brief Deinitialize WDT device
 * @param[in] res Device instance handle
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_Uninitialize(void* res);

/**
 * @fn WDT_PowerControl
 * @brief Control power state of WDT device
 * @param[in] res Device instance handle
 * @param[in] state Power state (from CSK_POWER_STATE)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn WDT_Control
 * @brief Configure WDT parameters
 * @param[in] res Device instance handle
 * @param[in] cfg Configuration structure
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_Control(void* res, HAL_DRIVER_WDT_Cfg_t* cfg);

/**
 * @fn WDT_Enable
 * @brief Enable WDT functionality
 * @param[in] res Device instance handle
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_Enable(void* res);

/**
 * @fn WDT_Disable
 * @brief Disable WDT functionality
 * @param[in] res Device instance handle
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_Disable(void* res);

/**
 * @fn WDT_Feed
 * @brief Feed the watchdog timer (prevent timeout)
 * @param[in] res Device instance handle
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 */
int32_t WDT_Feed(void *res);
/** @} */ // end of WDT_Exported_Functions group
/** @} */ // end of WDT group

#endif /* _DRIVER_WDT_H_ */
