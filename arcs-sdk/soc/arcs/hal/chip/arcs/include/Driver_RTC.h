/*
 * Project:      RTC (Real-Time Clock) Driver definitions
 */

#ifndef __DRIVER_RTC_H
#define __DRIVER_RTC_H

#include "Driver_Common.h"

// API version
#define CSK_RTC_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,01)

#define CSK_RTC_CTRL_EN              (1UL << 0)
#define CSK_RTC_CTRL_ALARM_WAKEUP    (1UL << 1)
#define CSK_RTC_CTRL_ALARM_INT       (1UL << 2)
#define CSK_RTC_CTRL_DAY_INT         (1UL << 3)
#define CSK_RTC_CTRL_HOUR_INT        (1UL << 4)
#define CSK_RTC_CTRL_MIN_INT         (1UL << 5)
#define CSK_RTC_CTRL_SEC_INT         (1UL << 6)
#define CSK_RTC_CTRL_HSEC_INT        (1UL << 7)
#define CSK_RTC_CLOCK_SRC            (1UL << 8)   // 0:digital 32k/   1:RCO 32k

// RTC event ID
#define CSK_RTC_EVENT_ALARM_INT     (1UL << 0)
#define CSK_RTC_EVENT_DAY_INT       (1UL << 1)
#define CSK_RTC_EVENT_HOUR_INT      (1UL << 2)
#define CSK_RTC_EVENT_MIN_INT       (1UL << 3)
#define CSK_RTC_EVENT_SEC_INT       (1UL << 4)
#define CSK_RTC_EVENT_HSEC_INT      (1UL << 5)

// callback function
typedef void
(*CSK_RTC_SignalEvent_t)(uint32_t event, void* workspace);

// RTC Driver Time Format.
typedef struct _CSK_RTC_TIME
{
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
} CSK_RTC_TIME;

// RTC Driver Date Format.
typedef struct _CSK_RTC_ALARM
{
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
} CSK_RTC_ALARM;

CSK_DRIVER_VERSION RTC_GetVersion(void);

int32_t RTC_Initialize(void *res, CSK_RTC_SignalEvent_t cb_event, void* workspace);

int32_t RTC_Uninitialize(void *res);

int32_t RTC_PowerControl(void* res, CSK_POWER_STATE state);

int32_t RTC_Control(void* res, uint32_t control, uint32_t arg);

int32_t RTC_SetTime(void* res, CSK_RTC_TIME* stime);

int32_t RTC_GetTime(void* res, CSK_RTC_TIME* stime);

int32_t RTC_SetAlarm(void* res, CSK_RTC_ALARM* salarm);

int32_t RTC_GetAlarm(void* res, CSK_RTC_ALARM* salarm);

void* RTC(void);

#endif /* __DRIVER_RTC_H */
