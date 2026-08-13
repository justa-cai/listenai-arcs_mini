/**
  ******************************************************************************
  * @file    Driver_GPADC.h
  * @author  ListenAI Application Team
  * @brief   Header file of GPADC HAL module.
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
#ifndef DRIVER_GPADC_H
#define DRIVER_GPADC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"

/** @defgroup GPADC
  * @brief GPADC HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup GPADC_Exported_Constants GPADC Exported Constants
  * @{
  */
/** @defgroup GPADC_API_Version GPADC API Version
  * @{
  */
#define CSK_GPADC_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)    /**< API version number */
#define CSK_GPADC_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)     /**< Driver version number */
/** @} */ /* End of group GPADC_API_Version */

/** @defgroup GPADC_Channel_Parameter GPADC Channel Parameter
  * @{
  */
#define CSK_GPADC_CHANNEL_NUM             0x06                            /**< Total number of available channels */
/*----- GPADC Channel select configuration register fields -----*/
#define CSK_GPADC_CHANNEL_SEL_Pos        0                                  /**< Starting bit position for channel selection mask */
#define CSK_GPADC_CHANNEL_SEL_Msk       (0x3FUL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Bitmask covering bits [5:0] */

/**
 * @brief Channel selection flags for individual channels
 * @details Each flag enables scanning of a specific analog input channel
 */
#define CSK_GPADC_CHANNEL_SEL_VBAT      (0x01UL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select VBAT channel */
#define CSK_GPADC_CHANNEL_SEL_TEMP      (0x02UL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select temperature sensor channel */
#define CSK_GPADC_CHANNEL_SEL_KEYSENSE0 (0x04UL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select key sense channel 0 */
#define CSK_GPADC_CHANNEL_SEL_0         (0x08UL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select user channel 0 */
#define CSK_GPADC_CHANNEL_SEL_1         (0x10UL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select user channel 1 */
#define CSK_GPADC_CHANNEL_SEL_2         (0x20UL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select user channel 2 */
#define CSK_GPADC_CHANNEL_SEL_ALL       (0x3FUL << CSK_GPADC_CHANNEL_SEL_Pos) /**< Select all available channels */
/** @} */ /* End of GPADC_Channel_Parameter group */

/** @defgroup GPADC_DMA_EN GPADC data transfer mode configuration
  * @{
  */
#define CSK_GPADC_DMA_ENABLE_Pos        16                                  /**< Starting bit position for DMA enable mask */
#define CSK_GPADC_DMA_ENABLE_Msk       (0x3FUL << CSK_GPADC_DMA_ENABLE_Pos)  /**< Bitmask covering bits [21:16] */
#define CSK_GPADC_DMA_ENABLE(n)        (((n) & 0x3FUL) << CSK_GPADC_DMA_ENABLE_Pos) /**< Macro to set DMA enable bits */
/** @} */ /* End of GPADC_DMA_EN group */

/** @defgroup GPADC_Events GPADC Event Codes
  * @{
  */
/****** GPADC Event Codes *****/
#define CSK_GPADC_COMPLETE              0x04                               /**< Conversion complete event code */
#define CSK_GPADC_EOC_ERROR             0x05                               /**< End of conversion error event code */
/** @} */ /* End of group GPADC_Events */

/**
  * @}
  */ /* End of group GPADC_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup GPADC_Exported_Types GPADC Exported Types
  * @{
  */

/**
 * @brief Type definition for GPADC signal event callback function
 *
 * This callback function type is used to handle GPADC events and provide
 * corresponding workspace data when an event occurs.
 *
 * @param[in] event Event identifier indicating the specific event type
 * @param[in] workspace Pointer to user-defined data associated with the event
 */
typedef void (*CSK_GPADC_SignalEvent_t) (uint32_t event, void* workspace);

/*----- GPADC channel define -----*/
/**
 * @enum GPADC_CHANNEL_TYPE
 * @brief General Purpose Analog-to-Digital Converter (GPADC) channel selection types
 *
 * These enumeration values represent different analog input channels available on the GPADC peripheral.
 */
