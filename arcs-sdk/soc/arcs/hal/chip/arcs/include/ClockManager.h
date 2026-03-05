/**
  ******************************************************************************
  * @file    ClockManager.h
  * @author  ListenAI Application Team
  * @brief   Header file of CRM HAL module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef INCLUDE_DRIVER_CLOCKMANAGER_H_
#define INCLUDE_DRIVER_CLOCKMANAGER_H_

/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"
#include "clock_config.h"
#include "arcs_ap.h"

/**
 * @enum _clock_src_name
 * @brief Enumeration of available clock sources in the system.
 *
 * This enumeration defines the different clock sources that can be used in the system.
 * It is used by various functions to select or identify the clock source for specific
 * operations or configurations. Each enumerator represents a unique clock source.
 */
typedef enum _clock_src_name {
    CRM_IpSrcInvalide       = 0x0U,
    CRM_IpSrcCoreClk,
    CRM_IpSrcPsramClk,
    CRM_IpSrcXtalClk,
    CRM_IpSrcPeriClk,
    CRM_IpSrcFlashClk,
    CRM_IpSrcCmn32kClk,
    CRM_IpSrcAon32kClk,
    CRM_IpSrcBBPLLCoreClk,
}clock_src_name_t;

typedef enum _bbpll_clock_src_core_div_t {
  BBPLL_CRM_IpCore_96Mhz = 0,
  BBPLL_CRM_IpCore_160Mhz = 1,
  BBPLL_CRM_IpCore_240Mhz = 2,
  BBPLL_CRM_IpCore_960Mhz = 3,
} bbpll_clock_src_core_div_t;

typedef enum _clock_src_core_div {
    CRM_IpCore_300MHz = 0,
    CRM_IpCore_240MHz = 1,
    CRM_IpCore_200MHz = 2,
    CRM_IpCore_150MHz = 3,
    CRM_IpCore_133MHz = 4,
    CRM_IpCore_120MHz = 5,
    CRM_IpCore_100MHz = 6,
} clock_src_core_div_t;
typedef enum _clock_src_psram_div {
    CRM_IpPsram_240MHz = 0,
    CRM_IpPsram_200MHz = 1,
    CRM_IpPsram_150MHz = 2,
    CRM_IpPsram_133MHz = 3,
    CRM_IpPsram_120MHz = 4,
	CRM_IpPsram_100MHz = 5,
} clock_src_psram_div_t;
typedef enum _clock_src_peri_div {
    CRM_IpPeri_100MHz = 0,
    CRM_IpPeri_75MHz = 1,
    CRM_IpPeri_50MHz = 2,
} clock_src_peri_div_t;
typedef enum _clock_src_flash_div {
    CRM_IpFlash_200MHz = 0,
    CRM_IpFlash_150MHz = 1,
    CRM_IpFlash_120MHz = 2,
    CRM_IpFlash_100MHz = 3,
} clock_src_flash_div_t;

/**********************************DEVICE************************************/
/** @defgroup _CRM_PSRAM PSRAM_CLK_FUNC
  * @brief PSRAM clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for PSRAM.
 *
 * This macro enables the clock for the PSRAM module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to PSRAM, allowing the module to operate. This macro should be called before
 * initializing or using PSRAM to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_PSRAM_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the PSRAM clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the PSRAM module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_PSRAM_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_PSRAM_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for PSRAM.
 *
 * This macro disables the clock for the PSRAM module.
 * Disabling the clock can be useful in power-saving modes or when the PSRAM module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to PSRAM.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that PSRAM
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          PSRAM operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_PSRAM_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_PSRAM_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the PSRAM clock is enabled.
 *
 * This inline static function determines whether the clock for the PSRAM module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of PSRAM before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the PSRAM clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_PsramClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_PSRAM_CLK;
}/**
 * @brief Sets the clock divider for Psram.
 *
 * This function configures the clock division for Psram by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Psram clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 31].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Psram specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetPsramClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for PSRAM.
 *
 * This inline static function obtains the current clock source and division factors for the
 * PSRAM module. The clock source and divider values are returned
 * through the pointer parameters  'div_m'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetPsramClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */

inline static void HAL_CRM_GetPsramClkConfig(uint32_t* div_m){
    if (div_m == NULL) {
        return;
    }
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of PSRAM.
 *
 * This function returns the frequency (in Hz) at which the PSRAM
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the PSRAM clock divider settings.
 *
 * @return uint32_t The operating frequency of PSRAM in Hertz. If the frequency cannot be
 *                  determined, or if PSRAM is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the PSRAM divider settings. Changes in these parameters can affect
 *       the PSRAM frequency.
 *
 * @warning Ensure that PSRAM and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetPsramFreq();/**
  * @}
  */

/** @defgroup _CRM_FLASH FLASH_CLK_FUNC
  * @brief FLASH clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for FLASH.
 *
 * This macro enables the clock for the FLASH module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to FLASH, allowing the module to operate. This macro should be called before
 * initializing or using FLASH to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_FLASH_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the FLASH clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the FLASH module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_FLASH_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_FLASH_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for FLASH.
 *
 * This macro disables the clock for the FLASH module.
 * Disabling the clock can be useful in power-saving modes or when the FLASH module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to FLASH.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that FLASH
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          FLASH operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_FLASH_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_FLASH_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the FLASH clock is enabled.
 *
 * This inline static function determines whether the clock for the FLASH module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of FLASH before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the FLASH clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_FlashClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_FLASH_CLK;
}/**
 * @brief Sets the clock source for Flash.
 *
 * This function configures Flash to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Flash clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Flash. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcFlashClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Flash
 *       and the system's clock configuration. It is advisable to ensure that Flash is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Flash can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Flash's operational requirements.
 */
uint32_t HAL_CRM_SetFlashClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Flash.
 *
 * This function configures the clock division for Flash by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Flash clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 31].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Flash specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetFlashClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for FLASH.
 *
 * This inline static function obtains the current clock source and division factors for the
 * FLASH module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetFlashClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetFlashClkConfig(clock_src_name_t* src, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.SEL_FLASH_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcFlashClk;
    }
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_FLASH_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of FLASH.
 *
 * This function returns the frequency (in Hz) at which the FLASH
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the FLASH clock divider settings.
 *
 * @return uint32_t The operating frequency of FLASH in Hertz. If the frequency cannot be
 *                  determined, or if FLASH is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the FLASH divider settings. Changes in these parameters can affect
 *       the FLASH frequency.
 *
 * @warning Ensure that FLASH and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetFlashFreq();/**
  * @}
  */

/** @defgroup _CRM_MTIME MTIME_CLK_FUNC
  * @brief MTIME clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for MTIME.
 *
 * This macro enables the clock for the MTIME module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to MTIME, allowing the module to operate. This macro should be called before
 * initializing or using MTIME to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_MTIME_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the MTIME clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the MTIME module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_MTIME_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_MTIME_TOGGLE = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for MTIME.
 *
 * This macro disables the clock for the MTIME module.
 * Disabling the clock can be useful in power-saving modes or when the MTIME module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to MTIME.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that MTIME
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          MTIME operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_MTIME_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_MTIME_TOGGLE = 0x0; \
} while(0)
/**
 * @brief Checks if the MTIME clock is enabled.
 *
 * This inline static function determines whether the clock for the MTIME module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of MTIME before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the MTIME clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_MtimeClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.ENA_MTIME_TOGGLE;
}/**
 * @brief Sets the clock divider for Mtime.
 *
 * This function configures the clock division for Mtime by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Mtime clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 63].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Mtime specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetMtimeClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for MTIME.
 *
 * This inline static function obtains the current clock source and division factors for the
 * MTIME module. The clock source and divider values are returned
 * through the pointer parameters  'div_m'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetMtimeClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetMtimeClkConfig(uint32_t* div_m){
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_MTIME_TOGGLE_M;
}
/**
 * @brief Retrieves the current operating frequency of MTIME.
 *
 * This function returns the frequency (in Hz) at which the MTIME
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the MTIME clock divider settings.
 *
 * @return uint32_t The operating frequency of MTIME in Hertz. If the frequency cannot be
 *                  determined, or if MTIME is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the MTIME divider settings. Changes in these parameters can affect
 *       the MTIME frequency.
 *
 * @warning Ensure that MTIME and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetMtimeFreq();/**
  * @}
  */

/** @defgroup _CRM_SPI0 SPI0_CLK_FUNC
  * @brief SPI0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for SPI0.
 *
 * This macro enables the clock for the SPI0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to SPI0, allowing the module to operate. This macro should be called before
 * initializing or using SPI0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the SPI0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the SPI0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_SPI0_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for SPI0.
 *
 * This macro disables the clock for the SPI0 module.
 * Disabling the clock can be useful in power-saving modes or when the SPI0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to SPI0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that SPI0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          SPI0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_SPI0_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the SPI0 clock is enabled.
 *
 * This inline static function determines whether the clock for the SPI0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of SPI0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the SPI0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Spi0ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK;
}/**
 * @brief Sets the clock source for Spi0.
 *
 * This function configures Spi0 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Spi0 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Spi0. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Spi0
 *       and the system's clock configuration. It is advisable to ensure that Spi0 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Spi0 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Spi0's operational requirements.
 */
uint32_t HAL_CRM_SetSpi0ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Spi0.
 *
 * This function configures the clock division for Spi0 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Spi0 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Spi0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSpi0ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for SPI0.
 *
 * This inline static function obtains the current clock source and division factors for the
 * SPI0 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetSpi0ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetSpi0ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.SEL_SPI0_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_n = IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_N;
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of SPI0.
 *
 * This function returns the frequency (in Hz) at which the SPI0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the SPI0 clock divider settings.
 *
 * @return uint32_t The operating frequency of SPI0 in Hertz. If the frequency cannot be
 *                  determined, or if SPI0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the SPI0 divider settings. Changes in these parameters can affect
 *       the SPI0 frequency.
 *
 * @warning Ensure that SPI0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetSpi0Freq();/**
  * @}
  */

/** @defgroup _CRM_UART0 UART0_CLK_FUNC
  * @brief UART0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for UART0.
 *
 * This macro enables the clock for the UART0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to UART0, allowing the module to operate. This macro should be called before
 * initializing or using UART0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_UART0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the UART0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the UART0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_UART0_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for UART0.
 *
 * This macro disables the clock for the UART0 module.
 * Disabling the clock can be useful in power-saving modes or when the UART0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to UART0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that UART0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          UART0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_UART0_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the UART0 clock is enabled.
 *
 * This inline static function determines whether the clock for the UART0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of UART0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the UART0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Uart0ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK;
}/**
 * @brief Sets the clock source for Uart0.
 *
 * This function configures Uart0 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Uart0 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Uart0. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Uart0
 *       and the system's clock configuration. It is advisable to ensure that Uart0 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Uart0 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Uart0's operational requirements.
 */
