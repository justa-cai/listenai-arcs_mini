/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA GPT HWTIMER 基础示例 - 周期定时器
 *
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_hwtimer.h"

#include "FreeRTOS.h"
#include "task.h"

#define HWTIMER_DEVICE "gpt_timer"
#define TIMER_CHANNEL  0
#define TIMER_FREQ_HZ  100000000  /* 100MHz */
#define TIMER_COUNT    TIMER_FREQ_HZ/2

static volatile uint32_t timer_trigger_count = 0;

/* 定时器超时回调函数 */
static void timer_callback(void *user_data)
{
    timer_trigger_count++;
    LISA_LOGI(LOG_TAG, "Timer triggered: %d", timer_trigger_count);
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA GPT HWTIMER basic example ===");

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
    ret = lisa_hwtimer_set_frequency(hwtimer_dev, TIMER_CHANNEL, TIMER_FREQ_HZ);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: set frequency failed");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Set timer frequency: %d Hz", TIMER_FREQ_HZ);

    /* 设置定时器回调 */
    ret = lisa_hwtimer_set_callback(hwtimer_dev, TIMER_CHANNEL, timer_callback, NULL);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: set callback failed");
        return -1;
    }

    /* 启动周期定时器 */
    ret = lisa_hwtimer_start(hwtimer_dev, TIMER_CHANNEL, TIMER_COUNT, LISA_HWTIMER_MODE_PERIODIC);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: start timer failed");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Timer started with count=%d (period=%.1fms)",
              TIMER_COUNT, (float)TIMER_COUNT / TIMER_FREQ_HZ * 1000);

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
