/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA WDT 基础示例 - 复位模式
 *
 * 本示例演示如何使用 LISA WDT 驱动的基本功能：
 * 1. 配置看门狗为复位模式
 * 2. 启动看门狗
 * 3. 在主循环中定期喂狗
 * 4. 演示看门狗的基本工作流程
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include "lisa_device.h"
#include "lisa_wdt.h"

#include "FreeRTOS.h"
#include "task.h"

#define WDT_DEVICE      "wdt0"
#define FEED_INTERVAL_MS 300  /* 每300ms喂一次狗 */

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA WDT basic example ===");

    /* 获取 WDT 设备 */
    lisa_device_t *wdt_dev = lisa_device_get(WDT_DEVICE);
    if (!lisa_device_ready(wdt_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", WDT_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", WDT_DEVICE);

    /* 配置看门狗：中断超时 300ms，复位超时 200ms（总复位时间 500ms） */
    lisa_wdt_config_t config = {
        .int_timeout_ms = 300,  /* 中断超时时间 300ms */
        .rst_timeout_ms = 200,  /* 复位超时时间 200ms（从中断超时后开始计算） */
    };
    int ret = lisa_wdt_setup(wdt_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: WDT setup failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "WDT configured: int_timeout=%u ms, rst_timeout=%u ms (total=%u ms)",
              config.int_timeout_ms, config.rst_timeout_ms, 
              config.int_timeout_ms + config.rst_timeout_ms);

    /* 启动看门狗 */
    ret = lisa_wdt_start(wdt_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: WDT start failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "WDT started");

    /* 查询状态 */
    lisa_wdt_state_t state;
    lisa_wdt_get_state(wdt_dev, &state);
    LISA_LOGI(LOG_TAG, "WDT state: %s", 
              state == LISA_WDT_STATE_RUNNING ? "RUNNING" : 
              state == LISA_WDT_STATE_IDLE ? "IDLE" : "EXPIRED");

    LISA_LOGI(LOG_TAG, "Feeding WDT every %u ms", FEED_INTERVAL_MS);

    while (1) {
        /* 喂狗 */
        ret = lisa_wdt_feed(wdt_dev);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "WDT feed failed (code: %d)", ret);
        }

        /* 延迟后再次喂狗 */
        vTaskDelay(pdMS_TO_TICKS(FEED_INTERVAL_MS));
    }

    return 0;
}