uint32_t HAL_CRM_SetUart0ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Uart0.
 *
 * This function configures the clock division for Uart0 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Uart0 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 511].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Uart0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetUart0ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for UART0.
 *
 * This inline static function obtains the current clock source and division factors for the
 * UART0 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetUart0ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetUart0ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.SEL_UART0_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_n = IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_N;
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of UART0.
 *
 * This function returns the frequency (in Hz) at which the UART0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the UART0 clock divider settings.
 *
 * @return uint32_t The operating frequency of UART0 in Hertz. If the frequency cannot be
 *                  determined, or if UART0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the UART0 divider settings. Changes in these parameters can affect
 *       the UART0 frequency.
 *
 * @warning Ensure that UART0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetUart0Freq();/**
  * @}
  */

/** @defgroup _CRM_SPI1 SPI1_CLK_FUNC
  * @brief SPI1 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for SPI1.
 *
 * This macro enables the clock for the SPI1 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to SPI1, allowing the module to operate. This macro should be called before
 * initializing or using SPI1 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_SPI1_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the SPI1 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the SPI1 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_SPI1_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for SPI1.
 *
 * This macro disables the clock for the SPI1 module.
 * Disabling the clock can be useful in power-saving modes or when the SPI1 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to SPI1.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that SPI1
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          SPI1 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_SPI1_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the SPI1 clock is enabled.
 *
 * This inline static function determines whether the clock for the SPI1 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of SPI1 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the SPI1 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Spi1ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK;
}/**
 * @brief Sets the clock source for Spi1.
 *
 * This function configures Spi1 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Spi1 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Spi1. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Spi1
 *       and the system's clock configuration. It is advisable to ensure that Spi1 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Spi1 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Spi1's operational requirements.
 */
uint32_t HAL_CRM_SetSpi1ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Spi1.
 *
 * This function configures the clock division for Spi1 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Spi1 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Spi1 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSpi1ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for SPI1.
 *
 * This inline static function obtains the current clock source and division factors for the
 * SPI1 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetSpi1ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetSpi1ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.SEL_SPI1_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_n = IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_N;
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of SPI1.
 *
 * This function returns the frequency (in Hz) at which the SPI1
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the SPI1 clock divider settings.
 *
 * @return uint32_t The operating frequency of SPI1 in Hertz. If the frequency cannot be
 *                  determined, or if SPI1 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the SPI1 divider settings. Changes in these parameters can affect
 *       the SPI1 frequency.
 *
 * @warning Ensure that SPI1 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetSpi1Freq();/**
  * @}
  */

/** @defgroup _CRM_UART1 UART1_CLK_FUNC
  * @brief UART1 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for UART1.
 *
 * This macro enables the clock for the UART1 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to UART1, allowing the module to operate. This macro should be called before
 * initializing or using UART1 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_UART1_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the UART1 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the UART1 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_UART1_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.ENA_UART1_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for UART1.
 *
 * This macro disables the clock for the UART1 module.
 * Disabling the clock can be useful in power-saving modes or when the UART1 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to UART1.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that UART1
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          UART1 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_UART1_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.ENA_UART1_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the UART1 clock is enabled.
 *
 * This inline static function determines whether the clock for the UART1 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of UART1 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the UART1 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Uart1ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.ENA_UART1_CLK;
}/**
 * @brief Sets the clock source for Uart1.
 *
 * This function configures Uart1 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Uart1 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Uart1. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Uart1
 *       and the system's clock configuration. It is advisable to ensure that Uart1 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Uart1 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Uart1's operational requirements.
 */
uint32_t HAL_CRM_SetUart1ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Uart1.
 *
 * This function configures the clock division for Uart1 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Uart1 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 511].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Uart1 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetUart1ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for UART1.
 *
 * This inline static function obtains the current clock source and division factors for the
 * UART1 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetUart1ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetUart1ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.SEL_UART1_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_n = IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_N;
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of UART1.
 *
 * This function returns the frequency (in Hz) at which the UART1
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the UART1 clock divider settings.
 *
 * @return uint32_t The operating frequency of UART1 in Hertz. If the frequency cannot be
 *                  determined, or if UART1 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the UART1 divider settings. Changes in these parameters can affect
 *       the UART1 frequency.
 *
 * @warning Ensure that UART1 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetUart1Freq();/**
  * @}
  */

/** @defgroup _CRM_SPI2 SPI2_CLK_FUNC
  * @brief SPI2 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for SPI2.
 *
 * This macro enables the clock for the SPI2 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to SPI2, allowing the module to operate. This macro should be called before
 * initializing or using SPI2 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_SPI2_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the SPI2 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the SPI2 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_SPI2_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_SPI2_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for SPI2.
 *
 * This macro disables the clock for the SPI2 module.
 * Disabling the clock can be useful in power-saving modes or when the SPI2 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to SPI2.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that SPI2
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          SPI2 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_SPI2_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_SPI2_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the SPI2 clock is enabled.
 *
 * This inline static function determines whether the clock for the SPI2 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of SPI2 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the SPI2 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Spi2ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_SPI2_CLK;
}/**
 * @brief Sets the clock source for Spi2.
 *
 * This function configures Spi2 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Spi2 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Spi2. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Spi2
 *       and the system's clock configuration. It is advisable to ensure that Spi2 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Spi2 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Spi2's operational requirements.
 */
uint32_t HAL_CRM_SetSpi2ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Spi2.
 *
 * This function configures the clock division for Spi2 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Spi2 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Spi2 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSpi2ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for SPI2.
 *
 * This inline static function obtains the current clock source and division factors for the
 * SPI2 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetSpi2ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetSpi2ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_SPI2_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_n = IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SPI2_CLK_N;
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SPI2_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of SPI2.
 *
 * This function returns the frequency (in Hz) at which the SPI2
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the SPI2 clock divider settings.
 *
 * @return uint32_t The operating frequency of SPI2 in Hertz. If the frequency cannot be
 *                  determined, or if SPI2 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the SPI2 divider settings. Changes in these parameters can affect
 *       the SPI2 frequency.
 *
 * @warning Ensure that SPI2 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetSpi2Freq();/**
  * @}
  */

/** @defgroup _CRM_UART2 UART2_CLK_FUNC
  * @brief UART2 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for UART2.
 *
 * This macro enables the clock for the UART2 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to UART2, allowing the module to operate. This macro should be called before
 * initializing or using UART2 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_UART2_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the UART2 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the UART2 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_UART2_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_UART2_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for UART2.
 *
 * This macro disables the clock for the UART2 module.
 * Disabling the clock can be useful in power-saving modes or when the UART2 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to UART2.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that UART2
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          UART2 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_UART2_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_UART2_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the UART2 clock is enabled.
 *
 * This inline static function determines whether the clock for the UART2 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of UART2 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the UART2 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Uart2ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_UART2_CLK;
}/**
 * @brief Sets the clock source for Uart2.
 *
 * This function configures Uart2 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Uart2 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Uart2. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Uart2
 *       and the system's clock configuration. It is advisable to ensure that Uart2 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Uart2 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Uart2's operational requirements.
 */
uint32_t HAL_CRM_SetUart2ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Uart2.
 *
 * This function configures the clock division for Uart2 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Uart2 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 511].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Uart2 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetUart2ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for UART2.
 *
 * This inline static function obtains the current clock source and division factors for the
 * UART2 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetUart2ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetUart2ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_UART2_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_n = IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_N;
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of UART2.
 *
 * This function returns the frequency (in Hz) at which the UART2
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the UART2 clock divider settings.
 *
 * @return uint32_t The operating frequency of UART2 in Hertz. If the frequency cannot be
 *                  determined, or if UART2 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the UART2 divider settings. Changes in these parameters can affect
 *       the UART2 frequency.
 *
 * @warning Ensure that UART2 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetUart2Freq();/**
  * @}
  */

/** @defgroup _CRM_GPT_T0 GPT_T0_CLK_FUNC
  * @brief GPT_T0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for GPT_T0.
 *
 * This macro enables the clock for the GPT_T0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to GPT_T0, allowing the module to operate. This macro should be called before
 * initializing or using GPT_T0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_GPT_T0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the GPT_T0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the GPT_T0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_GPT_T0_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK_T0 = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for GPT_T0.
 *
 * This macro disables the clock for the GPT_T0 module.
 * Disabling the clock can be useful in power-saving modes or when the GPT_T0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to GPT_T0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that GPT_T0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          GPT_T0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_GPT_T0_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK_T0 = 0x0; \
} while(0)
/**
 * @brief Checks if the GPT_T0 clock is enabled.
 *
 * This inline static function determines whether the clock for the GPT_T0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of GPT_T0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the GPT_T0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Gpt_t0ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK_T0;
}/**
 * @brief Sets the clock divider for Gpt_t0.
 *
 * This function configures the clock division for Gpt_t0 by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Gpt_t0 clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Gpt_t0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetGpt_t0ClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for GPT_T0.
 *
 * This inline static function obtains the current clock source and division factors for the
 * GPT_T0 module. The clock source and divider values are returned
 * through the pointer parameters  'div_m'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetGpt_t0ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetGpt_t0ClkConfig(uint32_t* div_m){
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_T0_M;
}
/**
 * @brief Retrieves the current operating frequency of GPT_T0.
 *
 * This function returns the frequency (in Hz) at which the GPT_T0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the GPT_T0 clock divider settings.
 *
 * @return uint32_t The operating frequency of GPT_T0 in Hertz. If the frequency cannot be
 *                  determined, or if GPT_T0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the GPT_T0 divider settings. Changes in these parameters can affect
 *       the GPT_T0 frequency.
 *
 * @warning Ensure that GPT_T0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetGpt_t0Freq();/**
  * @}
  */

/** @defgroup _CRM_GPT_S GPT_S_CLK_FUNC
  * @brief GPT_S clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for GPT_S.
 *
 * This macro enables the clock for the GPT_S module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to GPT_S, allowing the module to operate. This macro should be called before
 * initializing or using GPT_S to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_GPT_S_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the GPT_S clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the GPT_S module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_GPT_S_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK_S = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for GPT_S.
 *
 * This macro disables the clock for the GPT_S module.
 * Disabling the clock can be useful in power-saving modes or when the GPT_S module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to GPT_S.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that GPT_S
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          GPT_S operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_GPT_S_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK_S = 0x0; \
} while(0)
/**
 * @brief Checks if the GPT_S clock is enabled.
 *
 * This inline static function determines whether the clock for the GPT_S module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of GPT_S before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the GPT_S clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Gpt_sClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK_S;
}/**
 * @brief Sets the clock divider for Gpt_s.
 *
 * This function configures the clock division for Gpt_s by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Gpt_s clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Gpt_s specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetGpt_sClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for GPT_S.
 *
 * This inline static function obtains the current clock source and division factors for the
 * GPT_S module. The clock source and divider values are returned
 * through the pointer parameters  'div_m'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetGpt_sClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetGpt_sClkConfig(uint32_t* div_m){
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_S_M;
}
/**
 * @brief Retrieves the current operating frequency of GPT_S.
 *
 * This function returns the frequency (in Hz) at which the GPT_S
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the GPT_S clock divider settings.
 *
 * @return uint32_t The operating frequency of GPT_S in Hertz. If the frequency cannot be
 *                  determined, or if GPT_S is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the GPT_S divider settings. Changes in these parameters can affect
 *       the GPT_S frequency.
 *
 * @warning Ensure that GPT_S and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetGpt_sFreq();/**
  * @}
  */

