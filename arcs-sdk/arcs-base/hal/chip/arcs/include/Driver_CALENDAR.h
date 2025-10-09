#ifndef __DRIVER_CALENDAR_H
#define __DRIVER_CALENDAR_H

#include "arcs_ap.h"
#include "Driver_Common.h"

#define CSK_CALENDAR_CTRL_ALARM_EN        (1UL << 1)
#define CSK_CALENDAR_CTRL_CALIBRATION_EN  (1UL << 2)
#define CSK_CALENDAR_CTRL_HOUR_INT        (1UL << 4)
#define CSK_CALENDAR_CTRL_MIN_INT         (1UL << 5)
#define CSK_CALENDAR_CTRL_SEC_INT         (1UL << 6)

// CALENDAR event ID
#define CSK_CALENDAR_EVENT_ALARM_INT     (1UL << 0)
#define CSK_CALENDAR_EVENT_HOUR_INT      (1UL << 2)
#define CSK_CALENDAR_EVENT_MIN_INT       (1UL << 3)
#define CSK_CALENDAR_EVENT_SEC_INT       (1UL << 4)

// callback function
typedef void
(*CSK_CALENDAR_SignalEvent_t)(uint32_t event, void* workspace);

typedef struct _CSK_CALENDAR_TIME
{
    uint32_t year;
    uint32_t month;
    uint32_t weekend;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
} CSK_CALENDAR_TIME;

typedef struct _CSK_CALENDAR_ALARM
{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
} CSK_CALENDAR_ALARM;

int32_t CALENDAR_Initialize(void *res, CSK_CALENDAR_SignalEvent_t cb_event, void* workspace);

int32_t CALENDAR_Uninitialize(void *res);

int32_t CALENDAR_PowerControl(void* res, CSK_POWER_STATE state);

int32_t CALENDAR_Control(void* res, uint32_t control, uint32_t arg);

int32_t CALENDAR_SetTime(void* res, CSK_CALENDAR_TIME* stime);

int32_t CALENDAR_GetTime(void* res, CSK_CALENDAR_TIME* stime);

int32_t CALENDAR_SetAlarm(void* res, CSK_CALENDAR_ALARM* salarm);

int32_t CALENDAR_GetAlarm(void* res, CSK_CALENDAR_ALARM* salarm);

void* CALENDAR(void);

#endif
