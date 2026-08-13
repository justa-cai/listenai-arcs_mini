/**
 * @file    Driver_GPT_TIMER.h
 * @author  USER
 * @brief   Header file for General Purpose Timer (GPT) timer functionality
 * @details This file contains specialized definitions and function declarations
 *          for timer operations including configuration structures, control functions,
 *          count reading utilities and callback registration mechanisms.
 * @created Created on: April 24, 2025
 */

#ifndef DRIVER_GPT_TIMER_H_
#define DRIVER_GPT_TIMER_H_

#include "Driver_Common.h"
#include "Driver_GPT_Common.h"

/** @defgroup GPT_TIMER
  * @brief GPT_TIMER HAL module driver
  * @{
  */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup GPT_TIMER_Exported_Macros GPT TIMER Exported Macros
  * @{
  */
/**
 * @def HAL_GPT_TIMER_16BITS_INDEX_0
 * @brief Index value identifying 16-bit timer module
 * @details Maps to internal hardware resource allocation for 16-bit timer instance
 */
#define HAL_GPT_TIMER_16BITS_INDEX_0				0

/**
 * @def HAL_GPT_TIMER_8BITS_INDEX_0
 * @brief Index value identifying first 8-bit timer module
 * @details Maps to internal hardware resource allocation for first 8-bit timer instance
 */
#define HAL_GPT_TIMER_8BITS_INDEX_0					3

/**
 * @def HAL_GPT_TIMER_8BITS_INDEX_1
 * @brief Index value identifying second 8-bit timer module
 * @details Maps to internal hardware resource allocation for second 8-bit timer instance
 */
#define HAL_GPT_TIMER_8BITS_INDEX_1					5
/**
  * @}
  */ /* End of group GPT_TIMER_Exported_Macros */

/* Exported types ------------------------------------------------------------*/
/** @defgroup GPT_TIMER_Exported_Types GPT_TIMER Exported Types
  * @{
  */
/**
 * @enum GPT_TIMER_CountMode_t
 * @brief Counting direction modes for GPT timers
 * @var GPT_TIMER_COUNTMODE_UP Count upward from 0 to period
 * @var GPT_TIMER_COUNTMODE_DOWN Count downward from period to 0
 * @var GPT_TIMER_COUNTMODE_UPDOWM Toggle between up/down counting at extremes
 */
typedef enum {
	GPT_TIMER_COUNTMODE_UP = 0,       ///< Upward counting mode
	GPT_TIMER_COUNTMODE_DOWN,        ///< Downward counting mode
	GPT_TIMER_COUNTMODE_UPDOWM      ///< Bidirectional counting mode
} GPT_TIMER_CountMode_t;

/**
 * @enum GPT_TIMER_RunMode_t
 * @brief Operation modes for GPT timers
 * @var GPT_TIMER_RUNMODE_SINGLE Single cycle operation (stop after completion)
 * @var GPT_TIMER_RUNMODE_REPEAT Repeated cycling (auto-reload on completion)
 * @var GPT_TIMER_RUNMODE_FREE_RUN Free running mode (continuous counting without stop)
 * @var GPT_TIMER_RUNMODE_KPPEGO Special KPP trigger mode (hardware controlled start)
 */
typedef enum {
	GPT_TIMER_RUNMODE_SINGLE  = 0,    ///< Single shot operation
	GPT_TIMER_RUNMODE_REPEAT,         ///< Periodic repetition
	GPT_TIMER_RUNMODE_FREE_RUN,       ///< Continuous free-running mode
	GPT_TIMER_RUNMODE_KPPEGO         ///< KPP edge triggered operation
} GPT_TIMER_RunMode_t;

/**
 * @struct GPT_TIMER_Config_Para_t
 * @brief Configuration parameters for GPT timer instances
 * @var index Hardware module index selection
 * @var run_mode Timer operation mode (@ref GPT_TIMER_RunMode_t)
 * @var cnt_mode Counting direction mode (@ref GPT_TIMER_CountMode_t)
 */
typedef struct {
	uint8_t index;                    ///< Hardware module index
	GPT_TIMER_RunMode_t run_mode;     ///< Operation mode configuration
	GPT_TIMER_CountMode_t cnt_mode;   ///< Counting direction configuration
} GPT_TIMER_Config_Para_t;

/**
 * @struct GPT_TIMER_Info_t
 * @brief Timer channel information structure
 * @var workspace Array of per-channel working memory pointers
 */
typedef struct {
    void* workspace[GPT_NUMBER_OF_CHANNELS]; ///< Channel working memory areas
} GPT_TIMER_Info_t;
/**
  * @}
  */ /* End of group GPT_TIMER_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup GPT_TIMER_Exported_Functions GPT_TIMER Exported Functions
  * @{
  */

/**
 * @fn int32_t HAL_GPT_TimerControl(void* res, GPT_Channel_Num_t channel, GPT_TIMER_Config_Para_t* para)
 * @brief Configures timer parameters for specified channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] para Configuration parameters (@ref GPT_TIMER_Config_Para_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Applies operational mode, counting direction and hardware module mapping
 */
int32_t HAL_GPT_TimerControl(void* res, GPT_Channel_Num_t channel, GPT_TIMER_Config_Para_t* para);

/**
 * @fn int32_t HAL_GPT_ReadTimerCount(void *res, GPT_Channel_Num_t channel, uint8_t ch_mode, uint16_t *count)
 * @brief Reads current timer counter value
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] ch_mode Channel operating mode selector
 * @param[out] count Pointer to store retrieved counter value
 * @return Negative error code on failure, non-negative status code on success
 * @note Returns the current counter value based on selected channel mode
 */
int32_t HAL_GPT_ReadTimerCount(void *res, GPT_Channel_Num_t channel, uint8_t ch_mode, uint16_t *count);

/**
 * @fn int32_t HAL_GPT_RegisterTimerCallback(void* res, GPT_Channel_Num_t channel, CSK_GPT_SignalEvent_t cb_event, void* workspace)
 * @brief Registers event callback for timer notifications
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] cb_event Event handler function (@ref CSK_GPT_SignalEvent_t)
 * @param[in] workspace User-supplied context pointer passed to callback
 * @return Negative error code on failure, non-negative status code on success
 * @note Callback executes from interrupt context - keep processing minimal
 */
int32_t HAL_GPT_RegisterTimerCallback(void* res, GPT_Channel_Num_t channel, CSK_GPT_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t HAL_GPT_SetTimerPeriodByCount(void* res, GPT_Channel_Num_t channel, uint8_t ch_mode, uint16_t count)
 * @brief Sets timer period using direct count value
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] ch_mode Channel operating mode selector
 * @param[in] count Desired period value in timer counts
 * @return Negative error code on failure, non-negative status code on success
 * @details Programs the timer's period register directly with count value
 */
int32_t HAL_GPT_SetTimerPeriodByCount(void* res, GPT_Channel_Num_t channel, uint8_t ch_mode, uint16_t count);

/**
 * @fn int32_t HAL_GPT_StartTimer(void* res, GPT_Channel_Num_t channel, uint8_t ch_mode)
 * @brief Starts timer operation on specified channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target GPT channel number (@ref GPT_Channel_Num_t)
 * @param[in] ch_mode Channel operating mode selector
 * @return Negative error code on failure, non-negative status code on success
 * @details Initiates timer counting based on previously configured parameters
 */
int32_t HAL_GPT_StartTimer(void* res, GPT_Channel_Num_t channel, uint8_t ch_mode);
/** @} */ /* End of group GPT_TIMER_Exported_Functions */
/**
  * @}
  */ /* End of group GPT_TIMER */

#endif /* DRIVER_GPT_TIMER_H_ */
