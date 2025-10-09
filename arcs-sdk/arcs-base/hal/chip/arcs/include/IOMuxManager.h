#ifndef __CSK_IOMUX_MANAGER_H
#define __CSK_IOMUX_MANAGER_H

#include <stdint.h>

#include "Driver_Common.h"


#define CSK_IOMUX_PAD_A                 0
// PIN NUM: 0-31

#define CSK_IOMUX_PAD_B                 1
// PIN NUM: 0-15

#define CSK_IOMUX_PAD_A_MAX_PIN			31
#define CSK_IOMUX_PAD_B_MAX_PIN			15

/************** NORMAL IOMUX********************/
#define CSK_IOMUX_FUNC_DEFAULT                 (0U)
#define CSK_IOMUX_FUNC_ALTER1                  (1U)
#define CSK_IOMUX_FUNC_ALTER2                  (2U)
#define CSK_IOMUX_FUNC_ALTER3                  (3U)
#define CSK_IOMUX_FUNC_ALTER4                  (4U)
#define CSK_IOMUX_FUNC_ALTER5                  (5U)
#define CSK_IOMUX_FUNC_ALTER6                  (6U)
#define CSK_IOMUX_FUNC_ALTER7                  (7U)
#define CSK_IOMUX_FUNC_ALTER8                  (8U)
#define CSK_IOMUX_FUNC_ALTER9                  (9U)
#define CSK_IOMUX_FUNC_ALTER10                 (10U)
#define CSK_IOMUX_FUNC_ALTER11                 (11U)
#define CSK_IOMUX_FUNC_ALTER12                 (12U)
#define CSK_IOMUX_FUNC_ALTER13                 (13U)
#define CSK_IOMUX_FUNC_ALTER14                 (14U)
#define CSK_IOMUX_FUNC_ALTER15                 (15U)
#define CSK_IOMUX_FUNC_ALTER16                 (16U)
#define CSK_IOMUX_FUNC_ALTER17                 (17U)
#define CSK_IOMUX_FUNC_ALTER18                 (18U)
#define CSK_IOMUX_FUNC_ALTER19                 (19U)
#define CSK_IOMUX_FUNC_ALTER20                 (20U)
#define CSK_IOMUX_FUNC_ALTER21                 (21U)
#define CSK_IOMUX_FUNC_ALTER22                 (22U)
#define CSK_IOMUX_FUNC_ALTER23                 (23U)
#define CSK_IOMUX_FUNC_ALTER24                 (24U)
#define CSK_IOMUX_FUNC_ALTER25                 (25U)
#define CSK_IOMUX_FUNC_ALTER26                 (26U)
#define CSK_IOMUX_FUNC_ALTER27                 (27U)
#define CSK_IOMUX_FUNC_ALTER28                 (28U)
#define CSK_IOMUX_FUNC_ALTER29                 (29U)
#define CSK_IOMUX_FUNC_ALTER30                 (30U)
#define CSK_IOMUX_FUNC_ALTER31                 (31U)

/****************** AON IOMUX********************/
#define CSK_AON_IOMUX_FUNC_DEFAULT             (0U)
#define CSK_AON_IOMUX_FUNC_ALTER1              (1U)
#define CSK_AON_IOMUX_FUNC_ALTER2              (2U)
#define CSK_AON_IOMUX_FUNC_ALTER3              (3U)
#define CSK_AON_IOMUX_FUNC_ALTER4              (4U)
#define CSK_AON_IOMUX_FUNC_ALTER5              (5U)
#define CSK_AON_IOMUX_FUNC_ALTER6              (6U)

/***************** ANA IOMUX**********************/
#define CSK_ANA_IOMUX_FUNC_DEFAULT             	(0U)
#define CSK_ANA_IOMUX_FUNC_ALTER1              	(1U)
#define CSK_ANA_IOMUX_FUNC_ALTER2              	(2U)
#define CSK_ANA_IOMUX_FUNC_ALTER3              	(3U)
#define CSK_ANA_IOMUX_FUNC_ALTER4              	(4U)
#define CSK_ANA_IOMUX_FUNC_ALTER5              	(5U)
#define CSK_ANA_IOMUX_FUNC_ALTER6              	(6U)
#define CSK_ANA_IOMUX_FUNC_ALTER7              	(7U)
#define CSK_ANA_IOMUX_FUNC_ALTER8             	(8U)
#define CSK_ANA_IOMUX_FUNC_ALTER9              	(9U)
#define CSK_ANA_IOMUX_FUNC_ALTER10              (10U)
#define CSK_ANA_IOMUX_FUNC_ALTER11              (11U)
#define CSK_ANA_IOMUX_FUNC_ALTER12              (12U)
#define CSK_ANA_IOMUX_FUNC_ALTER13              (13U)
#define CSK_ANA_IOMUX_FUNC_ALTER14              (14U)
#define CSK_ANA_IOMUX_FUNC_ALTER15              (15U)


/**************** MODE CONFIGURE********************/
#define HAL_IOMUX_NONE_MODE                    (0U)
#define HAL_IOMUX_PULLUP_MODE                  (1U)
#define HAL_IOMUX_PULLDOWN_MODE                (2U)

/************** AON TO NORMAL********************/
#define CSK_AON_IOMUX_FUNC_NORMAL              (5U)

