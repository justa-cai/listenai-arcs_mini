/**
 * @file    Driver_KEYSENSE.h
 * @author  ListenAI Application Team
 * @brief   Header file for KEYSENSE Hardware Abstraction Layer (HAL) module.
 *          This file contains definitions and function declarations for the KEYSENSE driver.
 * @details The KEYSENSE HAL provides interfaces to initialize, control, and manage key sensing operations.
 *          It supports multiple interrupt modes and event callback registration.
 * @note    Compliant with BSD 3-Clause license as specified in the copyright notice.
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _DRIVER_KEYSENSE_H
#define _DRIVER_KEYSENSE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"

/** @defgroup KEYSENSE
  * @brief KEYSENSE HAL module driver
  * @{
  */
/** @defgroup KEYSENSE_Exported_Constants KEYSENSE Exported Constants
  * @{
  */
/** @defgroup KEYSENSE_API_Version KEYSENSE API Version
  * @{
  */
/**
 * @def CSK_KEYSENSE_API_VERSION
 * @brief API version number following major.minor format
 * @details Indicates the application programming interface version required by this driver.
 */
#define CSK_KEYSENSE_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)
/** @} */ /* End of group KEYSENSE_API_Version */

/** @defgroup KEYSENSE_DRV_Version KEYSENSE DRV Version
  * @{
  */
/**
 * @def CSK_KEYSENSE_DRV_VERSION
 * @brief Driver implementation version number following major.minor format
 * @details Specifies the actual driver implementation version number.
 */
#define CSK_KEYSENSE_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)
/** @} */ /* End of group KEYSENSE_DRV_Version */

#ifndef NULL
/**
 * @def NULL
 * @brief Standard null pointer definition if not previously defined
 * @details Ensures consistent null pointer definition across platforms.
 */
#define NULL                        (void *)0
#endif

/** @defgroup KEYSENSE_Control Mode KEYSENSE control Mode
  * @{
  */
/*---------------------Control Mode Configuration------------------------------*/
/**
 * @def CSK_KEYSENSE_ADC_TRIGGER_THD_Pos
 * @brief Bit position for ADC trigger threshold configuration
 * @details Position within register where ADC trigger threshold bits start.
 */
#define CSK_KEYSENSE_ADC_TRIGGER_THD_Pos             0

/**
 * @def CSK_KEYSENSE_ADC_TRIGGER_THD_Msk
 * @brief Bit mask for ADC trigger threshold configuration
 * @details Masking bits used to access ADC trigger threshold settings.
 */
#define CSK_KEYSENSE_ADC_TRIGGER_THD_Msk             (0xFFUL << CSK_KEYSENSE_ADC_TRIGGER_THD_Pos)

/**
 * @def CSK_KEYSENSE_WAKEUP_CNT_THD_Pos
 * @brief Bit position for wakeup counter threshold configuration
 * @details Position within register where wakeup counter threshold bits start.
 */
#define CSK_KEYSENSE_WAKEUP_CNT_THD_Pos              16

/**
 * @def CSK_KEYSENSE_WAKEUP_CNT_THD_Msk
 * @brief Bit mask for wakeup counter threshold configuration
 * @details Masking bits used to access wakeup counter threshold settings.
 */
#define CSK_KEYSENSE_WAKEUP_CNT_THD_Msk            	 (0xFFUL << CSK_KEYSENSE_WAKEUP_CNT_THD_Pos)

/**
 * @def CSK_KEYSENSE_MODE_NUM
 * @brief Number of supported operation modes
 * @details Total count of distinct operation modes available in the driver.
 */
#define CSK_KEYSENSE_MODE_NUM					  0x04

/**
 * @def CSK_KEYSENSE_THD
 * @brief Default combined threshold value
 * @details Preset combined value for both ADC trigger and wakeup counter thresholds.
 */
#define CSK_KEYSENSE_THD                          0x00200040
/** @} */ /* End of group KEYSENSE_Control */
/** @} */ /* End of group KEYSENSE_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup KEYSENSE_Exported_Types KEYSENSE Exported Types
  * @{
  */
/*----- KEYSENSE Interrupt Mode Type Definitions -----*/
/**
 * @enum KEYSENSE_INTERRUPT_MODE_TYPE
 * @brief Enumeration of available interrupt modes for key sensing events
 * @var CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP     Generate interrupt on wakeup event
 * @var CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER  Generate interrupt on ADC trigger event
 * @var CSK_KEYSENSE_INTERRUPT_MODE_RELEASE    Generate interrupt on key release event
 * @var CSK_KEYSENSE_INTERRUPT_MODE_PRESS      Generate interrupt on key press event
 */
typedef enum {
	CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP                 	= 0x01,
	CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER              = 0x02,
	CSK_KEYSENSE_INTERRUPT_MODE_RELEASE                 = 0x04,
	CSK_KEYSENSE_INTERRUPT_MODE_PRESS                  	= 0x08,
} KEYSENSE_INTERRUPT_MODE_TYPE;

/*----- KEYSENSE Operation Mode Type Definitions -----*/
/**
 * @enum KEYSENSE_MODE_TYPE
 * @brief Enumeration of operation modes for key sensing functionality
 * @var CSK_KEYSENSE_WAKEUP       Wakeup detection mode
 * @var CSK_KEYSENSE_ADCTRIG      ADC trigger detection mode
 * @var CSK_KEYSENSE_RELEASE      Key release detection mode
 * @var CSK_KEYSENSE_PRESS        Key press detection mode
 */
