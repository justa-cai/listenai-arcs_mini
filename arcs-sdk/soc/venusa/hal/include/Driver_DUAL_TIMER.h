/**
  ******************************************************************************
  * @file    Driver_DUAL_TIMER.h
  * @author  ListenAI Application Team
  * @brief   Header file of DUAL TIMER HAL module.
  * @date    2020-08-04
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

#ifndef __DRIVER_DUALTIMERS_H__
#define __DRIVER_DUALTIMERS_H__

#include "Driver_Common.h"

/** @defgroup DUALTIMER
  * @brief TIMER HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup TIMER_Exported_Constants TIMER Exported Constants
  * @{
  */

/** @defgroup TIMER_API_Version TIMER API Version
  * @{
  */
#define CSK_TIMER_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /**< API version */
/** @} */ /* End of group TIMER_API_Version */

/** @defgroup TIMER_Channel_Parameter TIMER Channel Parameter
  * @{
  */
/****** TIMER Channel Parameter *****/
#define CSK_TIMER_CHANNEL_0                 (0)  /**< Timer channel 0 */
#define CSK_TIMER_CHANNEL_1                 (1)  /**< Timer channel 1 */
/** @} */ /* End of group TIMER_Channel_Parameter */

/** @defgroup TIMER_Control_Codes TIMER Control Codes
  * @{
  */
/****** TIMER Control Codes *****/

/** @defgroup TIMER_Prescale TIMER Prescale
  * @{
  */
#define CSK_TIMER_PRESCALE_Pos              (0)                        /**< Prescale position */
#define CSK_TIMER_PRESCALE_Msk              (3UL << CSK_TIMER_PRESCALE_Pos) /**< Prescale mask */
#define CSK_TIMER_PRESCALE_Divide_1         (0UL << CSK_TIMER_PRESCALE_Pos) /**< Divide by 1 */
#define CSK_TIMER_PRESCALE_Divide_16        (1UL << CSK_TIMER_PRESCALE_Pos) /**< Divide by 16 */
#define CSK_TIMER_PRESCALE_Divide_256       (2UL << CSK_TIMER_PRESCALE_Pos) /**< Divide by 256 */
/** @} */ /* End of group TIMER_Prescale */

/** @defgroup TIMER_Size TIMER Size
  * @{
  */
#define CSK_TIMER_SIZE_Pos                  (2)                        /**< Size position */
#define CSK_TIMER_SIZE_Msk                  (1UL << CSK_TIMER_SIZE_Pos) /**< Size mask */
#define CSK_TIMER_SIZE_16Bit                (0UL << CSK_TIMER_SIZE_Pos) /**< 16-bit timer */
#define CSK_TIMER_SIZE_32Bit                (1UL << CSK_TIMER_SIZE_Pos) /**< 32-bit timer */
/** @} */ /* End of group TIMER_Size */

/** @defgroup TIMER_Mode TIMER Mode
  * @{
  */
#define CSK_TIMER_MODE_Pos                  (3)                        /**< Mode position */
#define CSK_TIMER_MODE_Msk                  (3UL << CSK_TIMER_MODE_Pos) /**< Mode mask */
#define CSK_TIMER_MODE_FreeRunning          (0UL << CSK_TIMER_MODE_Pos) /**< Free-running mode */
#define CSK_TIMER_MODE_Periodic             (1UL << CSK_TIMER_MODE_Pos) /**< Periodic mode */
#define CSK_TIMER_MODE_OneShot              (2UL << CSK_TIMER_MODE_Pos) /**< One-shot mode */
/** @} */ /* End of group TIMER_Mode */

/** @defgroup TIMER_Interrupt TIMER Interrupt
  * @{
  */
#define CSK_TIMER_INTERRUPT_Pos             (5)                        /**< Interrupt position */
#define CSK_TIMER_INTERRUPT_Msk             (1UL << CSK_TIMER_INTERRUPT_Pos) /**< Interrupt mask */
#define CSK_TIMER_INTERRUPT_Enabled         (0UL << CSK_TIMER_INTERRUPT_Pos) /**< Interrupt enabled */
#define CSK_TIMER_INTERRUPT_Disabled        (1UL << CSK_TIMER_INTERRUPT_Pos) /**< Interrupt disabled */
/** @} */ /* End of group TIMER_Interrupt */