typedef enum {
    CSK_GPADC_VBAT                      = 0,                /**< Battery voltage measurement channel */
    CSK_GPADC_TEMPSENSOR                = 1,                /**< Temperature sensor measurement channel */
    CSK_GPADC_KEYSENSE0                 = 2,                /**< Key sense line 0 measurement channel */
    CSK_GPADC_CHANNEL0                  = 3,                /**< User-configurable channel 0 */
    CSK_GPADC_CHANNEL1                  = 4,                /**< User-configurable channel 1 */
    CSK_GPADC_CHANNEL2                  = 5,                /**< User-configurable channel 2 */
} GPADC_CHANNEL_TYPE;

/**
 * @enum GPADC_VREF_TYPE
 * @brief General Purpose Analog-to-Digital Converter (GPADC) reference voltage selection types
 *
 * These enumeration values are used to select the reference voltage source for the GPADC peripheral.
 */
typedef enum {
    CSK_GPADC_VBG                       = 0,                /**< Internal reference voltage 1.2V VBG (Not supported) */
    CSK_GPADC_VDD_AUDIO                 = 1,                /**< VDD_AUDIO */
    CSK_GPADC_VDD_IO                    = 2,                /**< VDD_IO */
} GPADC_VREF_TYPE;

/**
 * @enum GPADC_FIFOINT_TYPE
 * @brief First-In First-Out (FIFO) interrupt types for GPADC
 *
 * These enumeration values define different threshold conditions that trigger FIFO interrupts.
 */
typedef enum {
    CSK_GPADC_FIFO_EMPTY                 = 0,                /**< Interrupt when FIFO becomes empty */
    CSK_GPADC_FIFO_FULL                  = 1,                /**< Interrupt when FIFO becomes full */
    CSK_GPADC_FIFO_THD                   = 2,                /**< Interrupt when reaching programmable threshold */
} GPADC_FIFOINT_TYPE;
/**
  * @}
  */ /* End of group GPADC_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup GPADC_Exported_Functions GPADC Exported Functions
  * @{
  */
/**
 * @fn void* GPADC(void)
 * @brief Get pointer to GPADC resource instance
 * @return Pointer to GPADC resource structure
 * @note This function typically returns a handle obtained from initialization
 */
void* GPADC(void);

/**
 * @fn int32_t HAL_GPADC_Initialize(void *res)
 * @brief Initialize the GPADC peripheral
 * @param[in,out] res Pointer to resource handle (initialized by this function)
 * @return Negative error code on failure, non-negative status code on success
 * @details This function performs necessary hardware setup and returns a resource handle
 */
int32_t HAL_GPADC_Initialize(void *res);

/**
 * @fn int32_t HAL_GPADC_Uninitialize(void *res)
 * @brief Deinitialize the GPADC peripheral
 * @param[in,out] res Resource handle obtained from HAL_GPADC_Initialize()
 * @return Negative error code on failure, non-negative status code on success
 * @details Releases resources allocated during initialization
 */
int32_t HAL_GPADC_Uninitialize(void *res);

/**
 * @fn uint32_t HAL_GPADC_Control(void* res, uint32_t control)
 * @brief Control GPADC peripheral behavior
 * @param[in] res Resource handle from successful initialization
 * @param[in] control Control command word
 * @return Status code indicating operation result
 * @note Control commands depend on implementation-specific definitions
 */
uint32_t HAL_GPADC_Control(void* res, uint32_t control);

/**
 * @fn uint32_t HAL_GPADC_Start(void* res)
 * @brief Start GPADC conversion process
 * @param[in] res Resource handle from successful initialization
 * @return Status code indicating operation result
 * @details Begins analog-to-digital conversion sequence
 */
uint32_t HAL_GPADC_Start(void* res);

/**
 * @fn uint32_t HAL_GPADC_Stop(void* res)
 * @brief Stop GPADC conversion process
 * @param[in] res Resource handle from successful initialization
 * @return Status code indicating operation result
 * @details Halts ongoing conversions gracefully
 */
uint32_t HAL_GPADC_Stop(void* res);

/**
 * @fn uint32_t HAL_GPADC_PollForConversion(void* res, uint32_t Timeout)
 * @brief Poll for conversion completion
 * @param[in] res Resource handle from successful initialization
 * @param[in] Timeout Maximum time to wait for conversion (milliseconds)
 * @return Status code: 0 if completed within timeout, error code otherwise
 * @details Blocks execution until conversion finishes or timeout occurs
 */
uint32_t HAL_GPADC_PollForConversion(void* res, uint32_t Timeout);

