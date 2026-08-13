/**
 * @file    Driver_GPT_PWM.h
 * @author  USER
 * @brief   Header file for General Purpose Timer (GPT) PWM driver interfaces
 * @details This file contains specialized definitions and function declarations
 *          for Pulse Width Modulation (PWM) functionality built on top of GPT peripheral.
 *          It provides PWM-specific types, configuration structures and control APIs.
 * @created Created on: May 12, 2025
 */

#ifndef DRIVER_GPT_PWM_H_
#define DRIVER_GPT_PWM_H_

#include "Driver_Common.h"
#include "Driver_GPT_Common.h"

/** @defgroup GPT_PWM
  * @brief GPT_PWM HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup GPT_PWM_Exported_Constants GPT_PWM Exported Constants
  * @{
  */

/**
  * @}
  */ /* End of group GPT_PWM_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup GPT_PWM_Exported_Types GPT_PWM Exported Types
  * @{
  */
/**
 * @enum GPT_PWM_Port_t
 * @brief PWM output port selection enumeration
 * @var GPT_PWM_PORT_0 Select PWM output port 0
 * @var GPT_PWM_PORT_1 Select PWM output port 1
 * @var GPT_PWM_PORT_2 Select PWM output port 2
 * @var GPT_PWM_PORT_3 Select PWM output port 3
 */
typedef enum {
    GPT_PWM_PORT_0 = 0, ///< PWM output port 0
    GPT_PWM_PORT_1,     ///< PWM output port 1
    GPT_PWM_PORT_2,     ///< PWM output port 2
    GPT_PWM_PORT_3,     ///< PWM output port 3
} GPT_PWM_Port_t;

/**
 * @enum GPT_PWM_PortBitMask_t
 * @brief Bitmask representations for PWM port enable/disable operations
 * @var GPT_PWM_PORT_0_MASK Bitmask for port 0 (bit 0)
 * @var GPT_PWM_PORT_1_MASK Bitmask for port 1 (bit 1)
 * @var GPT_PWM_PORT_2_MASK Bitmask for port 2 (bit 2)
 * @var GPT_PWM_PORT_3_MASK Bitmask for port 3 (bit 3)
 */
typedef enum {
	GPT_PWM_PORT_0_MASK = (1 << 0), ///< Port 0 bitmask
	GPT_PWM_PORT_1_MASK = (1 << 1), ///< Port 1 bitmask
	GPT_PWM_PORT_2_MASK = (1 << 2), ///< Port 2 bitmask
	GPT_PWM_PORT_3_MASK = (1 << 3), ///< Port 3 bitmask
} GPT_PWM_PortBitMask_t;

/**
 * @enum GPT_PWM_InitPolarity_t
 * @brief Initial voltage level when PWM is disabled
 * @var GPT_PWM_INIT_LEVEL_LOW Default to low when disabled
 * @var GPT_PWM_INIT_LEVEL_HIGH Default to high when disabled
 */
typedef enum {
    GPT_PWM_INIT_LEVEL_LOW = 0, ///< Low voltage when disabled
    GPT_PWM_INIT_LEVEL_HIGH    ///< High voltage when disabled
} GPT_PWM_InitPolarity_t;

/**
 * @enum GPT_PWM_OutputPolarity_t
 * @brief Active polarity configuration for PWM output
 * @var GPT_PWM_OUTPUT_ACTIVE_HIGH Active high output (duty cycle controls high time)
 * @var GPT_PWM_OUTPUT_ACTIVE_LOW Active low output (duty cycle controls low time)
 */
typedef enum {
    GPT_PWM_OUTPUT_ACTIVE_HIGH = 0, ///< Active high output
    GPT_PWM_OUTPUT_ACTIVE_LOW       ///< Active low output
} GPT_PWM_OutputPolarity_t;

/**
 * @struct GPT_PWM_Config_t
 * @brief PWM channel configuration parameters
 * @var init_level Initial voltage level when PWM is disabled (@ref GPT_PWM_InitPolarity_t)
 * @var output_polarity Active output polarity (@ref GPT_PWM_OutputPolarity_t)
 */
typedef struct {
    GPT_PWM_InitPolarity_t      init_level;        ///< Initial state when disabled
    GPT_PWM_OutputPolarity_t    output_polarity;   ///< Output active level configuration
} GPT_PWM_Config_t;

/**
 * @struct GPT_PWM_Info_t
 * @brief PWM channel instance information structure
 * @var workspace Pointer to allocated working memory area
 */
