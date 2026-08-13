/**
 * @file    IOMuxManager.h
 * @brief   IOMUX Manager Driver Header File
 * @details This file contains definitions for pin multiplexer management including pad types,
 *          function selections, mode configurations, and related API functions.
 * @copyright (c) 2025.06.06 ListenAI. All rights reserved.
 */
#ifndef _IOMUX_MANAGER_H
#define _IOMUX_MANAGER_H

#include <stdint.h>
#include "Driver_Common.h"

/** @defgroup IOMuxManger
  * @brief IOMuxManger HAL module driver
  * @{
  */
/** @defgroup IOMuxManger_Exported_Constants IOMuxManger Exported Constants
  * @{
  */

/** @defgroup iomux_pad_types Pad Type Definitions
 *  Selectable hardware pad modules with different pin capacities
 *  @{
 */
#define CSK_IOMUX_PAD_A                 0      ///< Main pad module (32 pins)
#define CSK_IOMUX_PAD_B                 1      ///< Secondary pad module (6 pins)
#define CSK_IOMUX_PAD_C                 2      ///< Tertiary pad module (4 pins)
#define CSK_IOMUX_PAD_SDIO              3      ///< SDIO pad module (6 pins)
#define CSK_IOMUX_PAD_FLASHIO           4      ///< FLASH pad module (6 pins)

/** Maximum pin numbers per pad module */
#define CSK_IOMUX_PAD_A_MAX_PIN         31     ///< Max pin index for PAD_A
#define CSK_IOMUX_PAD_B_MAX_PIN         5      ///< Max pin index for PAD_B
#define CSK_IOMUX_PAD_C_MAX_PIN         3      ///< Max pin index for PAD_C
#define CSK_IOMUX_PAD_SDIO_MAX_PIN      5      ///< Max pin index for PAD_SDIO
#define CSK_IOMUX_PAD_FLASHIO_MAX_PIN   5      ///< Max pin index for PAD_FLASH
/** @} */ /* End of group iomux_pad_types */

/** @defgroup normal_iomux Normal Mode Function Selections
 *  Standard operating modes for general-purpose IOMUX functionality
 * @{
 */
#define CSK_IOMUX_FUNC_DEFAULT           (0U)   ///< Default peripheral mapping
#define CSK_IOMUX_FUNC_ALTER1            (1U)   ///< Alternate function 1
#define CSK_IOMUX_FUNC_ALTER2            (2U)   ///< Alternate function 2
#define CSK_IOMUX_FUNC_ALTER3            (3U)   ///< Alternate function 3
#define CSK_IOMUX_FUNC_ALTER4            (4U)   ///< Alternate function 4
#define CSK_IOMUX_FUNC_ALTER5            (5U)   ///< Alternate function 5
#define CSK_IOMUX_FUNC_ALTER6            (6U)   ///< Alternate function 6
#define CSK_IOMUX_FUNC_ALTER7            (7U)   ///< Alternate function 7
#define CSK_IOMUX_FUNC_ALTER8            (8U)   ///< Alternate function 8
#define CSK_IOMUX_FUNC_ALTER9            (9U)   ///< Alternate function 9
#define CSK_IOMUX_FUNC_ALTER10          (10U)  ///< Alternate function 10
#define CSK_IOMUX_FUNC_ALTER11          (11U)  ///< Alternate function 11
#define CSK_IOMUX_FUNC_ALTER12          (12U)  ///< Alternate function 12
#define CSK_IOMUX_FUNC_ALTER13          (13U)  ///< Alternate function 13
#define CSK_IOMUX_FUNC_ALTER14          (14U)  ///< Alternate function 14
#define CSK_IOMUX_FUNC_ALTER15          (15U)  ///< Alternate function 15
#define CSK_IOMUX_FUNC_ALTER16          (16U)  ///< Alternate function 16
#define CSK_IOMUX_FUNC_ALTER17          (17U)  ///< Alternate function 17
#define CSK_IOMUX_FUNC_ALTER18          (18U)  ///< Alternate function 18
#define CSK_IOMUX_FUNC_ALTER19          (19U)  ///< Alternate function 19
#define CSK_IOMUX_FUNC_ALTER20          (20U)  ///< Alternate function 20
#define CSK_IOMUX_FUNC_ALTER21          (21U)  ///< Alternate function 21
#define CSK_IOMUX_FUNC_ALTER22          (22U)  ///< Alternate function 22
#define CSK_IOMUX_FUNC_ALTER23          (23U)  ///< Alternate function 23
#define CSK_IOMUX_FUNC_ALTER24          (24U)  ///< Alternate function 24
#define CSK_IOMUX_FUNC_ALTER25          (25U)  ///< Alternate function 25
#define CSK_IOMUX_FUNC_ALTER26          (26U)  ///< Alternate function 26
#define CSK_IOMUX_FUNC_ALTER27          (27U)  ///< Alternate function 27
#define CSK_IOMUX_FUNC_ALTER28          (28U)  ///< Alternate function 28
#define CSK_IOMUX_FUNC_ALTER29          (29U)  ///< Alternate function 29
#define CSK_IOMUX_FUNC_ALTER30          (30U)  ///< Alternate function 30
#define CSK_IOMUX_FUNC_ALTER31          (31U)  ///< Alternate function 31
/** @} */ /* End of group normal_iomux */

