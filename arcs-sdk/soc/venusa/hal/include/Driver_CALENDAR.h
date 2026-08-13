/*!
 * @file Driver_CALENDAR.h
 * @brief Real-Time Clock (RTC) and Alarm Functionality Driver Interface
 *        This file contains the interface definitions for the CSK Calendar/RTC hardware abstraction layer.
 *        Provides timekeeping, alarm setting, and event notification capabilities.
 *        Created on: May 27, 2025
 *        Author: USER
 */

#ifndef DRIVER_CALENDAR_H_
#define DRIVER_CALENDAR_H_

#include "venusa_ap.h"
#include "Driver_Common.h"

/** @defgroup CALENDAR
  * @brief CALENDAR HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup CALENDAR_Exported_Constants CALENDAR Exported Constants
  * @{
  */

/** @defgroup CALENDAR_CTRL Configuration
  * @{
  */

/// Alarm enable bit in control register
#define CSK_CALENDAR_CTRL_ALARM_EN        (1UL << 1) ///< Enable alarm interrupt generation
/// Calibration mode enable bit
#define CSK_CALENDAR_CTRL_CALIBRATION_EN  (1UL << 2) ///< Enable clock calibration mode
/// Hour rollover interrupt enable
#define CSK_CALENDAR_CTRL_HOUR_INT        (1UL << 4) ///< Generate interrupt at hour rollover
/// Minute rollover interrupt enable
#define CSK_CALENDAR_CTRL_MIN_INT         (1UL << 5) ///< Generate interrupt at minute rollover
/// Second rollover interrupt enable
#define CSK_CALENDAR_CTRL_SEC_INT         (1UL << 6) ///< Generate interrupt at second rollover
/** @} */ /* End of group CALENDAR_CTRL */

/** @defgroup CALENDAR_Events Alarm match interrupt event flag
  * @{
  */
/// Alarm match interrupt event flag
#define CSK_CALENDAR_EVENT_ALARM_INT     (1UL << 0) ///< Alarm condition met
/// Hour counter rollover event flag
#define CSK_CALENDAR_EVENT_HOUR_INT      (1UL << 2) ///< Hour value wrapped around
/// Minute counter rollover event flag
#define CSK_CALENDAR_EVENT_MIN_INT       (1UL << 3) ///< Minute value wrapped around
/// Second counter rollover event flag
#define CSK_CALENDAR_EVENT_SEC_INT       (1UL << 4) ///< Second value wrapped around
/** @} */ /* End of group CALENDAR_Events */

/**
  * @}
  */ /* End of group CALENDAR_Exported_Constants */


/* Exported types ------------------------------------------------------------*/
/** @defgroup CALENDAR_Exported_Types CALENDAR Exported Types
  * @{
  */

/**
 * @typedef CSK_CALENDAR_SignalEvent_t
 * @brief Event callback function pointer type for calendar events
 * @param[in] event Event identification code
 * @param[in] workspace User-defined context data pointer
 */
typedef void
(*CSK_CALENDAR_SignalEvent_t)(uint32_t event, void* workspace);

/**
 * @struct CSK_CALENDAR_TIME
 * @brief Complete date/time structure
 * @var year Full year (e.g., 2025)
 * @var month Month number (1-12)
 * @var weekend Day of week (0=Sunday to 6=Saturday)
 * @var day Day of month (1-31)
 * @var hour Hours in 24-hour format (0-23)
 * @var min Minutes (0-59)
 * @var sec Seconds (0-59)
 */
typedef struct _CSK_CALENDAR_TIME
{
    uint32_t year;   ///< Year component
    uint32_t month;  ///< Month component
    uint32_t weekend;///< Day of week component
    uint32_t day;    ///< Day of month component
    uint32_t hour;   ///< Hour component
    uint32_t min;    ///< Minute component
    uint32_t sec;    ///< Second component
} CSK_CALENDAR_TIME;