/** @defgroup _CRM_GPADC GPADC_CLK_FUNC
  * @brief GPADC clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for GPADC.
 *
 * This macro enables the clock for the GPADC module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to GPADC, allowing the module to operate. This macro should be called before
 * initializing or using GPADC to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_GPADC_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the GPADC clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the GPADC module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_GPADC_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for GPADC.
 *
 * This macro disables the clock for the GPADC module.
 * Disabling the clock can be useful in power-saving modes or when the GPADC module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to GPADC.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that GPADC
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          GPADC operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_GPADC_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the GPADC clock is enabled.
 *
 * This inline static function determines whether the clock for the GPADC module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of GPADC before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the GPADC clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_GpadcClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK;
}/**
 * @brief Sets the clock divider for Gpadc.
 *
 * This function configures the clock division for Gpadc by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Gpadc clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Gpadc specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetGpadcClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for GPADC.
 *
 * This inline static function obtains the current clock source and division factors for the
 * GPADC module. The clock source and divider values are returned
 * through the pointer parameters  'div_m'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetGpadcClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetGpadcClkConfig(uint32_t* div_m){
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of GPADC.
 *
 * This function returns the frequency (in Hz) at which the GPADC
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the GPADC clock divider settings.
 *
 * @return uint32_t The operating frequency of GPADC in Hertz. If the frequency cannot be
 *                  determined, or if GPADC is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the GPADC divider settings. Changes in these parameters can affect
 *       the GPADC frequency.
 *
 * @warning Ensure that GPADC and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetGpadcFreq();/**
  * @}
  */

/** @defgroup _CRM_IR_TX IR_TX_CLK_FUNC
  * @brief IR_TX clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Sets the clock divider for Ir_tx.
 *
 * This function configures the clock division for Ir_tx by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Ir_tx clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 63].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Ir_tx specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetIr_txClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for IR_TX.
 *
 * This inline static function obtains the current clock source and division factors for the
 * IR_TX module. The clock source and divider values are returned
 * through the pointer parameters  'div_m'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetIr_txClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetIr_txClkConfig(uint32_t* div_m){
    *div_m = IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_M;
}
/**
 * @brief Retrieves the current operating frequency of IR_TX.
 *
 * This function returns the frequency (in Hz) at which the IR_TX
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the IR_TX clock divider settings.
 *
 * @return uint32_t The operating frequency of IR_TX in Hertz. If the frequency cannot be
 *                  determined, or if IR_TX is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the IR_TX divider settings. Changes in these parameters can affect
 *       the IR_TX frequency.
 *
 * @warning Ensure that IR_TX and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetIr_txFreq();/**
  * @}
  */

/** @defgroup _CRM_IR IR_CLK_FUNC
  * @brief IR clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for IR.
 *
 * This macro enables the clock for the IR module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to IR, allowing the module to operate. This macro should be called before
 * initializing or using IR to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_IR_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the IR clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the IR module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_IR_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_IR_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for IR.
 *
 * This macro disables the clock for the IR module.
 * Disabling the clock can be useful in power-saving modes or when the IR module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to IR.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that IR
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          IR operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_IR_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_IR_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the IR clock is enabled.
 *
 * This inline static function determines whether the clock for the IR module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of IR before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the IR clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_IrClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.ENA_IR_CLK;
}/**
 * @brief Retrieves the current operating frequency of IR.
 *
 * This function returns the frequency (in Hz) at which the IR
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the IR clock divider settings.
 *
 * @return uint32_t The operating frequency of IR in Hertz. If the frequency cannot be
 *                  determined, or if IR is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the IR divider settings. Changes in these parameters can affect
 *       the IR frequency.
 *
 * @warning Ensure that IR and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetIrFreq();/**
  * @}
  */

/** @defgroup _CRM_DMA DMA_CLK_FUNC
  * @brief DMA clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for DMA.
 *
 * This macro enables the clock for the DMA module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to DMA, allowing the module to operate. This macro should be called before
 * initializing or using DMA to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_DMA_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the DMA clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the DMA module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_DMA_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_DMAC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for DMA.
 *
 * This macro disables the clock for the DMA module.
 * Disabling the clock can be useful in power-saving modes or when the DMA module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to DMA.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that DMA
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          DMA operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_DMA_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_DMAC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the DMA clock is enabled.
 *
 * This inline static function determines whether the clock for the DMA module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of DMA before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the DMA clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_DmaClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_DMAC_CLK;
}/**
 * @brief Retrieves the current operating frequency of DMA.
 *
 * This function returns the frequency (in Hz) at which the DMA
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the DMA clock divider settings.
 *
 * @return uint32_t The operating frequency of DMA in Hertz. If the frequency cannot be
 *                  determined, or if DMA is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the DMA divider settings. Changes in these parameters can affect
 *       the DMA frequency.
 *
 * @warning Ensure that DMA and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetDmaFreq();/**
  * @}
  */

/** @defgroup _CRM_GPIO0 GPIO0_CLK_FUNC
  * @brief GPIO0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for GPIO0.
 *
 * This macro enables the clock for the GPIO0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to GPIO0, allowing the module to operate. This macro should be called before
 * initializing or using GPIO0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_GPIO0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the GPIO0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the GPIO0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_GPIO0_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_GPIO0_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for GPIO0.
 *
 * This macro disables the clock for the GPIO0 module.
 * Disabling the clock can be useful in power-saving modes or when the GPIO0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to GPIO0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that GPIO0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          GPIO0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_GPIO0_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_GPIO0_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the GPIO0 clock is enabled.
 *
 * This inline static function determines whether the clock for the GPIO0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of GPIO0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the GPIO0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Gpio0ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_GPIO0_CLK;
}/**
 * @brief Retrieves the current operating frequency of GPIO0.
 *
 * This function returns the frequency (in Hz) at which the GPIO0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the GPIO0 clock divider settings.
 *
 * @return uint32_t The operating frequency of GPIO0 in Hertz. If the frequency cannot be
 *                  determined, or if GPIO0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the GPIO0 divider settings. Changes in these parameters can affect
 *       the GPIO0 frequency.
 *
 * @warning Ensure that GPIO0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetGpio0Freq();/**
  * @}
  */

/** @defgroup _CRM_GPIO1 GPIO1_CLK_FUNC
  * @brief GPIO1 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for GPIO1.
 *
 * This macro enables the clock for the GPIO1 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to GPIO1, allowing the module to operate. This macro should be called before
 * initializing or using GPIO1 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_GPIO1_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the GPIO1 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the GPIO1 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_GPIO1_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_GPIO1_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for GPIO1.
 *
 * This macro disables the clock for the GPIO1 module.
 * Disabling the clock can be useful in power-saving modes or when the GPIO1 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to GPIO1.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that GPIO1
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          GPIO1 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_GPIO1_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_GPIO1_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the GPIO1 clock is enabled.
 *
 * This inline static function determines whether the clock for the GPIO1 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of GPIO1 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the GPIO1 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Gpio1ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_GPIO1_CLK;
}/**
 * @brief Retrieves the current operating frequency of GPIO1.
 *
 * This function returns the frequency (in Hz) at which the GPIO1
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the GPIO1 clock divider settings.
 *
 * @return uint32_t The operating frequency of GPIO1 in Hertz. If the frequency cannot be
 *                  determined, or if GPIO1 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the GPIO1 divider settings. Changes in these parameters can affect
 *       the GPIO1 frequency.
 *
 * @warning Ensure that GPIO1 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetGpio1Freq();/**
  * @}
  */

/** @defgroup _CRM_I2C0 I2C0_CLK_FUNC
  * @brief I2C0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for I2C0.
 *
 * This macro enables the clock for the I2C0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to I2C0, allowing the module to operate. This macro should be called before
 * initializing or using I2C0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_I2C0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the I2C0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the I2C0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_I2C0_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_I2C0_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for I2C0.
 *
 * This macro disables the clock for the I2C0 module.
 * Disabling the clock can be useful in power-saving modes or when the I2C0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to I2C0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that I2C0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          I2C0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_I2C0_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_I2C0_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the I2C0 clock is enabled.
 *
 * This inline static function determines whether the clock for the I2C0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of I2C0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the I2C0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_I2c0ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_I2C0_CLK;
}/**
 * @brief Retrieves the current operating frequency of I2C0.
 *
 * This function returns the frequency (in Hz) at which the I2C0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the I2C0 clock divider settings.
 *
 * @return uint32_t The operating frequency of I2C0 in Hertz. If the frequency cannot be
 *                  determined, or if I2C0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the I2C0 divider settings. Changes in these parameters can affect
 *       the I2C0 frequency.
 *
 * @warning Ensure that I2C0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetI2c0Freq();/**
  * @}
  */

/** @defgroup _CRM_I2C1 I2C1_CLK_FUNC
  * @brief I2C1 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for I2C1.
 *
 * This macro enables the clock for the I2C1 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to I2C1, allowing the module to operate. This macro should be called before
 * initializing or using I2C1 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_I2C1_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the I2C1 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the I2C1 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_I2C1_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_I2C1_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for I2C1.
 *
 * This macro disables the clock for the I2C1 module.
 * Disabling the clock can be useful in power-saving modes or when the I2C1 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to I2C1.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that I2C1
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          I2C1 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_I2C1_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_I2C1_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the I2C1 clock is enabled.
 *
 * This inline static function determines whether the clock for the I2C1 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of I2C1 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the I2C1 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_I2c1ClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_I2C1_CLK;
}/**
 * @brief Retrieves the current operating frequency of I2C1.
 *
 * This function returns the frequency (in Hz) at which the I2C1
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the I2C1 clock divider settings.
 *
 * @return uint32_t The operating frequency of I2C1 in Hertz. If the frequency cannot be
 *                  determined, or if I2C1 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the I2C1 divider settings. Changes in these parameters can affect
 *       the I2C1 frequency.
 *
 * @warning Ensure that I2C1 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetI2c1Freq();/**
  * @}
  */

/** @defgroup _CRM_QDEC QDEC_CLK_FUNC
  * @brief QDEC clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for QDEC.
 *
 * This macro enables the clock for the QDEC module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to QDEC, allowing the module to operate. This macro should be called before
 * initializing or using QDEC to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_QDEC_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the QDEC clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the QDEC module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_QDEC_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_QDEC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for QDEC.
 *
 * This macro disables the clock for the QDEC module.
 * Disabling the clock can be useful in power-saving modes or when the QDEC module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to QDEC.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that QDEC
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          QDEC operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_QDEC_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_QDEC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the QDEC clock is enabled.
 *
 * This inline static function determines whether the clock for the QDEC module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of QDEC before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the QDEC clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_QdecClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_QDEC_CLK;
}/**
 * @brief Retrieves the current operating frequency of QDEC.
 *
 * This function returns the frequency (in Hz) at which the QDEC
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the QDEC clock divider settings.
 *
 * @return uint32_t The operating frequency of QDEC in Hertz. If the frequency cannot be
 *                  determined, or if QDEC is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the QDEC divider settings. Changes in these parameters can affect
 *       the QDEC frequency.
 *
 * @warning Ensure that QDEC and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetQdecFreq();/**
  * @}
  */

