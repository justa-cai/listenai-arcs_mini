#include "set_calendar.h"

void *calendar_handler = NULL;
static volatile uint32_t calendar_flag = 0;
static CSK_CALENDAR_TIME last_stime;

static void calendar_eventcallback(uint32_t event, void *workspace)
{
    calendar_flag = 1;
    CALENDAR_GetTime(calendar_handler, &last_stime);
}

bool set_calendar_sec_value()
{
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 0};

    CALENDAR_Initialize(calendar_handler, calendar_eventcallback, NULL);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_FULL);

    CALENDAR_SetTime(calendar_handler, &stime);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_SEC_INT, 1);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);

    while (calendar_flag == 0);
    calendar_flag = 0;

    bool set_sec_status = false;
    if (last_stime.sec == 1) {
        set_sec_status = true;
    } else {
        set_sec_status = false;
    }

    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_SEC_INT, 0);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(calendar_handler);

    return set_sec_status;
}

bool set_calendar_min_value()
{
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 59};

    CALENDAR_Initialize(calendar_handler, calendar_eventcallback, NULL);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_FULL);
    CALENDAR_SetTime(calendar_handler, &stime);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_MIN_INT, 1);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);

    while (calendar_flag == 0);
    calendar_flag = 0;

    bool set_min_status = false;
    if (last_stime.sec == 0 && last_stime.min == 1) {
        set_min_status = true;
    } else {
        set_min_status = false;
    }

    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_MIN_INT, 0);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(calendar_handler);

    return set_min_status;
}

bool set_calendar_hour_value()
{
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 59, 59};

    CALENDAR_Initialize(calendar_handler, calendar_eventcallback, NULL);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_FULL);
    CALENDAR_SetTime(calendar_handler, &stime);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_HOUR_INT, 1);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);

    while (calendar_flag == 0);
    calendar_flag = 0;

    bool set_hour_status = false;
    if (last_stime.hour == 1 && last_stime.min == 0 && last_stime.sec == 0) {
        set_hour_status = true;
    } else {
        set_hour_status = false;
    }

    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_HOUR_INT, 0);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(calendar_handler);

    return set_hour_status;
}

bool set_calendar_alarm_value()
{
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 0};
    CSK_CALENDAR_ALARM satime = {0, 0, 0, 0, 0, 1};

    CALENDAR_Initialize(calendar_handler, calendar_eventcallback, NULL);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_FULL);
    CALENDAR_SetTime(calendar_handler, &stime);
    CALENDAR_SetAlarm(calendar_handler, &satime);
    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

    while (calendar_flag == 0);
    calendar_flag = 0;

    bool set_alarm_status = false;
    if (last_stime.hour == satime.hour &&
        last_stime.min == satime.min &&
        last_stime.sec == satime.sec) {
        set_alarm_status = true;
    } else {
        set_alarm_status = false;
    }

    CALENDAR_Control(calendar_handler, CSK_CALENDAR_CTRL_ALARM_EN, 0);
    CALENDAR_PowerControl(calendar_handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(calendar_handler);

    return set_alarm_status;
}