/** @defgroup aon_iomux AON Mode Function Selections
 *  Always-On domain specific function mappings
 * @{
 */
#define CSK_AON_IOMUX_FUNC_DEFAULT       (0U)   ///< Default AON peripheral mapping
#define CSK_AON_IOMUX_FUNC_ALTER1        (1U)   ///< AON alternate function 1
#define CSK_AON_IOMUX_FUNC_ALTER2        (2U)   ///< AON alternate function 2
#define CSK_AON_IOMUX_FUNC_ALTER3        (3U)   ///< AON alternate function 3
#define CSK_AON_IOMUX_FUNC_ALTER4        (4U)   ///< AON alternate function 4
#define CSK_AON_IOMUX_FUNC_ALTER5        (5U)   ///< AON alternate function 5
#define CSK_AON_IOMUX_FUNC_ALTER6        (6U)   ///< AON alternate function 6
#define CSK_AON_IOMUX_FUNC_ALTER7        (7U)   ///< AON alternate function 7
#define CSK_AON_IOMUX_FUNC_ALTER8        (8U)   ///< AON alternate function 8
/** @} */ /* End of group aon_iomux */

/** @defgroup ana_iomux ANA Mode Function Selections
 *  Analog domain specific function mappings
 * @{
 */
#define CSK_ANA_IOMUX_FUNC_DEFAULT       (0U)   ///< Default ANA peripheral mapping
#define CSK_ANA_IOMUX_FUNC_ALTER1        (1U)   ///< ANA alternate function 1
#define CSK_ANA_IOMUX_FUNC_ALTER2        (2U)   ///< ANA alternate function 2
#define CSK_ANA_IOMUX_FUNC_ALTER3        (3U)   ///< ANA alternate function 3
#define CSK_ANA_IOMUX_FUNC_ALTER4        (4U)   ///< ANA alternate function 4
#define CSK_ANA_IOMUX_FUNC_ALTER5        (5U)   ///< ANA alternate function 5
#define CSK_ANA_IOMUX_FUNC_ALTER6        (6U)   ///< ANA alternate function 6
#define CSK_ANA_IOMUX_FUNC_ALTER7        (7U)   ///< ANA alternate function 7
#define CSK_ANA_IOMUX_FUNC_ALTER8        (8U)   ///< ANA alternate function 8
#define CSK_ANA_IOMUX_FUNC_ALTER9        (9U)   ///< ANA alternate function 9
#define CSK_ANA_IOMUX_FUNC_ALTER10      (10U)  ///< ANA alternate function 10
#define CSK_ANA_IOMUX_FUNC_ALTER11      (11U)  ///< ANA alternate function 11
/** @} */ /* End of group ana_iomux */

/** @defgroup mode_config Pin Mode Configurations
 *  Electrical characteristics for pin operation
 *  @{
 */
#define HAL_IOMUX_NONE_MODE              (0U)   ///< Floating input mode
#define HAL_IOMUX_PULLUP_MODE            (1U)   ///< Pull-up resistor enabled
#define HAL_IOMUX_PULLDOWN_MODE          (2U)   ///< Pull-down resistor enabled
/** @} */ /* End of group mode_config */

/** @defgroup aon_crossover AON to Other Domain Crossover Functions
 *  Special mappings between AON domain and other domains
 *  @{
 */
#define CSK_AON_IOMUX_FUNC_NORMAL        (6U)   ///< Map to normal domain functions
#define CSK_AON_GPIO_OUT_FUNC           (2U)   ///< Direct GPIO output path
#define CSK_AON_IOMUX_FUNC_ANA          (4U)   ///< Analog signal path
/** @} */ /* End of group aon_crossover */

/** @defgroup force_data Forced Data States
 *  Controlled drive levels when in forced state
 *  @{
 */
#define HAL_IOMUX_FORCE_OUT_LOW         (0U)   ///< Drive low (0V)
#define HAL_IOMUX_FORCE_OUT_HIGH       (1U)   ///< Drive high (VDD)
#define HAL_IOMUX_FORCE_OFF            (2U)   ///< High impedance state
/** @} */ /* End of group force_data */
/** @} */ /* End of group IOMuxManger_Exported_Constants */

