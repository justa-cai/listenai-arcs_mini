/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA RTC 基础时间示例
 *
 * 本示例演示如何使用 LISA RTC 驱动进行时间读写：
 * 1. 初始化RTC设备
 * 2. 设置当前时间
 * 3. 循环读取并显示时间
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

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA RTC time example ===");

    /* 获取 RTC 设备 */
    lisa_device_t *rtc_dev = lisa_device_get(RTC_DEVICE);
    if (!lisa_device_ready(rtc_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", RTC_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", RTC_DEVICE);

    /* 设置初始时间: 2025-01-15 Wednesday 12:30:00 */
    lisa_rtc_time_t time = {
        .year = 25,     /* 2025 年 (仅存储后两位) */
        .month = 1,     /* 1 月 */
        .day = 15,      /* 15 日 */
        .weekday = LISA_RTC_WEEKDAY_WEDNESDAY,
        .hour = 12,
        .minute = 30,
        .second = 0
    };

    int ret = lisa_rtc_set_time(rtc_dev, &time);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: RTC set time failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "RTC time set: 20%02d-%02d-%02d %02d:%02d:%02d",
              time.year, time.month, time.day, time.hour, time.minute, time.second);

    /* 循环读取时间 */
    LISA_LOGI(LOG_TAG, "Start reading RTC time...");

    while (1) {
        lisa_rtc_time_t current_time;
        ret = lisa_rtc_get_time(rtc_dev, &current_time);

        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "Current time: 20%02d-%02d-%02d %02d:%02d:%02d",
                      current_time.year, current_time.month, current_time.day,
                      current_time.hour, current_time.minute, current_time.second);
        } else {
            LISA_LOGE(LOG_TAG, "Error: RTC get time failed (code: %d)", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

