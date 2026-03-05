#include <stdio.h>
#include <stdint.h>

#include "IOMuxManager.h"
#include "Driver_CALENDAR.h"
#include "PowerManager.h"

#include "FreeRTOS.h"
#include "task.h"

static void* CALENDAR_Handler = NULL;

static void CALENDAR_Init_Handler()
{
    CALENDAR_Handler = CALENDAR();
}

static void CALENDAR_EventCallback(uint32_t event, void* workspace)
{
    /* RTC闹钟中断 */
    if ( event &  CSK_CALENDAR_EVENT_ALARM_INT) {
        printf("RTC Alarm interrupt trigger\n");
    }

    if (event & CSK_CALENDAR_EVENT_HOUR_INT) {
        printf("RTC Hour interrupt trigger\n");
    } else if (event & CSK_CALENDAR_EVENT_MIN_INT) {
        printf("RTC Minute interrupt trigger\n");
    } else if (event & CSK_CALENDAR_EVENT_SEC_INT) {
        printf("RTC Second interrupt trigger\n");
    }

    CSK_CALENDAR_TIME stime;
    CALENDAR_GetTime(CALENDAR_Handler, &stime);
    printf("Years: %d; Month: %d; Week: %d; Day: %d; Hour: %d; Min: %d; Sec: %d\n",\
           stime.year, stime.month, stime.weekend, stime.day, stime.hour,\
           stime.min, stime.sec);
}

static void CALENDAR_Test()
{
    /* RTC的时间为1年6月6日14时48分55秒 */
    CSK_CALENDAR_TIME set_time = {
        .year = 1,      // 1年，注意：这个参数最大值为127,超过127会出错
        .month = 6,     // 6月
        .weekend = 5,   // 星期五，即6月6日是星期五
        .day = 6,      // 6日
        .hour = 14,     // 14时
        .min = 48,      // 48分
        .sec = 55,      // 55秒
    };

    /* RTC的闹钟的时间为1年6月6日14时49分55秒 */
    CSK_CALENDAR_ALARM alarm_time = {
        .year = 1,      // 1年，注意：这个参数最大值为127,超过127会出错
        .month = 6,     // 6月
        .day = 6,      // 6日
        .hour = 14,     // 14时
        .min = 49,      // 49分
        .sec = 55,      // 55秒
    };

    /* RTC的初始化，注册事件回调 */
    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);

    /* 设置RTC的时间 */
    CALENDAR_SetTime(CALENDAR_Handler, &set_time);

    /* 设置闹钟的时间 */
    CALENDAR_SetAlarm(CALENDAR_Handler, &alarm_time);
    /* 使能闹钟中断 */
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

    /* 使能分钟中断
     * 注意：秒、分钟、小时的中断，一次只能设置一个
     * 分钟、小时的中断，只会在整分或整小时的时候触发
     */
    // CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_SEC_INT, 1);
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_MIN_INT, 1);
    // CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_HOUR_INT, 1);

    /* 使能校准
     * 如果不使能校准，由于RTC使用内部低精度时钟，运行一定时间后，RTC会存在一定频偏，进而导致RTC时间存在偏移
     */
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);


    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 关闭RTC */
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(CALENDAR_Handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! RTC\n");

    CALENDAR_Init_Handler();
    CALENDAR_Test();

    return 0;
}
