/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA UART 轮询发送示例
 *
 * 演示如何使用 lisa_uart_poll_out() 轮询发送单字节数据
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include "lisa_device.h"
#include "lisa_uart.h"
#include "IOMuxManager.h"
#include "pinmux.h"
#include "FreeRTOS.h"
#include "task.h"

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

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA UART poll_out example ===");

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

    LISA_LOGI(LOG_TAG, "UART ready, start polling send...");

    /* 3. 轮询发送数据 */
    const char *msg = "Hello UART!\r\n";
    uint32_t counter = 0;

    while (1) {
        LISA_LOGI(LOG_TAG, "Sending message %lu", counter++);

        /* 逐字节轮询发送 */
        for (int i = 0; msg[i] != '\0'; i++) {
            lisa_uart_poll_out(uart_dev, msg[i]);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