typedef enum {
	CSK_KEYSENSE_WAKEUP                	= 0,
	CSK_KEYSENSE_ADCTRIG                  	= 1,
	CSK_KEYSENSE_RELEASE                  	= 2,
	CSK_KEYSENSE_PRESS                  	= 3,
} KEYSENSE_MODE_TYPE;

/**
  * @brief   Key sense module signal event callback function type definition
  * @typedef CSK_KEYSENSE_SignalEvent_t
  * @param[in] workspace Pointer to key sense module workspace
  *                     - Provides access to internal state and data structures of key sense module
  *                     - Delivers context information when event occurs
  * @return None
  *
  * @details This callback function type is used to handle signal events generated by the key sense module.
  *          When a key sense event occurs (such as key press, key release, or touch detection),
  *          the registered callback function of this type will be invoked.
  *
  * @note 1. This callback function must be registered through configuration API before using key sense module
  * @note 2. Callback function should execute quickly and avoid blocking operations to ensure timely response
  * @note 3. Workspace pointer should be cast to appropriate structure type based on specific implementation
  * @note 4. When called in interrupt context, the function should maintain reentrancy
  */
typedef void (*CSK_KEYSENSE_SignalEvent_t)(void* workspace);

/** @} */ /* End of group KEYSENSE_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup KEYSENSE_Exported_Functions KEYSENSE Exported Functions
  * @{
  */
/**
 * @fn void* KEYSENSE0(void)
 * @brief Obtains handle to the first KEYSENSE instance
 * @return Void pointer to KEYSENSE instance handle
 * @details Returns a handle to access the primary KEYSENSE peripheral instance.
 */
void* KEYSENSE0(void);

/**
 * @fn int32_t HAL_KEYSENSE_Initialize(void *res)
 * @brief Initializes the KEYSENSE peripheral
 * @param[out] res Opaque handle to store initialization context
 * @return Negative error code on failure, non-negative status code on success
 * @details Performs necessary hardware setup and resource allocation for KEYSENSE functionality.
 */
int32_t HAL_KEYSENSE_Initialize(void *res);

/**
 * @fn int32_t HAL_KEYSENSE_Uninitialize(void *res)
 * @brief Deinitializes and releases KEYSENSE resources
 * @param[in] res Initialization handle obtained from successful initialization
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Frees allocated resources and disables peripheral clocks.
 */
int32_t HAL_KEYSENSE_Uninitialize(void *res);

/**
 * @fn int32_t HAL_KEYSENSE_Control(void* res, uint32_t control)
 * @brief Sends control commands to KEYSENSE peripheral
 * @param[in] res Initialization handle
 * @param[in] control Control command word
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Allows low-level control over peripheral behavior through command words.
 */
int32_t HAL_KEYSENSE_Control(void* res, uint32_t control);

/**
 * @fn int32_t HAL_KEYSENSE_InterruptEnable(void* res, KEYSENSE_INTERRUPT_MODE_TYPE intmode)
 * @brief Enables specific interrupt modes for KEYSENSE events
 * @param[in] res Initialization handle
 * @param[in] intmode Interrupt mode(s) to enable (@ref KEYSENSE_INTERRUPT_MODE_TYPE)
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Activates selected interrupt sources based on provided mode flags.
 */
int32_t HAL_KEYSENSE_InterruptEnable(void* res, KEYSENSE_INTERRUPT_MODE_TYPE intmode);

/**
 * @fn int32_t HAL_KEYSENSE_InterruptDisable(void* res, KEYSENSE_INTERRUPT_MODE_TYPE intmode)
 * @brief Disables specific interrupt modes for KEYSENSE events
 * @param[in] res Initialization handle
 * @param[in] intmode Interrupt mode(s) to disable (@ref KEYSENSE_INTERRUPT_MODE_TYPE)
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Deactivates selected interrupt sources based on provided mode flags.
 */
int32_t HAL_KEYSENSE_InterruptDisable(void* res, KEYSENSE_INTERRUPT_MODE_TYPE intmode);

/**
 * @fn int32_t HAL_KEYSENSE_Enable(void* res)
 * @brief Enables KEYSENSE functionality
 * @param[in] res Initialization handle
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Starts the key sensing operation after successful initialization.
 */
int32_t HAL_KEYSENSE_Enable(void* res);

/**
 * @fn int32_t HAL_KEYSENSE_Disable(void* res)
 * @brief Disables KEYSENSE functionality
 * @param[in] res Initialization handle
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Stops the key sensing operation while maintaining initialization state.
 */
int32_t HAL_KEYSENSE_Disable(void* res);

/**
 * @fn int32_t HAL_KEYSENSE_RegisterCallback(void *res, KEYSENSE_MODE_TYPE mode, CSK_KEYSENSE_SignalEvent_t cb_event)
 * @brief Registers an event callback for specific key sensing modes
 * @param[in] res Initialization handle
 * @param[in] mode Operation mode (@ref KEYSENSE_MODE_TYPE)
 * @param[in] cb_event Callback function pointer
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 * @details Associates a user-defined callback function with specific operation modes.
 */
int32_t HAL_KEYSENSE_RegisterCallback(void *res, KEYSENSE_MODE_TYPE mode, CSK_KEYSENSE_SignalEvent_t cb_event);
/** @} */ /* End of group KEYSENSE_Exported_Functions */
/**
  * @}
  */ /* End of group KEYSENSE */
#ifdef __cplusplus
}
#endif

#endif /* _DRIVER_KEYSENSE_H */