/* Exported functions --------------------------------------------------------*/
/** @defgroup IOMuxManager_Exported_Functions IOMuxManager Exported Functions
  * @brief I/O Multiplexer Manager exported functions
  * @details This module provides functions to configure pin multiplexing, electrical modes,
  *          and force control for digital, always-on (AON), and analog domains.
  *
  * The IOMux Manager allows flexible configuration of pin functions across different domains:
  * - Digital domain: Standard GPIO and peripheral functions
  * - AON domain: Always-on functions for low power operation
  * - Analog domain: Analog signal routing and configuration
  *
  * @note Pin numbers are 0-based within each pad module
  * @note Configuration codes are defined in respective header files
  * @{
  */

/**
 * @brief Configure pin multiplexer settings for digital domain
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] pin_cfg  Function selection code (see CSK_IOMUX_FUNC_* defines)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details This function configures the pin multiplexing for digital domain functions.
 *          It selects which peripheral function is routed to the specified pin.
 * @note The pin must not be concurrently used by multiple peripherals
 * @see CSK_IOMUX_FUNC_GPIO, CSK_IOMUX_FUNC_UART, CSK_IOMUX_FUNC_SPI, etc.
 */
int32_t IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

/**
 * @brief Set electrical mode for specified pin in digital domain
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] pin_mode Mode selection (HAL_IOMUX_NONE_MODE/PULLUP_MODE/PULLDOWN_MODE)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details Configures the electrical characteristics of the pin including pull-up/down resistors.
 * @note This setting affects both input and output modes
 * @see HAL_IOMUX_NONE_MODE, HAL_IOMUX_PULLUP_MODE, HAL_IOMUX_PULLDOWN_MODE
 */
int32_t IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode);

/**
 * @brief Force specified logic level on pin in digital domain
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] data     Forced state (HAL_IOMUX_FORCE_OUT_LOW/HIGH/OFF)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details Overrides the normal pin output and forces a specific logic level.
 *          Useful for testing and debug purposes.
 * @note Force mode takes precedence over normal peripheral control
 * @see HAL_IOMUX_FORCE_OUT_LOW, HAL_IOMUX_FORCE_OUT_HIGH, HAL_IOMUX_FORCE_OUT_OFF
 */
int32_t IOMuxManager_PinForce(uint8_t pad, uint8_t pin_num, uint8_t data);

/**
 * @brief AON domain specific pin configuration
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] pin_cfg  AON function selection code (see CSK_AON_IOMUX_FUNC_* defines)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details Configures pin multiplexing for Always-On domain functions.
 *          These functions remain active in low-power modes.
 * @note AON domain configuration is independent of digital domain configuration
 * @see CSK_AON_IOMUX_FUNC_RTC, CSK_AON_IOMUX_FUNC_LPUART, CSK_AON_IOMUX_FUNC_WAKEUP
 */
int32_t AON_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

/**
 * @brief Set electrical mode for AON domain pin
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] pin_mode Mode selection (HAL_IOMUX_NONE_MODE/PULLUP_MODE/PULLDOWN_MODE)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details Configures electrical characteristics for AON domain pins.
 *          These settings persist during low-power operation.
 * @note AON pin modes may have different electrical characteristics than digital domain
 * @see HAL_IOMUX_NONE_MODE, HAL_IOMUX_PULLUP_MODE, HAL_IOMUX_PULLDOWN_MODE
 */
int32_t AON_IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode);

/**
 * @brief Force logic level on AON domain pin
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] data     Forced state (HAL_IOMUX_FORCE_OUT_LOW/HIGH/OFF)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details Forces specific logic level on AON domain pins.
 *          Useful for low-power testing and wakeup signal control.
 * @note AON force control operates independently from digital domain force control
 * @see HAL_IOMUX_FORCE_OUT_LOW, HAL_IOMUX_FORCE_OUT_HIGH, HAL_IOMUX_FORCE_OUT_OFF
 */
int32_t AON_IOMuxManager_PinForce(uint8_t pad, uint8_t pin_num, uint8_t data);

/**
 * @brief Analog domain specific pin configuration
 * @param[in] pad      Selected pad module (CSK_IOMUX_PAD_A/B/C)
 * @param[in] pin_num  Pin number within selected pad (0-based)
 * @param[in] pin_cfg  Analog function selection code (see CSK_ANA_IOMUX_FUNC_* defines)
 * @return CSK_DRIVER_SUCCESS on success, error code otherwise
 * @details Configures pin multiplexing for analog domain functions such as ADC, DAC,
 *          analog comparators, and other analog peripherals.
 * @note Analog configuration may disable digital functionality on the same pin
 * @see CSK_ANA_IOMUX_FUNC_ADC, CSK_ANA_IOMUX_FUNC_DAC, CSK_ANA_IOMUX_FUNC_COMP
 */
int32_t ANA_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

/**
  * @}
  */ /* End of group IOMuxManager_Exported_Functions */

/** @} */ /* End of IOMuxManger group */

#endif /* _IOMUX_MANAGER_H */