/**
 * @fn uint32_t HAL_GPADC_Start_IT(void* res)
 * @brief Start GPADC conversion with interrupt support
 * @param[in] res Resource handle from successful initialization
 * @return Status code indicating operation result
 * @details Enables interrupt generation upon conversion completion
 */
uint32_t HAL_GPADC_Start_IT(void* res);

/**
 * @fn uint32_t HAL_GPADC_Stop_IT(void* res)
 * @brief Stop GPADC conversion with interrupt support
 * @param[in] res Resource handle from successful initialization
 * @return Status code indicating operation result
 * @details Disables interrupt generation from conversion events
 */
uint32_t HAL_GPADC_Stop_IT(void* res);

/**
 * @fn uint16_t HAL_GPADC_GetValue(void* res, uint32_t channelnum)
 * @brief Read converted value from specified channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channelnum Channel number (see GPADC_CHANNEL_TYPE)
 * @return 16-bit digital value representing analog input
 * @note Returns last converted value for selected channel
 */
uint16_t HAL_GPADC_GetValue(void* res, uint32_t channelnum);

/**
 * @fn uint32_t HAL_GPADC_SetTriggerNum(void* res, uint32_t trignum)
 * @brief Set number of hardware triggers
 * @param[in] res Resource handle from successful initialization
 * @param[in] trignum Number of trigger sources to configure
 * @return Status code indicating operation result
 * @details Configures multiple trigger points for scanned channels
 */
uint32_t HAL_GPADC_SetTriggerNum(void* res, uint32_t trignum);

/**
 * @fn uint32_t HAL_GPADC_SetSampleTime(void* res, uint32_t time)
 * @brief Set sampling time duration
 * @param[in] res Resource handle from successful initialization
 * @param[in] time Sampling time in clock cycles
 * @return Status code indicating operation result
 * @details Longer sampling times improve accuracy but reduce throughput
 */
uint32_t HAL_GPADC_SetSampleTime(void* res, uint32_t time);

/**
 * @fn uint32_t HAL_GPADC_SetSetupWaitTime(void* res, uint8_t waitTime)
 * @brief Set setup waiting time before conversion
 * @param[in] res Resource handle from successful initialization
 * @param[in] waitTime Wait time in system clock cycles
 * @return Status code indicating operation result
 * @details Ensures stable sampling by adding delay between channel switches
 */
uint32_t HAL_GPADC_SetSetupWaitTime(void* res, uint8_t waitTime);

/**
 * @fn uint32_t HAL_GPADC_SetFifoThd(void* res, uint8_t channelnum, uint8_t threshold)
 * @brief Set FIFO threshold level for specified channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channelnum Channel number (see GPADC_CHANNEL_TYPE)
 * @param[in] threshold Threshold level (count of samples) triggering interrupt
 * @return Status code indicating operation result
 * @details Configures watermark level for FIFO buffer management
 */
uint32_t HAL_GPADC_SetFifoThd(void* res, uint8_t channelnum, uint8_t threshold);

/**
 * @fn uint32_t HAL_GPADC_FifoClear(void* res, uint8_t channelnum, uint8_t clear)
 * @brief Clear FIFO buffer contents
 * @param[in] res Resource handle from successful initialization
 * @param[in] channelnum Channel number (see GPADC_CHANNEL_TYPE)
 * @param[in] clear Clear flag (non-zero to clear, zero to leave unchanged)
 * @return Status code indicating operation result
 * @details Forces FIFO buffer reset while maintaining current configuration
 */
uint32_t HAL_GPADC_FifoClear(void* res, uint8_t channelnum, uint8_t clear);

/**
 * @fn uint32_t HAL_GPADC_EnableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type)
 * @brief Enable FIFO interrupt for specified condition
 * @param[in] res Resource handle from successful initialization
 * @param[in] channelnum Channel number (see GPADC_CHANNEL_TYPE)
 * @param[in] type Interrupt type (see GPADC_FIFOINT_TYPE)
 * @return Status code indicating operation result
 * @details Registers interrupt handler for specified FIFO condition
 */
uint32_t HAL_GPADC_EnableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type);

/**
 * @fn uint32_t HAL_GPADC_DisableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type)
 * @brief Disable FIFO interrupt for specified condition
 * @param[in] res Resource handle from successful initialization
 * @param[in] channelnum Channel number (see GPADC_CHANNEL_TYPE)
 * @param[in] type Interrupt type (see GPADC_FIFOINT_TYPE)
 * @return Status code indicating operation result
 * @details Removes previously registered interrupt handler
 */
