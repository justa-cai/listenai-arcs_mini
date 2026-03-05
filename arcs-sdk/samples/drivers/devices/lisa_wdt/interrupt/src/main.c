/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA WDT 中断模式示例
 *
 * 本示例演示如何使用 LISA WDT 驱动的中断模式：
 * 1. 配置看门狗为中断模式
 * 2. 设置超时回调函数
 * 3. 启动看门狗
 * 4. 在主循环中定期喂狗
 * 5. 演示超时中断的处理
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include "lisa_device.h"
#include "lisa_wdt.h"

#include "FreeRTOS.h"
#include "task.h"

#define WDT_DEVICE      "wdt0"
#define FEED_INTERVAL_MS 800  /* 每800ms喂一次狗，小于中断时间1s */
#define NORMAL_FEED_COUNT 5   /* 正常喂狗次数，之后停止喂狗以触发超时 */

static lisa_device_t *g_wdt_dev = NULL;

/**
 * @brief WDT 超时回调函数
 *
 * 此函数在中断上下文中执行，应尽量简短快速
 * 参考HAL库示例，只打印信息，不进行其他操作
 */
void wdt_timeout_callback(void *user_data)
{
    printf("WDT callback triggered - Resetting\n");
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA WDT interrupt example ===");

    /* 获取 WDT 设备 */
    g_wdt_dev = lisa_device_get(WDT_DEVICE);
    if (!lisa_device_ready(g_wdt_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", WDT_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", WDT_DEVICE);

    /* 配置看门狗：按照HAL库示例的配置
     * 中断阶段：2^15 / 32k = 1s (interrupt stage)
     * 复位阶段：2^14 / 32k = 0.5s (reset stage)
     * 中断产生后就开始进入复位阶段
     */
    lisa_wdt_config_t config = {
        .int_timeout_ms = 1000,  /* 中断超时时间 1s (对应 hal_driver_wdt_int_time_15) */
        .rst_timeout_ms = 500,   /* 复位超时时间 0.5s (对应 hal_driver_wdt_rst_time_14) */
    };
    int ret = lisa_wdt_setup(g_wdt_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: WDT setup failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "WDT configured: int_timeout=%u ms, rst_timeout=%u ms (total=%u ms)",
              config.int_timeout_ms, config.rst_timeout_ms,
              config.int_timeout_ms + config.rst_timeout_ms);

    /* 设置超时回调函数 */
    ret = lisa_wdt_set_callback(g_wdt_dev, wdt_timeout_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: WDT callback setup failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "WDT timeout callback registered");

    /* 启动看门狗 */
    ret = lisa_wdt_start(g_wdt_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: WDT start failed (code: %d)", ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "WDT started");

    /* 查询状态 */
    lisa_wdt_state_t state;
    lisa_wdt_get_state(g_wdt_dev, &state);
    LISA_LOGI(LOG_TAG, "WDT state: %s", 
              state == LISA_WDT_STATE_RUNNING ? "RUNNING" : 
              state == LISA_WDT_STATE_IDLE ? "IDLE" : "EXPIRED");

    LISA_LOGI(LOG_TAG, "Feeding WDT every %d ms for %d times, then stop to trigger timeout", 
              FEED_INTERVAL_MS, NORMAL_FEED_COUNT);

    uint32_t feed_count = 0;
    while (1) {
        if (feed_count < NORMAL_FEED_COUNT) {
            /* 前几次正常喂狗 */
            ret = lisa_wdt_feed(g_wdt_dev);
            if (ret != LISA_DEVICE_OK) {
                LISA_LOGE(LOG_TAG, "WDT feed failed (code: %d)", ret);
            } else {
                feed_count++;
                LISA_LOGI(LOG_TAG, "WDT fed (%u/%u)", feed_count, NORMAL_FEED_COUNT);
            }
        } else {
            /* 停止喂狗，等待超时中断触发 */
            if (feed_count == NORMAL_FEED_COUNT) {
                LISA_LOGW(LOG_TAG, "Stopped feeding WDT, waiting for timeout interrupt (should occur after %u ms)...",
                          config.int_timeout_ms);
                feed_count++;  // 只打印一次
            }
            /* 在主循环中等待，超时会通过中断回调处理 */
            vTaskDelay(pdMS_TO_TICKS(100));  // 短暂延时，让其他任务运行
            continue;
        }

        /* 延迟后再次喂狗 */
        vTaskDelay(pdMS_TO_TICKS(FEED_INTERVAL_MS));
    }

    return 0;
}

