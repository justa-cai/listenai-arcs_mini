/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA UART 同步接收示例（DMA 模式 + 循环缓冲区）
 *
 * 演示在 DMA 模式下使用 UART 同步接收接口，配合循环缓冲区和空闲中断。
 *
 * 新特性：
 * 1. 使用 Ping-Pong 循环缓冲区，支持定长和不定长数据接收
 * 2. 启用空闲中断，自动检测不定长数据包结束
 * 3. 应用层调用 rx_enable 启动接收，read_sync 阻塞读取
 * 4. 支持三种返回条件：收满指定长度、空闲中断、缓冲区溢出
 *
 * DMA 模式 CPU 占用更低，适合高速率、大数据量场景。
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

#define UART_DEVICE       "uart1"
#define APP_RX_BUF_SIZE   16   /* 应用层读取缓冲区大小 */

/* 循环缓冲区配置 */
#define CIRC_BUF_COUNT    2     /* Ping-Pong 缓冲区数量 */
#define CIRC_BUF_SIZE     APP_RX_BUF_SIZE   /* 每个循环缓冲区大小 */

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

static uint8_t rx_buffer[APP_RX_BUF_SIZE];

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA UART Recv Sync (DMA Mode + Circular Buffer) ===");

    /* 1. 获取 UART 设备 */
    lisa_device_t *uart_dev = lisa_device_get(UART_DEVICE);
    if (!lisa_device_ready(uart_dev)) {
        LISA_LOGE(LOG_TAG, "Error: UART device not ready");
        return -1;
    }

    /* 2. 配置 UART (115200, 8N1, DMA 模式, 循环缓冲区) */
    lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
    /* 配置循环缓冲区 */
    config.rx_buf_config.buffer_count = CIRC_BUF_COUNT;
    config.rx_buf_config.buffer_size = CIRC_BUF_SIZE;

    if (lisa_uart_configure(uart_dev, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Error: UART configure failed");
        return -1;
    }

    /* 3. 使能接收（启动循环缓冲区自动接收） */
    if (lisa_uart_rx_enable(uart_dev) != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to enable RX");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "UART configured:");
    LISA_LOGI(LOG_TAG, "  - DMA mode with idle interrupt");
    LISA_LOGI(LOG_TAG, "  - %d circular buffers x %d bytes", CIRC_BUF_COUNT, CIRC_BUF_SIZE);
    LISA_LOGI(LOG_TAG, "  - Auto-detect variable length packets");
    LISA_LOGI(LOG_TAG, "Please send data via serial port (115200, 8N1)");

    /* 4. 循环同步接收数据 */
    while (1) {
        /* 清空接收缓冲区 */
        memset(rx_buffer, 0, sizeof(rx_buffer));

        /*
         * 同步接收数据（阻塞等待，无超时）
         *
         * 返回条件：
         * 1. 收到指定长度数据 (APP_RX_BUF_SIZE)
         * 2. 检测到空闲中断（不定长数据包结束）
         * 3. 缓冲区溢出（驱动停止接收）
         *
         * 底层使用循环缓冲区 + DMA + 空闲中断自动接收
         */
        int ret = lisa_uart_read_sync(uart_dev, rx_buffer, sizeof(rx_buffer));

        if (ret > 0) {
            /* 成功接收到数据 */
            LISA_LOGI(LOG_TAG, "Received %d bytes:", ret);
            for (int i = 0; i < ret; i++) {
                printf("%02X ", (unsigned char)rx_buffer[i]);
            }
            printf(" (\"");
            for (int i = 0; i < ret; i++) {
                if (rx_buffer[i] >= 32 && rx_buffer[i] <= 126) {
                    printf("%c", rx_buffer[i]);
                } else {
                    printf(".");
                }
            }
            printf("\")\n");
        } else if (ret == LISA_DEVICE_ERR_OVERFLOW) {
            /* 缓冲区溢出：应用层读取太慢，驱动已停止接收 */
            LISA_LOGE(LOG_TAG, "ERROR: Buffer overflow! Reception stopped.");
            LISA_LOGE(LOG_TAG, "Please call rx_disable + rx_enable to recover.");

            /* 恢复接收 */
            lisa_uart_rx_disable(uart_dev);
            vTaskDelay(pdMS_TO_TICKS(100));
            if (lisa_uart_rx_enable(uart_dev) == 0) {
                LISA_LOGI(LOG_TAG, "Reception restarted.");
            } else {
                LISA_LOGE(LOG_TAG, "Failed to restart reception!");
                break;
            }
        } else if (ret == LISA_DEVICE_ERR_NOT_READY) {
            /* 接收未使能 */
            LISA_LOGE(LOG_TAG, "ERROR: RX not enabled");
            break;
        } else {
            /* 其他错误 */
            LISA_LOGE(LOG_TAG, "Error: Failed to read (ret=%d)", ret);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /* 5. 清理（通常不会执行到这里） */
    lisa_uart_rx_disable(uart_dev);
    return 0;
}
