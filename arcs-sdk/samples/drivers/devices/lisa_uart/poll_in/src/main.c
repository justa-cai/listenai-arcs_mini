/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA UART 轮询接收示例
 *
 * 演示如何使用 lisa_uart_poll_in() 轮询接收单字节数据
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
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

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA UART poll_in example ===");

    /* 1. 获取 UART 设备 */
    lisa_device_t *uart_dev = lisa_device_get(UART_DEVICE);
    if (!lisa_device_ready(uart_dev)) {
        LISA_LOGE(LOG_TAG, "Error: UART device not ready");
        return -1;
    }

    /* 2. 配置 UART (115200, 8N1) */
    lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
    if (lisa_uart_configure(uart_dev, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Error: UART configure failed");
        return -1;
    }

    /* 3. 使能接收 */
    if (lisa_uart_rx_enable(uart_dev) != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to enable RX");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "UART ready, waiting for data...");
    LISA_LOGI(LOG_TAG, "Please send data via serial tool.");

    /* 4. 轮询接收数据 */
    while (1) {
        uint8_t byte;
        int ret = lisa_uart_poll_in(uart_dev, &byte);

        if (ret == 0) {
            /* 成功接收到数据 */
            /* 如果是可打印字符，显示字符 */
            if (byte >= 0x20 && byte <= 0x7E) {
                LISA_LOGI(LOG_TAG, "Received: 0x%02X ('%c')", byte, byte);
            } else {
                LISA_LOGI(LOG_TAG, "Received: 0x%02X", byte);
            }
        }

        /* 短暂延时，避免CPU占用过高 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return 0;
}