/** @defgroup _CRM_SMID SMID_CLK_FUNC
  * @brief SMID clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for SMID.
 *
 * This macro enables the clock for the SMID module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to SMID, allowing the module to operate. This macro should be called before
 * initializing or using SMID to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_SMID_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the SMID clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the SMID module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_SMID_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_SMID_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for SMID.
 *
 * This macro disables the clock for the SMID module.
 * Disabling the clock can be useful in power-saving modes or when the SMID module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to SMID.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that SMID
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          SMID operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_SMID_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_SMID_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the SMID clock is enabled.
 *
 * This inline static function determines whether the clock for the SMID module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of SMID before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the SMID clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_SmidClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_SMID_CLK;
}/**
 * @brief Retrieves the current operating frequency of SMID.
 *
 * This function returns the frequency (in Hz) at which the SMID
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the SMID clock divider settings.
 *
 * @return uint32_t The operating frequency of SMID in Hertz. If the frequency cannot be
 *                  determined, or if SMID is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the SMID divider settings. Changes in these parameters can affect
 *       the SMID frequency.
 *
 * @warning Ensure that SMID and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetSmidFreq();/**
  * @}
  */

/** @defgroup _CRM_RFIF RFIF_CLK_FUNC
  * @brief RFIF clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for RFIF.
 *
 * This macro enables the clock for the RFIF module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to RFIF, allowing the module to operate. This macro should be called before
 * initializing or using RFIF to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_RFIF_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the RFIF clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the RFIF module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_RFIF_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_RFIF_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for RFIF.
 *
 * This macro disables the clock for the RFIF module.
 * Disabling the clock can be useful in power-saving modes or when the RFIF module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to RFIF.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that RFIF
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          RFIF operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_RFIF_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_RFIF_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the RFIF clock is enabled.
 *
 * This inline static function determines whether the clock for the RFIF module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of RFIF before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the RFIF clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_RfifClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_RFIF_CLK;
}/**
 * @brief Retrieves the current operating frequency of RFIF.
 *
 * This function returns the frequency (in Hz) at which the RFIF
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the RFIF clock divider settings.
 *
 * @return uint32_t The operating frequency of RFIF in Hertz. If the frequency cannot be
 *                  determined, or if RFIF is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the RFIF divider settings. Changes in these parameters can affect
 *       the RFIF frequency.
 *
 * @warning Ensure that RFIF and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetRfifFreq();/**
  * @}
  */

/** @defgroup _CRM_TRNG TRNG_CLK_FUNC
  * @brief TRNG clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for TRNG.
 *
 * This macro enables the clock for the TRNG module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to TRNG, allowing the module to operate. This macro should be called before
 * initializing or using TRNG to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_TRNG_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the TRNG clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the TRNG module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_TRNG_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_TRNG_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for TRNG.
 *
 * This macro disables the clock for the TRNG module.
 * Disabling the clock can be useful in power-saving modes or when the TRNG module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to TRNG.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that TRNG
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          TRNG operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_TRNG_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_TRNG_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the TRNG clock is enabled.
 *
 * This inline static function determines whether the clock for the TRNG module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of TRNG before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the TRNG clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_TrngClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_TRNG_CLK;
}/**
 * @brief Retrieves the current operating frequency of TRNG.
 *
 * This function returns the frequency (in Hz) at which the TRNG
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the TRNG clock divider settings.
 *
 * @return uint32_t The operating frequency of TRNG in Hertz. If the frequency cannot be
 *                  determined, or if TRNG is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the TRNG divider settings. Changes in these parameters can affect
 *       the TRNG frequency.
 *
 * @warning Ensure that TRNG and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetTrngFreq();/**
  * @}
  */

/** @defgroup _CRM_CALENDAR CALENDAR_CLK_FUNC
  * @brief CALENDAR clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for CALENDAR.
 *
 * This macro enables the clock for the CALENDAR module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to CALENDAR, allowing the module to operate. This macro should be called before
 * initializing or using CALENDAR to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_CALENDAR_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the CALENDAR clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the CALENDAR module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_CALENDAR_CLK_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_CALENDAR_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for CALENDAR.
 *
 * This macro disables the clock for the CALENDAR module.
 * Disabling the clock can be useful in power-saving modes or when the CALENDAR module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to CALENDAR.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that CALENDAR
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          CALENDAR operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_CALENDAR_CLK_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_CALENDAR_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the CALENDAR clock is enabled.
 *
 * This inline static function determines whether the clock for the CALENDAR module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of CALENDAR before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the CALENDAR clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_CalendarClkIsEnabled(){
    return IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_CALENDAR_CLK;
}/**
 * @brief Retrieves the current operating frequency of CALENDAR.
 *
 * This function returns the frequency (in Hz) at which the CALENDAR
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the CALENDAR clock divider settings.
 *
 * @return uint32_t The operating frequency of CALENDAR in Hertz. If the frequency cannot be
 *                  determined, or if CALENDAR is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the CALENDAR divider settings. Changes in these parameters can affect
 *       the CALENDAR frequency.
 *
 * @warning Ensure that CALENDAR and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetCalendarFreq();/**
  * @}
  */

/** @defgroup _CRM_USB USB_CLK_FUNC
  * @brief USB clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for USB.
 *
 * This macro enables the clock for the USB module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to USB, allowing the module to operate. This macro should be called before
 * initializing or using USB to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_USB_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the USB clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the USB module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
//#define __HAL_CRM_USB_CLK_ENABLE()    \
//do { \
//    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1; \
//} while(0)
/**
 * @brief Macro to disable the clock for USB.
 *
 * This macro disables the clock for the USB module.
 * Disabling the clock can be useful in power-saving modes or when the USB module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to USB.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that USB
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          USB operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
//#define __HAL_CRM_USB_CLK_DISABLE()    \
//do { \
//	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x0; \
//} while(0)
/**
 * @brief Checks if the USB clock is enabled.
 *
 * This inline static function determines whether the clock for the USB module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of USB before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the USB clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_UsbClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK;
}/**
 * @brief Retrieves the current operating frequency of USB.
 *
 * This function returns the frequency (in Hz) at which the USB
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the USB clock divider settings.
 *
 * @return uint32_t The operating frequency of USB in Hertz. If the frequency cannot be
 *                  determined, or if USB is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the USB divider settings. Changes in these parameters can affect
 *       the USB frequency.
 *
 * @warning Ensure that USB and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetUsbFreq();/**
  * @}
  */

/** @defgroup _CRM_BT BT_CLK_FUNC
  * @brief BT clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for BT.
 *
 * This macro enables the clock for the BT module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to BT, allowing the module to operate. This macro should be called before
 * initializing or using BT to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_BT_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the BT clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the BT module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_BT_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_BT_HCLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for BT.
 *
 * This macro disables the clock for the BT module.
 * Disabling the clock can be useful in power-saving modes or when the BT module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to BT.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that BT
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          BT operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_BT_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_BT_HCLK = 0x0; \
} while(0)
/**
 * @brief Checks if the BT clock is enabled.
 *
 * This inline static function determines whether the clock for the BT module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of BT before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the BT clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_BtClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_BT_HCLK;
}/**
 * @brief Retrieves the current operating frequency of BT.
 *
 * This function returns the frequency (in Hz) at which the BT
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the BT clock divider settings.
 *
 * @return uint32_t The operating frequency of BT in Hertz. If the frequency cannot be
 *                  determined, or if BT is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the BT divider settings. Changes in these parameters can affect
 *       the BT frequency.
 *
 * @warning Ensure that BT and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetBtFreq();/**
  * @}
  */

/** @defgroup _CRM_WIFI WIFI_CLK_FUNC
  * @brief WIFI clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for WIFI.
 *
 * This macro enables the clock for the WIFI module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to WIFI, allowing the module to operate. This macro should be called before
 * initializing or using WIFI to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_WIFI_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the WIFI clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the WIFI module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_WIFI_CLK_ENABLE()    \
do { \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_WF_HCLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for WIFI.
 *
 * This macro disables the clock for the WIFI module.
 * Disabling the clock can be useful in power-saving modes or when the WIFI module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to WIFI.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that WIFI
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          WIFI operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_WIFI_CLK_DISABLE()    \
do { \
	IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_WF_HCLK = 0x0; \
} while(0)
/**
 * @brief Checks if the WIFI clock is enabled.
 *
 * This inline static function determines whether the clock for the WIFI module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of WIFI before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the WIFI clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_WifiClkIsEnabled(){
    return IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_WF_HCLK;
}/**
 * @brief Retrieves the current operating frequency of WIFI.
 *
 * This function returns the frequency (in Hz) at which the WIFI
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the WIFI clock divider settings.
 *
 * @return uint32_t The operating frequency of WIFI in Hertz. If the frequency cannot be
 *                  determined, or if WIFI is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the WIFI divider settings. Changes in these parameters can affect
 *       the WIFI frequency.
 *
 * @warning Ensure that WIFI and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetWifiFreq();/**
  * @}
  */

/** @defgroup _CRM_CRYPTO CRYPTO_CLK_FUNC
  * @brief CRYPTO clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for CRYPTO.
 *
 * This macro enables the clock for the CRYPTO module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to CRYPTO, allowing the module to operate. This macro should be called before
 * initializing or using CRYPTO to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_CRYPTO_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the CRYPTO clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the CRYPTO module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_CRYPTO_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CRYPTO_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for CRYPTO.
 *
 * This macro disables the clock for the CRYPTO module.
 * Disabling the clock can be useful in power-saving modes or when the CRYPTO module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to CRYPTO.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that CRYPTO
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          CRYPTO operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_CRYPTO_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CRYPTO_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the CRYPTO clock is enabled.
 *
 * This inline static function determines whether the clock for the CRYPTO module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of CRYPTO before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the CRYPTO clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_CryptoClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CRYPTO_CLK;
}/**
 * @brief Retrieves the current operating frequency of CRYPTO.
 *
 * This function returns the frequency (in Hz) at which the CRYPTO
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the CRYPTO clock divider settings.
 *
 * @return uint32_t The operating frequency of CRYPTO in Hertz. If the frequency cannot be
 *                  determined, or if CRYPTO is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the CRYPTO divider settings. Changes in these parameters can affect
 *       the CRYPTO frequency.
 *
 * @warning Ensure that CRYPTO and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetCryptoFreq();/**
  * @}
  */

