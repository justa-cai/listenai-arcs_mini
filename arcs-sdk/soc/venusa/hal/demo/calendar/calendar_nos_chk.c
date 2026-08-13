#include "log_print.h"
#include "Driver_CALENDAR.h"
#include "PowerManager.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 1000000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void CALENDAR_Sec_Int_Test();
static void CALENDAR_Min_Int_Test();
static void CALENDAR_Hour_Int_Test();
static void CALENDAR_Alarm_Int_Test();
static void CALENDAR_Alarm_Calibration_WakeUp_Test();

static function test_function_array[] = {
    // CALENDAR_Sec_Int_Test,
    // CALENDAR_Min_Int_Test,
    // CALENDAR_Hour_Int_Test,
    // CALENDAR_Alarm_Int_Test,
};

static void* CALENDAR_Handler = NULL;

static void CALENDAR_Init_Handler(){
    CALENDAR_Handler = CALENDAR();
}

static uint32_t time_2_int(CSK_CALENDAR_TIME stime){
	uint32_t totalSeconds = 0;
    totalSeconds += stime.year * 31536000;
    totalSeconds += stime.month * 2592000;
    totalSeconds += stime.day * 86400;
    totalSeconds += stime.hour * 3600;
    totalSeconds += stime.min * 60;
    totalSeconds += stime.sec;

    return totalSeconds;
}

static void int_2_time(uint32_t itime, CSK_CALENDAR_ALARM* stime){
    uint32_t total_sec = itime;

    stime->year = total_sec / 31536000;
    total_sec %= 31536000;
    stime->month = total_sec / 2592000;
    total_sec %= 2592000;
    stime->day = total_sec / 86400;
    total_sec %= 86400;
    stime->hour = total_sec / 3600;
    total_sec %= 3600;
    stime->min = total_sec / 60;
    stime->sec = total_sec % 60;
}

static void CALENDAR_EventCallback(uint32_t event, void* workspace){
    CLOGD("Trigger: %d", event);

    CSK_CALENDAR_TIME stime;
    CALENDAR_GetTime(CALENDAR_Handler, &stime);
    CLOGD("Years: %d; Month: %d; Day: %d; Week: %d; Hour: %d; Min: %d; Sec: %d",\
            stime.year, stime.month, stime.weekend, stime.day, stime.hour,\
            stime.min, stime.sec);
}

static void CALENDAR_Sec_Int_Test(){
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 0};

    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);

    CALENDAR_SetTime(CALENDAR_Handler, &stime);
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_SEC_INT, 1);
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);

    FAKE_WHILE();

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_SEC_INT, 0);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(CALENDAR_Handler);
}

static void CALENDAR_Min_Int_Test(){
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 0, 59};

    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);
    CALENDAR_SetTime(CALENDAR_Handler, &stime);
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_MIN_INT, 1);

    FAKE_WHILE();

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_MIN_INT, 0);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(CALENDAR_Handler);
}

static void CALENDAR_Hour_Int_Test(){
    CSK_CALENDAR_TIME stime = {0, 0, 0, 0, 0, 59, 59};

    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);
    CALENDAR_SetTime(CALENDAR_Handler, &stime);
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_HOUR_INT, 1);

    FAKE_WHILE();

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_HOUR_INT, 0);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(CALENDAR_Handler);
}

static void CALENDAR_Alarm_Int_Test(){
    CSK_CALENDAR_TIME stime = {1, 12, 7, 31, 23, 59, 59};
    CSK_CALENDAR_ALARM satime = {2, 1, 1, 0, 0, 5};

    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);
    CALENDAR_SetTime(CALENDAR_Handler, &stime);
    CALENDAR_SetAlarm(CALENDAR_Handler, &satime);
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

    FAKE_WHILE();

    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 0);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(CALENDAR_Handler);
}

int main(){
    uint32_t times;
    logInit(0, 115200);
    CALENDAR_Init_Handler();

    CLOGD("Calendar validation");

    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}