/** @} */ /* End of group TIMER_Control_Codes */

/**
  * @}
  */ /* End of group TIMER_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup TIMER_Exported_Types TIMER Exported Types
  * @{
  */

/** @defgroup TIMER_Callback TIMER Callback
  * @{
  */
/**
 * @brief Timer event callback function type
 *
 * This function type defines the callback that will be invoked when a timer event occurs.
 *
 * @param event The event that triggered the callback.
 * @param workspace User workspace pointer for application context.
 */
typedef void (*CSK_TIMER_SignalEvent_t) (uint32_t event, void* workspace);
/** @} */ /* End of group TIMER_Callback */

/** @defgroup TIMER_Events TIMER Events
  * @{
  */
#define CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH0    (1UL << 0)  /**< One step complete event for channel 0 */
#define CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH1    (1UL << 1)  /**< One step complete event for channel 1 */
/** @} */ /* End of group TIMER_Events */

/**
  * @}
  */ /* End of group TIMER_Exported_Types */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup TIMER_Exported_Macros TIMER Exported Macros
  * @{
  */

/**
  * @}
  */ /* End of group TIMER_Exported_Macros */

/* Exported functions --------------------------------------------------------*/
/** @defgroup TIMER_Exported_Functions TIMER Exported Functions
  * @{
  */

/**
 * @brief Get dual timers driver version.
 * @return \ref CSK_DRIVER_VERSION
 */
CSK_DRIVER_VERSION CSK_DUALTIMERS_GetVersion(void);

/**
 * @brief Initialize dual timers.
 * @param res Pointer to dual timers resources.
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_Initialize(void* res);

/**
 * @brief Uninitialize dual timers.
 * @param res Pointer to dual timers resources.
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_Uninitialize(void* res);

/**
 * @brief Control dual timers power.
 * @param res Pointer to dual timers resources.
 * @param state Power state
 *        - \ref CSK_POWER_OFF: Power off
 *        - \ref CSK_POWER_LOW: Low power mode
 *        - \ref CSK_POWER_FULL: Full power
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @brief Control dual timers operation.
 * @param res Pointer to dual timers resources.
 * @param control Control operation code
 * @param ch Timer channel (0 or 1)
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_Control(void* res, uint32_t control, uint32_t ch);

/**
 * @brief Set timer callback function.
 * @param res Pointer to dual timers resources.
 * @param channel Timer channel (0 or 1)
 * @param cb_event Callback function pointer
 * @param workspace User workspace pointer
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_SetTimerCallback(void* res, uint32_t channel, CSK_TIMER_SignalEvent_t cb_event, void* workspace);

/**
 * @brief Set timer period by count.
 * @param res Pointer to dual timers resources.
 * @param channel Timer channel (0 or 1)
 * @param count Timer period count
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_SetTimerPeriodByCount(void* res, uint32_t channel, uint32_t count);

/**
 * @brief Start timer.
 * @param res Pointer to dual timers resources.
 * @param channel Timer channel (0 or 1)
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_StartTimer(void* res, uint32_t channel);

/**
 * @brief Read timer count.
 * @param res Pointer to dual timers resources.
 * @param channel Timer channel (0 or 1)
 * @param count Pointer to store the current timer count
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_ReadTimerCount(void* res, uint32_t channel, uint32_t *count);

/**
 * @brief Stop timer.
 * @param res Pointer to dual timers resources.
 * @param channel Timer channel (0 or 1)
 * @return Execution status
 *         - @b 0: Operation successful
 *         - @b -1: Operation failed
 */
int32_t DUALTIMERS_StopTimer(void* res, uint32_t channel);

/**
 * @brief Get dual timers 0 instance.
 * @return Pointer to dual timers 0 resources
 */
void* DUALTIMERS0(void);

/**
 * @brief Get dual timers 1 instance.
 * @return Pointer to dual timers 1 resources
 */
void* DUALTIMERS1(void);

/** @} */ /* End of group TIMER_Exported_Functions */

/**
  * @}
  */ /* End of group DUALTIMER */

 #endif /*  */
 