typedef struct {
    void* workspace; ///< Allocated working memory block
} GPT_PWM_Info_t;
/**
  * @}
  */ /* End of group GPT_PWM_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup GPT_PWM_Exported_Functions GPT_PWM Exported Functions
  * @{
  */
/**
 * @fn int32_t HAL_GPT_DisablePWM(void *res, GPT_Channel_Num_t channel, GPT_PWM_PortBitMask_t port_mask)
 * @brief Disables specified PWM outputs on a GPT channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] port_mask Bitmask specifying which ports to disable (@ref GPT_PWM_PortBitMask_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Stops PWM generation on specified ports while maintaining channel allocation
 */
int32_t HAL_GPT_DisablePWM(void *res, GPT_Channel_Num_t channel, GPT_PWM_PortBitMask_t port_mask);

/**
 * @fn int32_t HAL_GPT_EnablePWM(void *res, GPT_Channel_Num_t channel, GPT_PWM_PortBitMask_t port_mask)
 * @brief Enables specified PWM outputs on a GPT channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] port_mask Bitmask specifying which ports to enable (@ref GPT_PWM_PortBitMask_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Starts PWM waveform generation on specified ports using current configuration
 */
int32_t HAL_GPT_EnablePWM(void *res, GPT_Channel_Num_t channel, GPT_PWM_PortBitMask_t port_mask);

/**
 * @fn int32_t HAL_GPT_SetPWMDuty(void* res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, uint16_t h_duty)
 * @brief Sets duty cycle percentage for specific PWM port
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] port Target PWM output port (@ref GPT_PWM_Port_t)
 * @param[in] h_duty Duty cycle value (units depend on system scaling)
 * @return Negative error code on failure, non-negative status code on success
 * @details Updates the active portion of PWM cycle for specified port
 */
int32_t HAL_GPT_SetPWMDuty(void* res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, uint16_t h_duty);

/**
 * @fn int32_t HAL_GPT_SetPWMDelayPhase(void* res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, uint16_t dly)
 * @brief Sets phase delay for PWM waveform
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] port Target PWM output port (@ref GPT_PWM_Port_t)
 * @param[in] dly Phase delay value (time units depend on system clock)
 * @return Negative error code on failure, non-negative status code on success
 * @details Controls timing offset between different PWM channels/phases
 */
int32_t HAL_GPT_SetPWMDelayPhase(void* res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, uint16_t dly);

/**
 * @fn int32_t HAL_GPT_PWMControl(void *res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, GPT_PWM_Config_t* para)
 * @brief Applies comprehensive PWM configuration to specific port
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] port Target PWM output port (@ref GPT_PWM_Port_t)
 * @param[in] para Configuration parameters (@ref GPT_PWM_Config_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Updates both initial level and output polarity settings simultaneously
 */
int32_t HAL_GPT_PWMControl(void *res, GPT_Channel_Num_t channel, GPT_PWM_Port_t port, GPT_PWM_Config_t* para);

/**
 * @fn int32_t HAL_GPT_SetPWMFrequency(void* res, GPT_Channel_Num_t channel, uint16_t freq)
 * @brief Sets PWM base frequency for entire channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] freq Desired PWM frequency in Hz
 * @return Negative error code on failure, non-negative status code on success
 * @details Affects all PWM outputs associated with this channel
 */
int32_t HAL_GPT_SetPWMFrequence(void* res, GPT_Channel_Num_t channel, uint16_t freq);

/**
 * @fn int32_t HAL_GPT_RegsterPWMCallback(void* res, GPT_Channel_Num_t channel, CSK_GPT_SignalEvent_t cb_event, void *workspace)
 * @brief Registers event callback for PWM notifications
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] cb_event Event handler function (@ref CSK_GPT_SignalEvent_t)
 * @param[in] workspace User-supplied context pointer passed to callback
 * @return Negative error code on failure, non-negative status code on success
 * @note Callback executes from interrupt context - keep processing minimal
 * @warning Note potential typo in function name (should be Register instead of Regster)
 */
int32_t HAL_GPT_RegsterPWMCallback(void* res, GPT_Channel_Num_t channel, CSK_GPT_SignalEvent_t cb_event, void *workspace);
/** @} */ /* End of group GPT_PWM_Exported_Functions */
/**
  * @}
  */ /* End of group GPT_PWM */

#endif /* DRIVER_GPT_PWM_H_ */
