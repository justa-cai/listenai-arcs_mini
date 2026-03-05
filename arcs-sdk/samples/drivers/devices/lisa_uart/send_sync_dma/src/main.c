/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA UART DMA 模式同步发送示例
 *
 * 演示如何在 DMA 模式下使用 LISA UART 驱动的同步发送接口发送数据
 * - 使用 DMA 传输，CPU 占用更低，适合高速率、大数据量场景
 * - 同步发送接口内部通过异步发送+信号量实现，避免轮询
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_uart.h"
#include "IOMuxManager.h"
#include "FreeRTOS.h"
#include "task.h"

#define UART_DEVICE    "uart1"

/* UART1 引脚: PB2=TX, PB3=RX */
#define UART1_TX_PAD   CSK_IOMUX_PAD_B
#define UART1_TX_PIN   2
#define UART1_RX_PAD   CSK_IOMUX_PAD_B
#define UART1_RX_PIN   3
#define UART1_FUNC     CSK_IOMUX_FUNC_ALTER3

/*
    为满足不同板型示例场景，重定向uart设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(UART1_TX_PAD, UART1_TX_PIN, UART1_FUNC);
    IOMuxManager_PinConfigure(UART1_RX_PAD, UART1_RX_PIN, UART1_FUNC);
}
#endif

char msg[64];

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA UART send sync (DMA mode) example ===");

    /* 1. 获取 UART 设备 */
    lisa_device_t *uart_dev = lisa_device_get(UART_DEVICE);
    if (!lisa_device_ready(uart_dev)) {
        LISA_LOGE(LOG_TAG, "Error: UART device not ready");
        return -1;
    }

    /* 2. 配置 UART (115200, 8N1, DMA 模式) */
    lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
    if (lisa_uart_configure(uart_dev, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Error: UART configure failed");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "UART configured in DMA mode, start sending...");

    /* 3. 循环发送数据 */
    uint32_t counter = 0;
    while (1) {
        snprintf(msg, sizeof(msg), "Hello UART (sync, DMA)! Counter: %lu\r\n", counter++);

        /* 同步发送数据，超时时间 100ms */
        int ret = lisa_uart_write_sync(uart_dev, (uint8_t *)msg, strlen(msg), 100);
        if (ret > 0) {
            LISA_LOGI(LOG_TAG, "Sent: %s", msg);
        } else if (ret == LISA_DEVICE_ERR_TIMEOUT) {
            LISA_LOGW(LOG_TAG, "Warning: TX timeout");
        } else {
            LISA_LOGE(LOG_TAG, "Error: Send failed (code: %d)", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
