/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA UART DMA模式异步发送示例
 *
 * 演示在DMA模式下使用 lisa_uart_write_async() 异步发送数据，
 * 通过事件回调接收发送完成通知
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "lisa_device.h"
#include "lisa_uart.h"
#include "IOMuxManager.h"
#include "pinmux.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define UART_DEVICE    "uart1"

#ifdef CONFIG_BOARD_ARCS_MINI
/* ARCS_MINI 板型 UART1 引脚: PA9=TX, PA8=RX */
#define UART1_TX_PAD   CSK_IOMUX_PAD_A
#define UART1_TX_PIN   9
#define UART1_RX_PAD   CSK_IOMUX_PAD_A
#define UART1_RX_PIN   8
#define UART1_FUNC     CSK_IOMUX_FUNC_ALTER3
#else // !CONFIG_BOARD_ARCS_MINI
/* UART1 引脚: PB2=TX, PB3=RX */
#define UART1_TX_PAD   CSK_IOMUX_PAD_B
#define UART1_TX_PIN   2
#define UART1_RX_PAD   CSK_IOMUX_PAD_B
#define UART1_RX_PIN   3
#define UART1_FUNC     CSK_IOMUX_FUNC_ALTER3
#endif // CONFIG_BOARD_ARCS_MINI

/*
    为满足不同板型示例场景，重定向uart设备的pinmux配置
*/
#if defined(CONFIG_BOARD_ARCS_EVB) || defined(CONFIG_BOARD_ARCS_MINI)
void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(UART1_TX_PAD, UART1_TX_PIN, UART1_FUNC);
    IOMuxManager_PinConfigure(UART1_RX_PAD, UART1_RX_PIN, UART1_FUNC);
}
#endif

/* 发送完成信号量 */
static SemaphoreHandle_t tx_sem = NULL;

char msg[64];

/**
 * @brief UART 事件回调函数
 *
 * @param event 事件类型
 * @param user_data 用户数据
 */
static void uart_event_callback(lisa_uart_event_t event, void *user_data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (event & LISA_UART_EVENT_TX_DONE) {
        /* 从中断中释放信号量 */
        xSemaphoreGiveFromISR(tx_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA UART async send (DMA mode) example ===");

    /* 1. 创建发送完成信号量 (二值信号量) */
    tx_sem = xSemaphoreCreateBinary();
    if (tx_sem == NULL) {
        LISA_LOGE(LOG_TAG, "Error: Failed to create semaphore");
        return -1;
    }

    /* 2. 获取 UART 设备 */
    lisa_device_t *uart_dev = lisa_device_get(UART_DEVICE);
    if (!lisa_device_ready(uart_dev)) {
        LISA_LOGE(LOG_TAG, "Error: UART device not ready");
        return -1;
    }

    /* 3. 配置 UART (115200, 8N1, DMA) */
    lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
    if (lisa_uart_configure(uart_dev, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Error: UART configure failed");
        return -1;
    }

    /* 4. 设置事件回调 */
    if (lisa_uart_set_callback(uart_dev, uart_event_callback, NULL) != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to set callback");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "UART ready, start sending...");

    /* 5. 循环发送数据 */
    uint32_t counter = 0;
    while (1) {
        snprintf(msg, sizeof(msg), "Hello UART! Counter: %lu\r\n", counter++);

        /* 异步发送数据 */
        int ret = lisa_uart_write_async(uart_dev, (uint8_t *)msg, strlen(msg));
        if (ret > 0) {
            /* 等待回调释放的信号量，超时时间 100ms */
            if (xSemaphoreTake(tx_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                LISA_LOGI(LOG_TAG, "Sent: %s", msg);
            } else {
                LISA_LOGW(LOG_TAG, "Warning: TX timeout");
            }
        } else {
            LISA_LOGE(LOG_TAG, "Error: Send failed");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