/************** AON TO ANA********************/
#define CSK_AON_IOMUX_FUNC_ANA                 (3U)

/************** AON IOMUX FUNC********************/
#define CSK_AON_IOMUX_FUNC_DEFAULT             (0U)
#define CSK_AON_IOMUX_FUNC_ALTER1              (1U)
#define CSK_AON_IOMUX_FUNC_ALTER2              (2U)
#define CSK_AON_IOMUX_FUNC_ALTER3              (3U)
#define CSK_AON_IOMUX_FUNC_ALTER4              (4U)
#define CSK_AON_IOMUX_FUNC_ALTER5              (5U)
#define CSK_AON_IOMUX_FUNC_ALTER6              (6U)

//************ IOMUX FORCE DATA *****************/
#define HAL_IOMUX_FORCE_OUT_LOW                (0U)
#define HAL_IOMUX_FORCE_OUT_HIGH               (1U)
#define HAL_IOMUX_FORCE_OFF                    (2U)

/**
 * @brief Configures the IOMUX pin with the specified pad, pin number, and configuration.
 *
 * This function sets up the IOMUX pin by selecting the specified function for the given pad and pin number.
 *
 * @param[in] pad      The pad identifier, can be CSK_IOMUX_PAD_A or CSK_IOMUX_PAD_B.
 * @param[in] pin_num  The pin number within the selected pad.
 * @param[in] pin_cfg  The configuration for the pin function, must not exceed CSK_IOMUX_FUNC_ALTER31.
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if any parameter is out of range.
 */
int32_t IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

/**
 * @brief Configures the IOMUX pin mode with the specified pad, pin number, and mode.
 *
 * This function sets the mode (e.g., pull-up, pull-down) for the specified pad and pin.
 *
 * @param[in] pad      The pad identifier, can be CSK_IOMUX_PAD_A or CSK_IOMUX_PAD_B.
 * @param[in] pin_num  The pin number within the selected pad.
 * @param[in] pin_mode The desired mode for the pin, such as HAL_IOMUX_NONE_MODE, HAL_IOMUX_PULLUP_MODE, or HAL_IOMUX_PULLDOWN_MODE.
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if any parameter is out of range or invalid.
 */
int32_t IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode);

/**
 * @brief Configures the AON IOMUX pin with the specified pad, pin number, and configuration.
 *
 * This function sets up the AON IOMUX pin by selecting the specified function for the given pad and pin number.
 * Note that PAD_A is not supported for AON IOMUX configuration.
 *
 * @param[in] pad      The pad identifier, supports only CSK_IOMUX_PAD_B for this function.
 * @param[in] pin_num  The pin number within the selected pad.
 * @param[in] pin_cfg  The configuration for the pin function, allowing values up to 0x1F.
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if any parameter is out of range or invalid.
 */
int32_t AON_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

/**
 * @brief Configures the AON IOMUX pin mode with the specified pad, pin number, and mode.
 *
 * This function sets the mode (e.g., pull-up, pull-down) for the specified AON IOMUX pad and pin.
 * Note that PAD_A is not supported for AON IOMUX configuration.
 *
 * @param[in] pad      The pad identifier, supports only CSK_IOMUX_PAD_B for this function.
 * @param[in] pin_num  The pin number within the selected pad.
 * @param[in] pin_mode The desired mode for the pin, such as HAL_IOMUX_NONE_MODE, HAL_IOMUX_PULLUP_MODE, or HAL_IOMUX_PULLDOWN_MODE.
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if any parameter is out of range or invalid.
 */
int32_t AON_IOMuxManager_ModeConfigure(uint8_t pad, uint8_t pin_num, uint8_t pin_mode);

/**
 * @brief Configures the ANA IOMUX pin with the specified pad, pin number, and configuration.
 *
 * This function sets up the ANA IOMUX pin by selecting the specified function for the given pad and pin number.
 * It allows configuring the function of the pin with specific settings, primarily for pads A and B.
 *
 * @param[in] pad      The pad identifier, can be CSK_IOMUX_PAD_A or CSK_IOMUX_PAD_B.
 * @param[in] pin_num  The pin number within the selected pad.
 * @param[in] pin_cfg  The configuration for the pin function, where only the 4 bits starting from bit 5 are applied.
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if any parameter is out of range or invalid.
 */
int32_t ANA_IOMuxManager_PinConfigure(uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

/**
 * @brief Forces the IOMUX pin output to a specified state.
 *
 * This function enables forced output on a specified pad and pin, setting it to a high, low, or disabled state.
 * It uses force control and out force settings to manage the pin's forced state.
 *
 * @param[in] pad      The pad identifier, can be CSK_IOMUX_PAD_A or CSK_IOMUX_PAD_B.
 * @param[in] pin_num  The pin number within the selected pad.
 * @param[in] data     The forced output state:
 *                     - HAL_IOMUX_FORCE_OFF to disable forced output.
 *                     - HAL_IOMUX_FORCE_OUT_LOW to force output to low.
 *                     - HAL_IOMUX_FORCE_OUT_HIGH to force output to high.
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if any parameter is out of range or invalid.
 */
int32_t IOMuxManager_PinForce(uint8_t pad, uint8_t pin_num, uint8_t data);

#endif /* __CSK_IOMUX_MANAGER_H */
