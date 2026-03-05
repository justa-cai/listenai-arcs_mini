/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA UART 同步发送示例（中断模式）
 *
 * 演示在中断模式下使用同步发送接口
 * 底层通过中断异步发送+信号量实现，避免轮询占用CPU
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

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== UART Sync Send (Interrupt Mode) ===");

    /* 1. 获取并检查设备 */
    lisa_device_t *uart_dev = lisa_device_get(UART_DEVICE);
    if (!lisa_device_ready(uart_dev)) {
        LISA_LOGE(LOG_TAG, "Device not ready");
        return -1;
    }

    /* 2. 配置 UART (115200, 8N1, INTERRUPT) */
    lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
    if (lisa_uart_configure(uart_dev, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Config failed");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "UART configured in INT mode, start sending...");

    /* 3. 循环同步发送 */
    uint32_t counter = 0;
    while (1) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Hello UART (sync, INT)! Counter: #%lu\r\n", counter++);

        /* 同步发送（中断异步发送+信号量等待），超时 100ms */
        int ret = lisa_uart_write_sync(uart_dev, (uint8_t *)msg, strlen(msg), 100);
        if (ret > 0) {
            LISA_LOGI(LOG_TAG, "Sent: %s", msg);
        } else if (ret == LISA_DEVICE_ERR_TIMEOUT) {
            LISA_LOGW(LOG_TAG, "Timeout");
        } else {
            LISA_LOGE(LOG_TAG, "Failed: %d", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