uint32_t HAL_GPADC_DisableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type);

/**
 * @fn uint32_t HAL_GPADC_EnableCmpInterrupt(void* res)
 * @brief Enable comparator interrupt
 * @param[in] res Resource handle from successful initialization
 * @return Status code indicating operation result
 * @details Activates interrupt generation when comparator threshold crossed
 */
uint32_t HAL_GPADC_EnableCmpInterrupt(void* res);

/**
 * @fn uint32_t HAL_GPADC_DisableCmpInterrupt(void* res)
 * @brief Disable comparator interrupt
 * @param[in] res Resource handle from successful initialization
 * @return Status code indicating operation result
 * @details Deactivates comparator interrupt generation
 */
uint32_t HAL_GPADC_DisableCmpInterrupt(void* res);

/**
 * @fn uint32_t HAL_GPADC_SetVinBuf_Enable(void* res, uint8_t enable)
 * @brief Enable/disable input buffer for voltage measurements
 * @param[in] res Resource handle from successful initialization
 * @param[in] enable Non-zero to enable, zero to disable
 * @return Status code indicating operation result
 * @details Controls presence of sample-and-hold buffer circuitry
 */
uint32_t HAL_GPADC_SetVinBuf_Enable(void* res, uint8_t enable);

/**
 * @fn uint32_t HAL_GPADC_SetVrefSel(void* res, GPADC_VREF_TYPE vrefsel)
 * @brief Select reference voltage source
 * @param[in] res Resource handle from successful initialization
 * @param[in] vrefsel Reference voltage selection (implementation-specific codes)
 * @return Status code indicating operation result
 * @details Chooses between internal bandgap reference and external references
 */
uint32_t HAL_GPADC_SetVrefSel(void* res, GPADC_VREF_TYPE vrefsel);

/**
 * @fn uint32_t HAL_GPADC_SetKeysenseTrigger_enable(void* res, uint8_t enable)
 * @brief Enable/disable key sense trigger functionality
 * @param[in] res Resource handle from successful initialization
 * @param[in] enable Non-zero to enable, zero to disable
 * @return Status code indicating operation result
 * @details Allows keyboard scanning to initiate conversions automatically
 */
uint32_t HAL_GPADC_SetKeysenseTrigger_enable(void* res, uint8_t enable);

/**
 * @fn uint32_t HAL_GPADC_SetGptTrigger_enable(void* res, uint8_t enable)
 * @brief Enable/disable general purpose timer trigger
 * @param[in] res Resource handle from successful initialization
 * @param[in] enable Non-zero to enable, zero to disable
 * @return Status code indicating operation result
 * @details Permits synchronization with system timer events
 */
uint32_t HAL_GPADC_SetGptTrigger_enable(void* res, uint8_t enable);

/**
 * @fn int32_t HAL_GPADC_RegisterChannelCallback(void *res, GPADC_CHANNEL_TYPE channel, CSK_GPADC_SignalEvent_t cb_event)
 * @brief Register per-channel event callback function
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Channel number (see GPADC_CHANNEL_TYPE)
 * @param[in] cb_event Function pointer to callback routine
 * @return Negative error code on failure, non-negative status code on success
 * @details Associates custom handler with specific channel events
 */
int32_t HAL_GPADC_RegisterChannelCallback(void *res, GPADC_CHANNEL_TYPE channel, CSK_GPADC_SignalEvent_t cb_event);

/**
 * @fn int32_t HAL_GPADC_RegisterCompleteCallback(void *res, CSK_GPADC_SignalEvent_t cb_event)
 * @brief Register global completion callback function
 * @param[in] res Resource handle from successful initialization
 * @param[in] cb_event Function pointer to callback routine
 * @return Negative error code on failure, non-negative status code on success
 * @details Sets handler for all conversion complete events
 */
int32_t HAL_GPADC_RegisterCompleteCallback(void *res, CSK_GPADC_SignalEvent_t cb_event);

/**
 * @fn uint32_t ls_rand(void)
 * @brief Hardware random number generator interface
 * @return 32-bit pseudo-random number
 * @note Direct access to embedded hardware entropy source
 */
uint32_t ls_rand(void);
/**
  * @}
  */ /* End of group GPADC_Exported_Functions */

/**
  * @}
  */ /* End of group GPADC */

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_GPADC_H */