/** @defgroup _CRM_JPEG JPEG_CLK_FUNC
  * @brief JPEG clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for JPEG.
 *
 * This macro enables the clock for the JPEG module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to JPEG, allowing the module to operate. This macro should be called before
 * initializing or using JPEG to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_JPEG_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the JPEG clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the JPEG module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_JPEG_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_JPEG_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for JPEG.
 *
 * This macro disables the clock for the JPEG module.
 * Disabling the clock can be useful in power-saving modes or when the JPEG module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to JPEG.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that JPEG
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          JPEG operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_JPEG_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG1.bit.ENA_JPEG_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the JPEG clock is enabled.
 *
 * This inline static function determines whether the clock for the JPEG module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of JPEG before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the JPEG clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_JpegClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG1.bit.ENA_JPEG_CLK;
}/**
 * @brief Retrieves the current operating frequency of JPEG.
 *
 * This function returns the frequency (in Hz) at which the JPEG
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the JPEG clock divider settings.
 *
 * @return uint32_t The operating frequency of JPEG in Hertz. If the frequency cannot be
 *                  determined, or if JPEG is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the JPEG divider settings. Changes in these parameters can affect
 *       the JPEG frequency.
 *
 * @warning Ensure that JPEG and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetJpegFreq();/**
  * @}
  */

/** @defgroup _CRM_GPDMA GPDMA_CLK_FUNC
  * @brief GPDMA clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for GPDMA.
 *
 * This macro enables the clock for the GPDMA module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to GPDMA, allowing the module to operate. This macro should be called before
 * initializing or using GPDMA to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_GPDMA_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the GPDMA clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the GPDMA module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_GPDMA_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for GPDMA.
 *
 * This macro disables the clock for the GPDMA module.
 * Disabling the clock can be useful in power-saving modes or when the GPDMA module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to GPDMA.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that GPDMA
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          GPDMA operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_GPDMA_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the GPDMA clock is enabled.
 *
 * This inline static function determines whether the clock for the GPDMA module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of GPDMA before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the GPDMA clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_GpdmaClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK;
}/**
 * @brief Retrieves the current operating frequency of GPDMA.
 *
 * This function returns the frequency (in Hz) at which the GPDMA
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the GPDMA clock divider settings.
 *
 * @return uint32_t The operating frequency of GPDMA in Hertz. If the frequency cannot be
 *                  determined, or if GPDMA is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the GPDMA divider settings. Changes in these parameters can affect
 *       the GPDMA frequency.
 *
 * @warning Ensure that GPDMA and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetGpdmaFreq();/**
  * @}
  */

/** @defgroup _CRM_RGB RGB_CLK_FUNC
  * @brief RGB clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for RGB.
 *
 * This macro enables the clock for the RGB module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to RGB, allowing the module to operate. This macro should be called before
 * initializing or using RGB to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_RGB_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the RGB clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the RGB module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_RGB_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_RGB_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for RGB.
 *
 * This macro disables the clock for the RGB module.
 * Disabling the clock can be useful in power-saving modes or when the RGB module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to RGB.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that RGB
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          RGB operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_RGB_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_RGB_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the RGB clock is enabled.
 *
 * This inline static function determines whether the clock for the RGB module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of RGB before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the RGB clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_RgbClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_RGB_CLK;
}/**
 * @brief Sets the clock source for Rgb.
 *
 * This function configures Rgb to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Rgb clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Rgb. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Rgb
 *       and the system's clock configuration. It is advisable to ensure that Rgb is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Rgb can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Rgb's operational requirements.
 */
uint32_t HAL_CRM_SetRgbClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Rgb.
 *
 * This function configures the clock division for Rgb by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Rgb clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 7].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Rgb specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetRgbClkDiv(uint32_t div_m);/**
 * @brief Retrieves the clock configuration for RGB.
 *
 * This inline static function obtains the current clock source and division factors for the
 * RGB module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetRgbClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetRgbClkConfig(clock_src_name_t* src, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcPeriClk;
    }
    *div_m = IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of RGB.
 *
 * This function returns the frequency (in Hz) at which the RGB
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the RGB clock divider settings.
 *
 * @return uint32_t The operating frequency of RGB in Hertz. If the frequency cannot be
 *                  determined, or if RGB is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the RGB divider settings. Changes in these parameters can affect
 *       the RGB frequency.
 *
 * @warning Ensure that RGB and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetRgbFreq();/**
  * @}
  */

/** @defgroup _CRM_BLENDER BLENDER_CLK_FUNC
  * @brief BLENDER clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of BLENDER.
 *
 * This function returns the frequency (in Hz) at which the BLENDER
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the BLENDER clock divider settings.
 *
 * @return uint32_t The operating frequency of BLENDER in Hertz. If the frequency cannot be
 *                  determined, or if BLENDER is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the BLENDER divider settings. Changes in these parameters can affect
 *       the BLENDER frequency.
 *
 * @warning Ensure that BLENDER and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetBlenderFreq();/**
  * @}
  */

/** @defgroup _CRM_SDIO_D SDIO_D_CLK_FUNC
  * @brief SDIO_D clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of SDIO_D.
 *
 * This function returns the frequency (in Hz) at which the SDIO_D
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the SDIO_D clock divider settings.
 *
 * @return uint32_t The operating frequency of SDIO_D in Hertz. If the frequency cannot be
 *                  determined, or if SDIO_D is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the SDIO_D divider settings. Changes in these parameters can affect
 *       the SDIO_D frequency.
 *
 * @warning Ensure that SDIO_D and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetSdio_dFreq();/**
  * @}
  */

/** @defgroup _CRM_SDIO_H SDIO_H_CLK_FUNC
  * @brief SDIO_H clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for SDIO_H.
 *
 * This macro enables the clock for the SDIO_H module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to SDIO_H, allowing the module to operate. This macro should be called before
 * initializing or using SDIO_H to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_SDIO_H_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the SDIO_H clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the SDIO_H module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_SDIO_H_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for SDIO_H.
 *
 * This macro disables the clock for the SDIO_H module.
 * Disabling the clock can be useful in power-saving modes or when the SDIO_H module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to SDIO_H.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that SDIO_H
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          SDIO_H operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_SDIO_H_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the SDIO_H clock is enabled.
 *
 * This inline static function determines whether the clock for the SDIO_H module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of SDIO_H before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the SDIO_H clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Sdio_hClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK;
}/**
 * @brief Sets the clock source for Sdio_h.
 *
 * This function configures Sdio_h to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Sdio_h clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Sdio_h. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Sdio_h
 *       and the system's clock configuration. It is advisable to ensure that Sdio_h is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Sdio_h can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Sdio_h's operational requirements.
 */
uint32_t HAL_CRM_SetSdio_hClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Sdio_h.
 *
 * This function configures the clock division for Sdio_h by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Sdio_h clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Sdio_h specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSdio_hClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for SDIO_H.
 *
 * This inline static function obtains the current clock source and division factors for the
 * SDIO_H module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetSdio_hClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetSdio_hClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_AP_CFG->REG_CLK_CFG1.bit.SEL_SDIOH_CLK2X;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcFlashClk;
    }
    *div_n = IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_N;
    *div_m = IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_M;
}
/**
 * @brief Retrieves the current operating frequency of SDIO_H.
 *
 * This function returns the frequency (in Hz) at which the SDIO_H
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the SDIO_H clock divider settings.
 *
 * @return uint32_t The operating frequency of SDIO_H in Hertz. If the frequency cannot be
 *                  determined, or if SDIO_H is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the SDIO_H divider settings. Changes in these parameters can affect
 *       the SDIO_H frequency.
 *
 * @warning Ensure that SDIO_H and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetSdio_hFreq();/**
  * @}
  */

/** @defgroup _CRM_WDT WDT_CLK_FUNC
  * @brief WDT clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of WDT.
 *
 * This function returns the frequency (in Hz) at which the WDT
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the WDT clock divider settings.
 *
 * @return uint32_t The operating frequency of WDT in Hertz. If the frequency cannot be
 *                  determined, or if WDT is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the WDT divider settings. Changes in these parameters can affect
 *       the WDT frequency.
 *
 * @warning Ensure that WDT and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetWdtFreq();/**
  * @}
  */

/** @defgroup _CRM_APC APC_CLK_FUNC
  * @brief APC clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for APC.
 *
 * This macro enables the clock for the APC module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to APC, allowing the module to operate. This macro should be called before
 * initializing or using APC to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_APC_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the APC clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the APC module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_APC_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for APC.
 *
 * This macro disables the clock for the APC module.
 * Disabling the clock can be useful in power-saving modes or when the APC module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to APC.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that APC
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          APC operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_APC_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the APC clock is enabled.
 *
 * This inline static function determines whether the clock for the APC module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of APC before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the APC clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_ApcClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK;
}/**
 * @brief Retrieves the current operating frequency of APC.
 *
 * This function returns the frequency (in Hz) at which the APC
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the APC clock divider settings.
 *
 * @return uint32_t The operating frequency of APC in Hertz. If the frequency cannot be
 *                  determined, or if APC is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the APC divider settings. Changes in these parameters can affect
 *       the APC frequency.
 *
 * @warning Ensure that APC and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetApcFreq();/**
  * @}
  */

/** @defgroup _CRM_I2S I2S_CLK_FUNC
  * @brief I2S clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for I2S.
 *
 * This macro enables the clock for the I2S module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to I2S, allowing the module to operate. This macro should be called before
 * initializing or using I2S to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_I2S_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the I2S clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the I2S module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_I2S_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for I2S.
 *
 * This macro disables the clock for the I2S module.
 * Disabling the clock can be useful in power-saving modes or when the I2S module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to I2S.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that I2S
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          I2S operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_I2S_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the I2S clock is enabled.
 *
 * This inline static function determines whether the clock for the I2S module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of I2S before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the I2S clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_I2sClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK;
}/**
 * @brief Retrieves the current operating frequency of I2S.
 *
 * This function returns the frequency (in Hz) at which the I2S
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the I2S clock divider settings.
 *
 * @return uint32_t The operating frequency of I2S in Hertz. If the frequency cannot be
 *                  determined, or if I2S is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the I2S divider settings. Changes in these parameters can affect
 *       the I2S frequency.
 *
 * @warning Ensure that I2S and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetI2sFreq();/**
  * @}
  */

/** @defgroup _CRM_DAC DAC_CLK_FUNC
  * @brief DAC clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for DAC.
 *
 * This macro enables the clock for the DAC module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to DAC, allowing the module to operate. This macro should be called before
 * initializing or using DAC to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_DAC_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the DAC clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the DAC module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_DAC_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_DAC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for DAC.
 *
 * This macro disables the clock for the DAC module.
 * Disabling the clock can be useful in power-saving modes or when the DAC module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to DAC.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that DAC
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          DAC operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_DAC_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_DAC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the DAC clock is enabled.
 *
 * This inline static function determines whether the clock for the DAC module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of DAC before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the DAC clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_DacClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_DAC_CLK;
}/**
 * @brief Retrieves the current operating frequency of DAC.
 *
 * This function returns the frequency (in Hz) at which the DAC
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the DAC clock divider settings.
 *
 * @return uint32_t The operating frequency of DAC in Hertz. If the frequency cannot be
 *                  determined, or if DAC is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the DAC divider settings. Changes in these parameters can affect
 *       the DAC frequency.
 *
 * @warning Ensure that DAC and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetDacFreq();/**
  * @}
  */

