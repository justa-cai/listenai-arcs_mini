/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA RTC 闹钟示例
 *
 * 本示例演示如何使用 LISA RTC 驱动的闹钟功能：
 * 1. 初始化RTC设备
 * 2. 设置当前时间
 * 3. 配置闹钟在10秒后触发
 * 4. 等待闹钟事件
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_rtc.h"

#include "FreeRTOS.h"
#include "task.h"

#define RTC_DEVICE "rtc0"
#define ALARM_DELAY_SECONDS 3

static volatile bool alarm_triggered = false;

/**
 * @brief RTC 事件回调函数
 */
static void rtc_event_callback(uint32_t event, void *user_data)
{
    if (event & LISA_RTC_EVENT_ALARM) {
        LISA_LOGI(LOG_TAG, "Alarm triggered!");
        alarm_triggered = true;
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA RTC alarm example ===");

    /* 获取 RTC 设备 */
    lisa_device_t *rtc_dev = lisa_device_get(RTC_DEVICE);
    if (!lisa_device_ready(rtc_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", RTC_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", RTC_DEVICE);

    /* 注册事件回调 */
    int ret = lisa_rtc_set_callback(rtc_dev, rtc_event_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: Failed to set callback (code: %d)", ret);
        return -1;
    }

    /* 设置当前时间: 2025-01-15 Wednesday 12:00:00 */
    lisa_rtc_time_t time = {
        .year = 25,     /* 2025 年 */
        .month = 1,     /* 1 月 */
        .day = 15,      /* 15 日 */
        .weekday = LISA_RTC_WEEKDAY_WEDNESDAY,
        .hour = 12,
        .minute = 0,
        .second = 0
    };

    ret = lisa_rtc_set_time(rtc_dev, &time);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: RTC set time failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "RTC time set: 20%02d-%02d-%02d %02d:%02d:%02d",
              time.year, time.month, time.day, time.hour, time.minute, time.second);

    /* 读取当前时间, 计算几秒后的闹钟触发时间 */
    lisa_rtc_time_t current;
    ret = lisa_rtc_get_time(rtc_dev, &current);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: RTC get time failed (code: %d)", ret);
        return -1;
    }

    lisa_rtc_alarm_t alarm = {
        .year = current.year,
        .month = current.month,
        .day = current.day,
        .weekday = LISA_RTC_ALARM_IGNORE_WEEKDAY,
        .hour = current.hour,
        .minute = current.minute,
        .second = current.second
    };

    alarm.second += ALARM_DELAY_SECONDS;
    if (alarm.second >= 60) {
        alarm.second -= 60;
        alarm.minute += 1;
        if (alarm.minute >= 60) {
            alarm.minute = 0;
            alarm.hour += 1;
            if (alarm.hour >= 24) {
                alarm.hour = 0;
                alarm.day += 1; /* 简化处理: 示例不考虑月底及闰年 */
            }
        }
    }

    ret = lisa_rtc_set_alarm(rtc_dev, 0, &alarm);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: Failed to set alarm (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Alarm set after %d s -> %02d:%02d:%02d",
              ALARM_DELAY_SECONDS, alarm.hour, alarm.minute, alarm.second);

    /* 启用闹钟 */
    ret = lisa_rtc_enable_alarm(rtc_dev, 0, true);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: Failed to enable alarm (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Alarm enabled, waiting for trigger...");

    /* 等待闹钟触发 */
    while (!alarm_triggered) {
        lisa_rtc_time_t current_time;
        lisa_rtc_get_time(rtc_dev, &current_time);
        LISA_LOGI(LOG_TAG, "Current time: 20%02d-%02d-%02d %02d:%02d:%02d",
                  current_time.year, current_time.month, current_time.day,
                  current_time.hour, current_time.minute, current_time.second);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    LISA_LOGI(LOG_TAG, "Alarm test completed!");

    /* 禁用闹钟 */
    lisa_rtc_enable_alarm(rtc_dev, 0, false);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}

