/*
 * Project:      WDT (watch dog timer)
 *               Driver definitions
 */

#ifndef __DRIVER_WDT_H
#define __DRIVER_WDT_H

#include "Driver_Common.h"

/*
    Timing Diagram for Watchdog Timer (WDT) Events

    +----------------------------------------------------+
    |                      Time                          |
    +----------------------------------------------------+
    |                                                    |
    | [-------------------------------] [---------------]|
    | [         Interrupt             ] [ Reset         ]|
    |                                                    |
    |    wdt_int    _____________________________________|
    |_______________|                                    |
    |                                                    |
    |    wdt_rst                           ______________|
    |                                      |             |
    | _____________________________________|             |
    |                                                    |
    | <-Interrupt Interval-><-- Reset Interval -->       |
    +----------------------------------------------------+
    
    Description:
    - The Interrupt Stage is followed by a Reset Stage.
    - Signals `wdt_int` and `wdt_rst` represent the watchdog timer interrupt and reset signals respectively.
    - `wdt_int` is high during the interrupt stage and low otherwise.
    - `wdt_rst` becomes high at the end of the reset interval.
*/


/****** WDT Control Codes: clock source: 2 bits *****/
typedef enum _hal_driver_wdt_clk_src
{
    hal_driver_wdt_clk_src_32k = 0x0,
    hal_driver_wdt_clk_src_apb = 0x1,
} hal_driver_wdt_clk_src;

typedef enum _hal_driver_wdt_rst_time
{
    hal_driver_wdt_rst_time_7 = 0x0, // clock_period * 2^7
    hal_driver_wdt_rst_time_8,
    hal_driver_wdt_rst_time_9,
    hal_driver_wdt_rst_time_10,
    hal_driver_wdt_rst_time_11,
    hal_driver_wdt_rst_time_12,
    hal_driver_wdt_rst_time_13,
    hal_driver_wdt_rst_time_14,
} hal_driver_wdt_rst_time;

typedef enum _hal_driver_wdt_int_time
{
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
} hal_driver_wdt_int_time;

typedef struct _hal_driver_wdt_cfg_t
{
    hal_driver_wdt_clk_src clk_src;
    hal_driver_wdt_rst_time rst_time;
    hal_driver_wdt_int_time int_time;
} hal_driver_wdt_cfg_t;

typedef void (*HAL_WDT_SignalEvent_t)(void* workspace);

/**
 * @brief Initializes the Watchdog Timer (WDT).
 *
 * This function prepares the Watchdog Timer (WDT) for operation by setting it to a known state and
 * optionally registering a callback function that will be called upon specific WDT events.
 * It typically involves setting the WDT to its default configuration and ensuring that all related resources
 * are properly allocated and configured.
 *
 * @param res A pointer to the resources needed by the WDT, such as base addresses or hardware descriptors.
 * @param callback A function pointer to the callback function that will handle WDT events. This parameter
 *                 can be NULL if no callback is to be used.
 * @param workspace A pointer to a memory region that will be used for WDT operations. This could be used
 *                  to store runtime data, state information, or any other data required by the WDT or
 *                  the callback function.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code indicates the
 *                 nature of the failure in the initialization process.
 */
int32_t WDT_Initialize(void* res, HAL_WDT_SignalEvent_t callback, void* workspace);


/**
 * @brief Uninitializes the Watchdog Timer (WDT).
 *
 * This function disables the WDT and releases the associated resources. It should be called when the WDT
 * is no longer needed, to ensure proper cleanup of resources.
 *
 * @param res A pointer to the resources used by the WDT.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_Uninitialize(void* res);

/**
 * @brief Controls power state of the Watchdog Timer (WDT).
 *
 * This function manages the power modes of the WDT, enabling or disabling it based on the specified power state.
 *
 * @param res A pointer to the WDT resources.
 * @param state The desired power state to be set for the WDT, as defined by CSK_POWER_STATE.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @brief Configures the Watchdog Timer (WDT) with specified settings.
 *
 * This function sets the operational parameters of the WDT, including the clock source, reset time, and
 * interrupt time based on the provided configuration struct.
 *
 * @param res A pointer to the WDT resources.
 * @param cfg Configuration struct containing the new settings for the WDT.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_Control(void* res, hal_driver_wdt_cfg_t* cfg);

/**
 * @brief Updates the clock source for the Watchdog Timer (WDT).
 *
 * This function changes the clock source of the WDT to either an external source or an APB clock as specified.
 *
 * @param rst A pointer to the WDT resources.
 * @param clk_src The new clock source as defined by hal_driver_wdt_clk_src.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_ClkSrc_Update(void* rst, hal_driver_wdt_clk_src clk_src);

/**
 * @brief Updates the interrupt time for the Watchdog Timer (WDT).
 *
 * This function sets the interrupt generation time of the WDT, adjusting how long the WDT waits
 * before generating an interrupt, based on the specified setting.
 *
 * @param res A pointer to the WDT resources.
 * @param int_time The new interrupt time as defined by hal_driver_wdt_int_time.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_IntTime_Update(void* res, hal_driver_wdt_int_time int_time);

/**
 * @brief Updates the reset time for the Watchdog Timer (WDT).
 *
 * This function adjusts the reset time of the WDT, determining the delay before the system is reset
 * after a timeout.
 *
 * @param res A pointer to the WDT resources.
 * @param rst_time The new reset time as defined by hal_driver_wdt_rst_time.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_RstTime_Update(void* res, hal_driver_wdt_rst_time rst_time);

/**
 * @brief Enables the Watchdog Timer (WDT).
 *
 * This function activates the WDT to start monitoring the system. The WDT begins counting down based
 * on its configured parameters and will reset the system unless periodically refreshed.
 *
 * @param res A pointer to the WDT resources.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_Enable(void* res);

/**
 * @brief Refreshes the Watchdog Timer (WDT).
 *
 * This function resets the countdown of the WDT to prevent the system from resetting. It must be called
 * periodically before the countdown expires to keep the system running.
 *
 * @param res A pointer to the WDT resources.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_Refresh(void* res);

/**
 * @brief Disables the Watchdog Timer (WDT).
 *
 * This function stops the WDT from counting down, effectively preventing it from resetting the system.
 * It is typically used during system shutdown or when the WDT is no longer required.
 *
 * @param res A pointer to the WDT resources.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_Disable(void* res);

void* WDT(void);
#endif /* __DRIVER_WDT_H */