/** @defgroup _CRM_ADC ADC_CLK_FUNC
  * @brief ADC clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for ADC.
 *
 * This macro enables the clock for the ADC module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to ADC, allowing the module to operate. This macro should be called before
 * initializing or using ADC to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_ADC_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the ADC clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the ADC module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_ADC_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_ADC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for ADC.
 *
 * This macro disables the clock for the ADC module.
 * Disabling the clock can be useful in power-saving modes or when the ADC module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to ADC.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that ADC
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          ADC operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_ADC_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_ADC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the ADC clock is enabled.
 *
 * This inline static function determines whether the clock for the ADC module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of ADC before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the ADC clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_AdcClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_ADC_CLK;
}/**
 * @brief Retrieves the current operating frequency of ADC.
 *
 * This function returns the frequency (in Hz) at which the ADC
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the ADC clock divider settings.
 *
 * @return uint32_t The operating frequency of ADC in Hertz. If the frequency cannot be
 *                  determined, or if ADC is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the ADC divider settings. Changes in these parameters can affect
 *       the ADC frequency.
 *
 * @warning Ensure that ADC and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetAdcFreq();/**
  * @}
  */

/** @defgroup _CRM_EFUSE EFUSE_CLK_FUNC
  * @brief EFUSE clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for EFUSE.
 *
 * This macro enables the clock for the EFUSE module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to EFUSE, allowing the module to operate. This macro should be called before
 * initializing or using EFUSE to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_EFUSE_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the EFUSE clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the EFUSE module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_EFUSE_CLK_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for EFUSE.
 *
 * This macro disables the clock for the EFUSE module.
 * Disabling the clock can be useful in power-saving modes or when the EFUSE module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to EFUSE.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that EFUSE
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          EFUSE operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_EFUSE_CLK_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the EFUSE clock is enabled.
 *
 * This inline static function determines whether the clock for the EFUSE module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of EFUSE before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the EFUSE clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_EfuseClkIsEnabled(){
    return IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK;
}/**
 * @brief Retrieves the current operating frequency of EFUSE.
 *
 * This function returns the frequency (in Hz) at which the EFUSE
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the EFUSE clock divider settings.
 *
 * @return uint32_t The operating frequency of EFUSE in Hertz. If the frequency cannot be
 *                  determined, or if EFUSE is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the EFUSE divider settings. Changes in these parameters can affect
 *       the EFUSE frequency.
 *
 * @warning Ensure that EFUSE and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetEfuseFreq();/**
  * @}
  */

/** @defgroup _CRM_DMA2D DMA2D_CLK_FUNC
  * @brief DMA2D clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of DMA2D.
 *
 * This function returns the frequency (in Hz) at which the DMA2D
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the DMA2D clock divider settings.
 *
 * @return uint32_t The operating frequency of DMA2D in Hertz. If the frequency cannot be
 *                  determined, or if DMA2D is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the DMA2D divider settings. Changes in these parameters can affect
 *       the DMA2D frequency.
 *
 * @warning Ensure that DMA2D and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetDma2dFreq();/**
  * @}
  */

/** @defgroup _CRM_VIDEO VIDEO_CLK_FUNC
  * @brief VIDEO clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for VIDEO.
 *
 * This macro enables the clock for the VIDEO module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to VIDEO, allowing the module to operate. This macro should be called before
 * initializing or using VIDEO to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_VIDEO_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the VIDEO clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the VIDEO module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_VIDEO_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for VIDEO.
 *
 * This macro disables the clock for the VIDEO module.
 * Disabling the clock can be useful in power-saving modes or when the VIDEO module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to VIDEO.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that VIDEO
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          VIDEO operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_VIDEO_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the VIDEO clock is enabled.
 *
 * This inline static function determines whether the clock for the VIDEO module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of VIDEO before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the VIDEO clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_VideoClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK;
}/**
 * @brief Retrieves the current operating frequency of VIDEO.
 *
 * This function returns the frequency (in Hz) at which the VIDEO
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the VIDEO clock divider settings.
 *
 * @return uint32_t The operating frequency of VIDEO in Hertz. If the frequency cannot be
 *                  determined, or if VIDEO is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the VIDEO divider settings. Changes in these parameters can affect
 *       the VIDEO frequency.
 *
 * @warning Ensure that VIDEO and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetVideoFreq();/**
  * @}
  */

/** @defgroup _CRM_QSPI0 QSPI0_CLK_FUNC
  * @brief QSPI0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for QSPI0.
 *
 * This macro enables the clock for the QSPI0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to QSPI0, allowing the module to operate. This macro should be called before
 * initializing or using QSPI0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_QSPI0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the QSPI0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the QSPI0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_QSPI0_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI0_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for QSPI0.
 *
 * This macro disables the clock for the QSPI0 module.
 * Disabling the clock can be useful in power-saving modes or when the QSPI0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to QSPI0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that QSPI0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          QSPI0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_QSPI0_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI0_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the QSPI0 clock is enabled.
 *
 * This inline static function determines whether the clock for the QSPI0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of QSPI0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the QSPI0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Qspi0ClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI0_CLK;
}/**
 * @brief Sets the clock source for Qspi0.
 *
 * This function configures Qspi0 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Qspi0 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Qspi0. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Qspi0
 *       and the system's clock configuration. It is advisable to ensure that Qspi0 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Qspi0 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Qspi0's operational requirements.
 */
uint32_t HAL_CRM_SetQspi0ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Qspi0.
 *
 * This function configures the clock division for Qspi0 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Qspi0 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Qspi0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetQspi0ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for QSPI0.
 *
 * This inline static function obtains the current clock source and division factors for the
 * QSPI0 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetQspi0ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetQspi0ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI0_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcFlashClk;
    }
    *div_n = IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_N;
    *div_m = IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of QSPI0.
 *
 * This function returns the frequency (in Hz) at which the QSPI0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the QSPI0 clock divider settings.
 *
 * @return uint32_t The operating frequency of QSPI0 in Hertz. If the frequency cannot be
 *                  determined, or if QSPI0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the QSPI0 divider settings. Changes in these parameters can affect
 *       the QSPI0 frequency.
 *
 * @warning Ensure that QSPI0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetQspi0Freq();/**
  * @}
  */

/** @defgroup _CRM_QSPI1 QSPI1_CLK_FUNC
  * @brief QSPI1 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for QSPI1.
 *
 * This macro enables the clock for the QSPI1 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to QSPI1, allowing the module to operate. This macro should be called before
 * initializing or using QSPI1 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_QSPI1_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the QSPI1 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the QSPI1 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_QSPI1_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI1_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for QSPI1.
 *
 * This macro disables the clock for the QSPI1 module.
 * Disabling the clock can be useful in power-saving modes or when the QSPI1 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to QSPI1.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that QSPI1
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          QSPI1 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_QSPI1_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI1_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the QSPI1 clock is enabled.
 *
 * This inline static function determines whether the clock for the QSPI1 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of QSPI1 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the QSPI1 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Qspi1ClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI1_CLK;
}/**
 * @brief Sets the clock source for Qspi1.
 *
 * This function configures Qspi1 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Qspi1 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Qspi1. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Qspi1
 *       and the system's clock configuration. It is advisable to ensure that Qspi1 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Qspi1 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Qspi1's operational requirements.
 */
uint32_t HAL_CRM_SetQspi1ClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for Qspi1.
 *
 * This function configures the clock division for Qspi1 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Qspi1 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Qspi1 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetQspi1ClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for QSPI1.
 *
 * This inline static function obtains the current clock source and division factors for the
 * QSPI1 module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetQspi1ClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetQspi1ClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI1_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcFlashClk;
    }
    *div_n = IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_N;
    *div_m = IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_M;
}
/**
 * @brief Retrieves the current operating frequency of QSPI1.
 *
 * This function returns the frequency (in Hz) at which the QSPI1
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the QSPI1 clock divider settings.
 *
 * @return uint32_t The operating frequency of QSPI1 in Hertz. If the frequency cannot be
 *                  determined, or if QSPI1 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the QSPI1 divider settings. Changes in these parameters can affect
 *       the QSPI1 frequency.
 *
 * @warning Ensure that QSPI1 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetQspi1Freq();/**
  * @}
  */

/** @defgroup _CRM_DVP DVP_CLK_FUNC
  * @brief DVP clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for DVP.
 *
 * This macro enables the clock for the DVP module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to DVP, allowing the module to operate. This macro should be called before
 * initializing or using DVP to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_DVP_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the DVP clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the DVP module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_DVP_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for DVP.
 *
 * This macro disables the clock for the DVP module.
 * Disabling the clock can be useful in power-saving modes or when the DVP module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to DVP.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that DVP
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          DVP operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_DVP_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the DVP clock is enabled.
 *
 * This inline static function determines whether the clock for the DVP module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of DVP before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the DVP clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_DvpClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK;
}/**
 * @brief Retrieves the current operating frequency of DVP.
 *
 * This function returns the frequency (in Hz) at which the DVP
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the DVP clock divider settings.
 *
 * @return uint32_t The operating frequency of DVP in Hertz. If the frequency cannot be
 *                  determined, or if DVP is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the DVP divider settings. Changes in these parameters can affect
 *       the DVP frequency.
 *
 * @warning Ensure that DVP and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetDvpFreq();/**
  * @}
  */

/** @defgroup _CRM_KEYSENSE0 KEYSENSE0_CLK_FUNC
  * @brief KEYSENSE0 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for KEYSENSE0.
 *
 * This macro enables the clock for the KEYSENSE0 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to KEYSENSE0, allowing the module to operate. This macro should be called before
 * initializing or using KEYSENSE0 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_KEYSENSE0_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the KEYSENSE0 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the KEYSENSE0 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_KEYSENSE0_CLK_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_KEYSENSE0_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for KEYSENSE0.
 *
 * This macro disables the clock for the KEYSENSE0 module.
 * Disabling the clock can be useful in power-saving modes or when the KEYSENSE0 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to KEYSENSE0.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that KEYSENSE0
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          KEYSENSE0 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_KEYSENSE0_CLK_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_KEYSENSE0_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the KEYSENSE0 clock is enabled.
 *
 * This inline static function determines whether the clock for the KEYSENSE0 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of KEYSENSE0 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the KEYSENSE0 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Keysense0ClkIsEnabled(){
    return IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_KEYSENSE0_CLK;
}/**
 * @brief Retrieves the current operating frequency of KEYSENSE0.
 *
 * This function returns the frequency (in Hz) at which the KEYSENSE0
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the KEYSENSE0 clock divider settings.
 *
 * @return uint32_t The operating frequency of KEYSENSE0 in Hertz. If the frequency cannot be
 *                  determined, or if KEYSENSE0 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the KEYSENSE0 divider settings. Changes in these parameters can affect
 *       the KEYSENSE0 frequency.
 *
 * @warning Ensure that KEYSENSE0 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetKeysense0Freq();/**
  * @}
  */

