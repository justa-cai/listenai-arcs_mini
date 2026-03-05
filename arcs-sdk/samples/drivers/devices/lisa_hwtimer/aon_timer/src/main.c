/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA AON HWTIMER 基础示例 - 周期定时器
 *
 * 本示例演示如何使用 AON Timer 实现周期定时功能。
 * AON Timer 支持两种时钟源：
 * - RC32K: 内部RC振荡器，频率动态获取（典型值约32768Hz，有偏差）
 * - XO32K: 外部32K晶振，固定32768Hz，精度高
 *
 * 示例通过 get_capabilities 查询实际频率，然后启动 1 秒周期定时器。
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_hwtimer.h"

#include "FreeRTOS.h"
#include "task.h"

#define HWTIMER_DEVICE "aon_timer"
#define TIMER_CHANNEL  0

static volatile uint32_t timer_trigger_count = 0;

/* 定时器超时回调函数 */
static void timer_callback(void *user_data)
{
    timer_trigger_count++;
    LISA_LOGI(LOG_TAG, "Timer triggered: %d", timer_trigger_count);
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA AON HWTIMER basic example ===");

    /* 获取定时器设备 */
    lisa_device_t *hwtimer_dev = lisa_device_get(HWTIMER_DEVICE);
    if (!lisa_device_ready(hwtimer_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", HWTIMER_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", HWTIMER_DEVICE);

    /* 获取定时器能力 */
    lisa_hwtimer_capabilities_t caps;
    int ret = lisa_hwtimer_get_capabilities(hwtimer_dev, &caps);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: get capabilities failed");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Timer capabilities: channels=%d, freq range=%d-%d Hz",
              caps.channel_count, caps.min_freq_hz, caps.max_freq_hz);

    /* 设置定时器频率 */
    ret = lisa_hwtimer_set_frequency(hwtimer_dev, TIMER_CHANNEL, caps.max_freq_hz);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: set frequency failed");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Set timer frequency: %d Hz", caps.max_freq_hz);

    /* 设置定时器回调 */
    ret = lisa_hwtimer_set_callback(hwtimer_dev, TIMER_CHANNEL, timer_callback, NULL);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: set callback failed");
        return -1;
    }

    /* 启动周期定时器 */
    uint32_t count = caps.max_freq_hz;
    ret = lisa_hwtimer_start(hwtimer_dev, TIMER_CHANNEL, count, LISA_HWTIMER_MODE_PERIODIC);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: start timer failed");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Timer started with count=%d (period=%.1fms)",
              count, (float)count / caps.max_freq_hz * 1000);

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