/**
 * @struct CSK_CALENDAR_ALARM
 * @brief Alarm trigger condition structure
 * @var year Year match criterion (0 = don't care)
 * @var month Month match criterion (0 = don't care)
 * @var day Day of month match criterion (0 = don't care)
 * @var hour Hour match criterion (0 = don't care)
 * @var min Minute match criterion (0 = don't care)
 * @var sec Second match criterion (0 = don't care)
 */
typedef struct _CSK_CALENDAR_ALARM
{
    uint32_t year;  ///< Year comparison value
    uint32_t month; ///< Month comparison value
    uint32_t day;   ///< Day comparison value
    uint32_t hour;  ///< Hour comparison value
    uint32_t min;   ///< Minute comparison value
    uint32_t sec;   ///< Second comparison value
} CSK_CALENDAR_ALARM;

/**
  * @}
  */ /* End of group CALENDAR_Exported_Types */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup CALENDAR_Exported_Macros CALENDAR Exported Macros
  * @{
  */

/**
  * @}
  */ /* End of group CALENDAR_Exported_Macros */


/* Exported functions --------------------------------------------------------*/
/** @defgroup CALENDAR_Exported_Functions CALENDAR Exported Functions
  * @{
  */

/**
 * @fn int32_t CALENDAR_Initialize(void *res, CSK_CALENDAR_SignalEvent_t cb_event, void* workspace)
 * @brief Initializes the Calendar/RTC driver
 * @param[in] res Device resource handle
 * @param[in] cb_event Event callback function pointer
 * @param[in] workspace User-defined context data pointer
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_Initialize(void *res, CSK_CALENDAR_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t CALENDAR_Uninitialize(void *res)
 * @brief Deinitializes the Calendar/RTC driver
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_Uninitialize(void *res);

/**
 * @fn int32_t CALENDAR_PowerControl(void* res, CSK_POWER_STATE state)
 * @brief Controls power state of the Calendar/RTC peripheral
 * @param[in] res Device resource handle
 * @param[in] state Target power state
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn int32_t CALENDAR_Control(void* res, uint32_t control, uint32_t arg)
 * @brief Configures Calendar/RTC operational parameters
 * @param[in] res Device resource handle
 * @param[in] control Control command flags
 * @param[in] arg Command argument (value depends on control type)
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_Control(void* res, uint32_t control, uint32_t arg);

/**
 * @fn int32_t CALENDAR_SetTime(void* res, CSK_CALENDAR_TIME* stime)
 * @brief Sets the current date/time
 * @param[in] res Device resource handle
 * @param[in] stime Pointer to complete date/time structure
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_SetTime(void* res, CSK_CALENDAR_TIME* stime);

/**
 * @fn int32_t CALENDAR_GetTime(void* res, CSK_CALENDAR_TIME* stime)
 * @brief Retrieves the current date/time
 * @param[in] res Device resource handle
 * @param[out] stime Pointer to store current date/time
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_GetTime(void* res, CSK_CALENDAR_TIME* stime);

/**
 * @fn int32_t CALENDAR_SetAlarm(void* res, CSK_CALENDAR_ALARM* salarm)
 * @brief Sets the alarm trigger condition
 * @param[in] res Device resource handle
 * @param[in] salarm Pointer to alarm condition structure
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_SetAlarm(void* res, CSK_CALENDAR_ALARM* salarm);

/**
 * @fn int32_t CALENDAR_GetAlarm(void* res, CSK_CALENDAR_ALARM* salarm)
 * @brief Retrieves the current alarm settings
 * @param[in] res Device resource handle
 * @param[out] salarm Pointer to store alarm settings
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t CALENDAR_GetAlarm(void* res, CSK_CALENDAR_ALARM* salarm);

/**
 * @fn void* CALENDAR(void)
 * @brief Gets default Calendar/RTC resource handle
 * @return Pointer to default resource structure
 */
void* CALENDAR(void);

/**
  * @}
  */ /* End of group CALENDAR_Exported_Functions */

/** @} */ /* End of CALENDAR group */

#endif /* DRIVER_CALENDAR_H_ */