/** @defgroup _CRM_KEYSENSE1 KEYSENSE1_CLK_FUNC
  * @brief KEYSENSE1 clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for KEYSENSE1.
 *
 * This macro enables the clock for the KEYSENSE1 module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to KEYSENSE1, allowing the module to operate. This macro should be called before
 * initializing or using KEYSENSE1 to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_KEYSENSE1_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the KEYSENSE1 clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the KEYSENSE1 module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_KEYSENSE1_CLK_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_KEYSENSE1_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for KEYSENSE1.
 *
 * This macro disables the clock for the KEYSENSE1 module.
 * Disabling the clock can be useful in power-saving modes or when the KEYSENSE1 module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to KEYSENSE1.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that KEYSENSE1
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          KEYSENSE1 operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_KEYSENSE1_CLK_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_KEYSENSE1_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the KEYSENSE1 clock is enabled.
 *
 * This inline static function determines whether the clock for the KEYSENSE1 module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of KEYSENSE1 before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the KEYSENSE1 clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Keysense1ClkIsEnabled(){
    return IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_KEYSENSE1_CLK;
}/**
 * @brief Retrieves the current operating frequency of KEYSENSE1.
 *
 * This function returns the frequency (in Hz) at which the KEYSENSE1
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the KEYSENSE1 clock divider settings.
 *
 * @return uint32_t The operating frequency of KEYSENSE1 in Hertz. If the frequency cannot be
 *                  determined, or if KEYSENSE1 is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the KEYSENSE1 divider settings. Changes in these parameters can affect
 *       the KEYSENSE1 frequency.
 *
 * @warning Ensure that KEYSENSE1 and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetKeysense1Freq();/**
  * @}
  */

/** @defgroup _CRM_DUALTIMER DUALTIMER_CLK_FUNC
  * @brief DUALTIMER clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of DUALTIMER.
 *
 * This function returns the frequency (in Hz) at which the DUALTIMER
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the DUALTIMER clock divider settings.
 *
 * @return uint32_t The operating frequency of DUALTIMER in Hertz. If the frequency cannot be
 *                  determined, or if DUALTIMER is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the DUALTIMER divider settings. Changes in these parameters can affect
 *       the DUALTIMER frequency.
 *
 * @warning Ensure that DUALTIMER and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetDualtimerFreq();/**
  * @}
  */

/** @defgroup _CRM_AON_TIMER AON_TIMER_CLK_FUNC
  * @brief AON_TIMER clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for AON_TIMER.
 *
 * This macro enables the clock for the AON_TIMER module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to AON_TIMER, allowing the module to operate. This macro should be called before
 * initializing or using AON_TIMER to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_AON_TIMER_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the AON_TIMER clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the AON_TIMER module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_AON_TIMER_CLK_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for AON_TIMER.
 *
 * This macro disables the clock for the AON_TIMER module.
 * Disabling the clock can be useful in power-saving modes or when the AON_TIMER module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to AON_TIMER.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that AON_TIMER
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          AON_TIMER operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_AON_TIMER_CLK_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the AON_TIMER clock is enabled.
 *
 * This inline static function determines whether the clock for the AON_TIMER module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of AON_TIMER before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the AON_TIMER clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_Aon_timerClkIsEnabled(){
    return IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK;
}/**
 * @brief Retrieves the current operating frequency of AON_TIMER.
 *
 * This function returns the frequency (in Hz) at which the AON_TIMER
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the AON_TIMER clock divider settings.
 *
 * @return uint32_t The operating frequency of AON_TIMER in Hertz. If the frequency cannot be
 *                  determined, or if AON_TIMER is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the AON_TIMER divider settings. Changes in these parameters can affect
 *       the AON_TIMER frequency.
 *
 * @warning Ensure that AON_TIMER and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetAon_timerFreq();/**
  * @}
  */

/** @defgroup _CRM_AON_WDT AON_WDT_CLK_FUNC
  * @brief AON_WDT clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of AON_WDT.
 *
 * This function returns the frequency (in Hz) at which the AON_WDT
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the AON_WDT clock divider settings.
 *
 * @return uint32_t The operating frequency of AON_WDT in Hertz. If the frequency cannot be
 *                  determined, or if AON_WDT is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the AON_WDT divider settings. Changes in these parameters can affect
 *       the AON_WDT frequency.
 *
 * @warning Ensure that AON_WDT and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetAon_wdtFreq();/**
  * @}
  */

/** @defgroup _CRM_MAILBOX MAILBOX_CLK_FUNC
  * @brief MAILBOX clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of MAILBOX.
 *
 * This function returns the frequency (in Hz) at which the MAILBOX
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the MAILBOX clock divider settings.
 *
 * @return uint32_t The operating frequency of MAILBOX in Hertz. If the frequency cannot be
 *                  determined, or if MAILBOX is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the MAILBOX divider settings. Changes in these parameters can affect
 *       the MAILBOX frequency.
 *
 * @warning Ensure that MAILBOX and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetMailboxFreq();/**
  * @}
  */

/** @defgroup _CRM_MUTEX MUTEX_CLK_FUNC
  * @brief MUTEX clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of MUTEX.
 *
 * This function returns the frequency (in Hz) at which the MUTEX
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the MUTEX clock divider settings.
 *
 * @return uint32_t The operating frequency of MUTEX in Hertz. If the frequency cannot be
 *                  determined, or if MUTEX is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the MUTEX divider settings. Changes in these parameters can affect
 *       the MUTEX frequency.
 *
 * @warning Ensure that MUTEX and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetMutexFreq();/**
  * @}
  */

/** @defgroup _CRM_LUNA LUNA_CLK_FUNC
  * @brief LUNA clock control function which can enable, disable or get status from corresponding
  * device, set ip clock source, set ip clock divider, or get ip clock configuration, help you calculate the ip divider
  * parameter when having ip reference clock and desire clock
  * @{
  */
/**
 * @brief Macro to enable the clock for LUNA.
 *
 * This macro enables the clock for the LUNA module.
 * It typically modifies a specific bit in a hardware register to provide the clock
 * to LUNA, allowing the module to operate. This macro should be called before
 * initializing or using LUNA to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_LUNA_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the LUNA clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the LUNA module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_LUNA_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_LUNA_CLK = 0x1; \
} while(0)
/**
 * @brief Macro to disable the clock for LUNA.
 *
 * This macro disables the clock for the LUNA module.
 * Disabling the clock can be useful in power-saving modes or when the LUNA module is
 * not in use. This macro typically modifies a specific bit in a hardware register to
 * stop the clock supply to LUNA.
 *
 * Usage:
 *      __HAL_CRM_SPI0_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that LUNA
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          LUNA operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_LUNA_CLK_DISABLE()    \
do { \
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_LUNA_CLK = 0x0; \
} while(0)
/**
 * @brief Checks if the LUNA clock is enabled.
 *
 * This inline static function determines whether the clock for the LUNA module is currently
 * enabled. It typically checks a specific bit in a control register and returns the status.
 * This function can be used to verify the clock state of LUNA before performing operations
 * that require the clock to be active.
 *
 * @return uint32_t Returns 1 if the LUNA clock is enabled, and 0 if it is disabled.
 *
 * @note Since this is an inline function, it is expanded at the point of each call, which can
 *       lead to increased code size if used frequently. However, inlining often results in
 *       faster execution, as the overhead of a function call is eliminated.
 *
 * @warning This function should be used with the understanding that the state of the clock could
 *          change immediately after the function call, especially in multi-threaded or
 *          interrupt-driven environments. Additional synchronization mechanisms may be needed
 *          in such cases.
 */
inline static uint32_t HAL_CRM_LunaClkIsEnabled(){
    return IP_AP_CFG->REG_CLK_CFG0.bit.ENA_LUNA_CLK;
}/**
 * @brief Retrieves the current operating frequency of LUNA.
 *
 * This function returns the frequency (in Hz) at which the LUNA
 * is currently operating. The frequency is calculated based on the current configuration of
 * the system's clock sources and the LUNA clock divider settings.
 *
 * @return uint32_t The operating frequency of LUNA in Hertz. If the frequency cannot be
 *                  determined, or if LUNA is not properly configured, the function may return
 *                  0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration and the LUNA divider settings. Changes in these parameters can affect
 *       the LUNA frequency.
 *
 * @warning Ensure that LUNA and its clock sources are properly configured before calling this
 *          function. Calling this function without proper initialization may lead to undefined
 *          behavior or incorrect frequency values.
 */
uint32_t CRM_GetLunaFreq();/**
  * @}
  */

/**********************************CORE************************************/
/** @defgroup _CRM_CMN_PERI_PCLK CMN_PERI_PCLK_CLK_FUNC
  * @brief CMN_PERI_PCLK clock control function which can enable, disable or get status from corresponding
  * source clock, set core clock source, set core clock divider, or get core clock configuration, help you calculate the core divider
  * parameter when having core reference clock and desire clock
  * @{
  */
/**
 * @brief Sets the clock divider for the Cmn_peri_pclk.
 *
 * This function configures the clock division for the Cmn_peri_pclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Cmn_peri_pclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 15].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 31]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Cmn_peri_pclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetCmn_peri_pclkClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for CMN_PERI_PCLK.
 *
 * This inline static function obtains the current clock source and division factors for the
 * CMN_PERI_PCLK module. The clock source and divider values are returned
 * through the pointer parameters  'div_n', and'div_m'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetCMN_PERI_PCLKClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetCmn_peri_pclkClkConfig(uint32_t* div_n, uint32_t* div_m){
    *div_n = IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_N;
    *div_m = IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_M;
}
/**
 * @brief Retrieves the current operating frequency of the Cmn_peri_pclk.
 *
 * This function returns the frequency (in Hz) at which the Cmn_peri_pclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Cmn_peri_pclk in Hertz. If the frequency cannot be
 *                  determined, or if Cmn_peri_pclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Cmn_peri_pclk frequency.
 *
 * @warning This function can call anywhere which get the Cmn_peri_pclk clock frequency.
 */
uint32_t CRM_GetCmn_peri_pclkFreq();/**
  * @}
  */

/** @defgroup _CRM_AON_CFG_PCLK AON_CFG_PCLK_CLK_FUNC
  * @brief AON_CFG_PCLK clock control function which can enable, disable or get status from corresponding
  * source clock, set core clock source, set core clock divider, or get core clock configuration, help you calculate the core divider
  * parameter when having core reference clock and desire clock
  * @{
  */
/**
 * @brief Sets the clock divider for the Aon_cfg_pclk.
 *
 * This function configures the clock division for the Aon_cfg_pclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Aon_cfg_pclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 31].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 63]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Aon_cfg_pclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetAon_cfg_pclkClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for AON_CFG_PCLK.
 *
 * This inline static function obtains the current clock source and division factors for the
 * AON_CFG_PCLK module. The clock source and divider values are returned
 * through the pointer parameters  'div_n', and'div_m'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetAON_CFG_PCLKClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetAon_cfg_pclkClkConfig(uint32_t* div_n, uint32_t* div_m){
    *div_n = IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_N;
    *div_m = IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_M;
}
/**
 * @brief Retrieves the current operating frequency of the Aon_cfg_pclk.
 *
 * This function returns the frequency (in Hz) at which the Aon_cfg_pclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Aon_cfg_pclk in Hertz. If the frequency cannot be
 *                  determined, or if Aon_cfg_pclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Aon_cfg_pclk frequency.
 *
 * @warning This function can call anywhere which get the Aon_cfg_pclk clock frequency.
 */
uint32_t CRM_GetAon_cfg_pclkFreq();/**
  * @}
  */

/** @defgroup _CRM_AP_PERI_PCLK AP_PERI_PCLK_CLK_FUNC
  * @brief AP_PERI_PCLK clock control function which can enable, disable or get status from corresponding
  * source clock, set core clock source, set core clock divider, or get core clock configuration, help you calculate the core divider
  * parameter when having core reference clock and desire clock
  * @{
  */
/**
 * @brief Sets the clock divider for the Ap_peri_pclk.
 *
 * This function configures the clock division for the Ap_peri_pclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Ap_peri_pclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 15].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 31]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Ap_peri_pclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetAp_peri_pclkClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for AP_PERI_PCLK.
 *
 * This inline static function obtains the current clock source and division factors for the
 * AP_PERI_PCLK module. The clock source and divider values are returned
 * through the pointer parameters  'div_n', and'div_m'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetAP_PERI_PCLKClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetAp_peri_pclkClkConfig(uint32_t* div_n, uint32_t* div_m){
    *div_n = IP_AP_CFG->REG_CLK_CFG0.bit.DIV_AP_PERI_PCLK_N;
    *div_m = IP_AP_CFG->REG_CLK_CFG0.bit.DIV_AP_PERI_PCLK_M;
}
/**
 * @brief Retrieves the current operating frequency of the Ap_peri_pclk.
 *
 * This function returns the frequency (in Hz) at which the Ap_peri_pclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Ap_peri_pclk in Hertz. If the frequency cannot be
 *                  determined, or if Ap_peri_pclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Ap_peri_pclk frequency.
 *
 * @warning This function can call anywhere which get the Ap_peri_pclk clock frequency.
 */
uint32_t CRM_GetAp_peri_pclkFreq();/**
  * @}
  */

/** @defgroup _CRM_HCLK HCLK_CLK_FUNC
  * @brief HCLK clock control function which can enable, disable or get status from corresponding
  * source clock, set core clock source, set core clock divider, or get core clock configuration, help you calculate the core divider
  * parameter when having core reference clock and desire clock
  * @{
  */
/**
 * @brief Sets the clock source for the Hclk.
 *
 * This function configures the system's Hclk to use a specific
 * clock source as defined by the 'src' parameter. The 'src' parameter should be one of
 * the values defined in the 'clock_src_name_t' enumeration, representing the various
 * clock sources available in the system. Changing the Hclk source can be crucial for
 * system performance tuning, power management, or adapting to different operational
 * modes.
 *
 * @param src The desired clock source for Hclk. It should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use, can choose this source -> [CRM_IpSrcXtalClk, CRM_IpSrcCoreClk, ]
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error
 *                  code can indicate issues such as an invalid clock source or failure
 *                  in applying the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state
 *       of Hclk and the system's overall clock configuration. It is advisable to ensure
 *       that the system is in a suitable state to change the clock source without
 *       disrupting operational stability.
 *
 * @warning Using an incorrect or unsupported clock source for Hclk can lead to system
 *          instability or malfunction. Ensure that the selected clock source is compatible
 *          with the system's requirements and that any dependent subsystems are
 *          appropriately configured to handle the change in clock source.
 */
uint32_t HAL_CRM_SetHclkClkSrc(clock_src_name_t src);/**
 * @brief Sets the clock divider for the Hclk.
 *
 * This function configures the clock division for the Hclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Hclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 15].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 31]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Hclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetHclkClkDiv(uint32_t div_n, uint32_t div_m);/**
 * @brief Retrieves the clock configuration for HCLK.
 *
 * This inline static function obtains the current clock source and division factors for the
 * HCLK module. The clock source and divider values are returned
 * through the pointer parameters 'src', 'div_n', and'div_m'.
 * * @param[out] src Pointer to a 'clock_src_name_t' variable where the clock source will be stored.
 *                 The clock source is indicated as an enumeration value of type 'clock_src_name_t'.
 * * * @param[out] div_n Pointer to a 'uint32_t' variable where the numerator of the clock division
 *                   ratio will be stored.
 * * * @param[out] div_m Pointer to a 'uint32_t' variable where the denominator of the clock division
 *                   ratio will be stored.
 * * @note Being an inline function, 'HAL_CRM_GetHCLKClkConfig()' is expanded at each point of call,
 *       which can increase code size but typically reduces execution time by avoiding a function
 *       call overhead. Use this function judiciously where performance is critical.
 *
 * @warning Ensure that the pointers passed to this function are valid and point to appropriate
 *          memory locations. Passing invalid pointers may lead to undefined behavior, including
 *          crashes. Also, consider the potential for race conditions if the clock configuration
 *          can be changed by other parts of the program while this function is being executed.
 */
inline static void HAL_CRM_GetHclkClkConfig(clock_src_name_t* src, uint32_t* div_n, uint32_t* div_m){
    uint32_t src_t;
    src_t = IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK;
    if (src_t == 0){
        *src = CRM_IpSrcXtalClk;
    }
    if (src_t == 1){
        *src = CRM_IpSrcCoreClk;
    }
    if (src_t == 2){
        *src = CRM_IpSrcBBPLLCoreClk;
    }
    *div_n = IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIV_HCLK_N;
    *div_m = IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIV_HCLK_M;
}
/**
 * @brief Retrieves the current operating frequency of the Hclk.
 *
 * This function returns the frequency (in Hz) at which the Hclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Hclk in Hertz. If the frequency cannot be
 *                  determined, or if Hclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Hclk frequency.
 *
 * @warning This function can call anywhere which get the Hclk clock frequency.
 */
uint32_t CRM_GetHclkFreq();/**
  * @}
  */

/** @defgroup _CRM_CPU CPU_CLK_FUNC
  * @brief CPU clock control function which can enable, disable or get status from corresponding
  * source clock, set core clock source, set core clock divider, or get core clock configuration, help you calculate the core divider
  * parameter when having core reference clock and desire clock
  * @{
  */
/**
 * @brief Retrieves the current operating frequency of the Cpu.
 *
 * This function returns the frequency (in Hz) at which the Cpu is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Cpu in Hertz. If the frequency cannot be
 *                  determined, or if Cpu is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Cpu frequency.
 *
 * @warning This function can call anywhere which get the Cpu clock frequency.
 */
uint32_t CRM_GetCpuFreq();/**
  * @}
  */

void VCO_Init(uint32_t n, uint32_t frac_n);

int32_t BBPLL_Init();

int32_t CRM_BBPLL_InitCoreSrc(bbpll_clock_src_core_div_t div);

int32_t SYSPLL_Init();

int32_t CRM_InitCoreSrc(clock_src_core_div_t div);
int32_t CRM_InitPsramSrc(clock_src_psram_div_t div);
int32_t CRM_InitPeriSrc(clock_src_peri_div_t div);
int32_t CRM_InitFlashSrc(clock_src_flash_div_t div);
/**
 * @brief Retrieves the frequency of a specified clock source.
 *
 * This function returns the current frequency (in Hz) of a specified clock source in the system.
 * The clock source is determined by the 'src' parameter, which should be one of the values
 * defined in the 'clock_src_name_t' enumeration.
 *
 * @param src The clock source for which the frequency is requested. This parameter should be
 *            a value from the 'clock_src_name_t' enumeration, indicating the specific clock source.
 *
 * @return uint32_t The frequency of the specified clock source in Hertz. If the specified source
 *                  is invalid or the frequency cannot be determined, the function may return 0
 *                  or an error code (if defined).
 *
 * @note The accuracy and availability of the returned frequency may depend on the system's
 *       current state and the specific clock source queried.
 *
 * @warning Ensure that the clock source specified in 'src' is initialized and active before
 *          calling this function. Querying an inactive or uninitialized clock source might
 *          lead to undefined behavior or incorrect frequency values.
 */
uint32_t CRM_GetSrcFreq(clock_src_name_t src);

/**
 * @brief Macro to enable the clocks for USB.
 *
 * This macro enables the clocks(PHYSEL and DIGIT_EN) for the USB module.
 * It typically modifies a specific bit in a hardware register to provide the clocks
 * to USB, allowing the module to operate. This macro should be called before
 * initializing or using USB to ensure that the hardware is properly powered and
 * ready for operation.
 *
 * Usage:
 *      __HAL_CRM_USB_CLK_ENABLE();
 *
 * @note This macro directly interacts with hardware registers, and its effects are
 *       immediate. Ensure that the system is in a state where enabling the PSRAM clock
 *       is safe and appropriate.
 *
 * @warning Incorrect use of this macro, such as enabling the clock without proper
 *          configuration of the USB module, may lead to unexpected behavior or
 *          system instability. Always ensure that the peripheral is configured
 *          correctly before enabling its clock.
 */
#define __HAL_CRM_USB_CLK_ENABLE()    \
do { \
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1; \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1; \
} while(0)

/**
 * @brief Macro to disable the clocks for USB.
 *
 * This macro disables the clocks(PHYSEL and DIGIT_EN) for the USB module.
 * Disabling the clocks will disable the USB module from HW level.
 * This macro typically modifies a specific bit in a hardware register to
 * stop the clocks supply to USB module.
 *
 * Usage:
 *      __HAL_CRM_USB_CLK_DISABLE();
 *
 * @note Disabling the clock to a module while it is in use can lead to incomplete or
 *       corrupted data transfers and should be done with caution. Ensure that USB module
 *       is not actively transmitting or receiving data before calling this macro.
 *
 * @warning Improper use of this macro, such as disabling the clock during an active
 *          USB operation, may result in system instability or data corruption. Always
 *          make sure that the peripheral is idle or powered off before disabling its clock.
 */
#define __HAL_CRM_USB_CLK_DISABLE()    \
do { \
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x0; \
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x0; \
} while(0)

/**
 * @brief Enables or disables the automatic trigger for RC32k calibration.
 */
void HAL_CRM_SetRc32kCaliAutoTrigger(uint8_t enable);

/**
 * @brief Sets the RC32k calibration cycle number.
 * This function configures the length of the RC32k calibration cycle by setting the
 * RCCAL_LENGTH field in the RCCAL register. The length parameter determines how many
 * cycles(2 ^ length) the calibration process will run, with a maximum value of 8(2 ^ 8).
 */
void HAL_CRM_SetRc32kCaliLength(uint8_t length);

/**
 * @brief Starts the RC32k calibration process.
 * This function initiates the RC32k calibration by setting the RCCAL_START bit in the
 * RCCAL register. It also clears any previous calibration done interrupt status and waits
 * for the calibration to complete by polling the RCCAL_DONE_RAWSTAT bit in the RCCAL_IRQ
 * register.
 */
void HAL_CRM_SetRc32kCaliStart();

#endif /* INCLUDE_DRIVER_CLOCKMANAGER_H_ */
