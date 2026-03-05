/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA SPI 主设备示例
 *
 * 演示如何使用 LISA SPI 驱动进行主设备通信
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_spi.h"
#include "IOMuxManager.h"
#include <lisa_semaphore.h>
#include "FreeRTOS.h"
#include "task.h"

#define SPI_DEVICE    "spi0"

#define SPI_CLK_PAD        CSK_IOMUX_PAD_A
#define SPI_CLK_PIN        15
#define SPI_CLK_FUNC       CSK_IOMUX_FUNC_ALTER5

#define SPI_MOSI_PAD      CSK_IOMUX_PAD_A
#define SPI_MOSI_PIN      14
#define SPI_MOSI_FUNC     CSK_IOMUX_FUNC_ALTER5

#define SPI_MISO_PAD      CSK_IOMUX_PAD_A
#define SPI_MISO_PIN      13
#define SPI_MISO_FUNC     CSK_IOMUX_FUNC_ALTER5
#define SPI_CS_PAD        CSK_IOMUX_PAD_A
#define SPI_CS_PIN        12
#define SPI_CS_FUNC       CSK_IOMUX_FUNC_ALTER5

static lisa_semaphore_t *tx_complete_sem = NULL;

/*
    为满足不同板型示例场景，重定向SPI设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_spi0_pinmux()
{
    IOMuxManager_PinConfigure(SPI_CLK_PAD, SPI_CLK_PIN, SPI_CLK_FUNC);
    IOMuxManager_PinConfigure(SPI_MOSI_PAD, SPI_MOSI_PIN, SPI_MOSI_FUNC);
    IOMuxManager_PinConfigure(SPI_MISO_PAD, SPI_MISO_PIN, SPI_MISO_FUNC);
    IOMuxManager_PinConfigure(SPI_CS_PAD, SPI_CS_PIN, SPI_CS_FUNC);
}
#endif

static void spi_transfer_callback(void *user_data)
{
    LISA_LOGI(LOG_TAG, "SPI transfer completed");
    lisa_semaphore_give(tx_complete_sem);
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA SPI master example ===");
    int ret;

    /* 获取 SPI 设备 */
    lisa_device_t *spi_dev = lisa_device_get(SPI_DEVICE);
    if (!lisa_device_ready(spi_dev)) {
        LISA_LOGE(LOG_TAG, "Failed to get SPI device: %s", SPI_DEVICE);
        return -1;
    }

    /* 创建信号量 */
    tx_complete_sem = lisa_semaphore_create(1);
    if (!tx_complete_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create semaphore");
        return -1;
    }

    /* 配置 SPI 参数 (使用中断模式) */
    lisa_spi_config_t spi_config = LISA_SPI_CONFIG_DEFAULT();
    if (lisa_spi_configure(spi_dev, &spi_config) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to configure SPI device");
        return -1;
    }

    // 注册回调函数
    ret = lisa_spi_register_callback(spi_dev, spi_transfer_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to register SPI callback");
        return -1;
    }

    // SPI 仅发送演示
    uint8_t tx_buf[] = {0x11, 0x22, 0x33, 0x44};

    ret = lisa_spi_write(spi_dev, tx_buf, 4);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "SPI transfer failed");
        return -1;
    }
    ret = lisa_semaphore_take(tx_complete_sem, pdMS_TO_TICKS(100));
    if (ret != LISA_OK) {
        LISA_LOGE(LOG_TAG, "SPI transfer timeout");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "SPI transfer success, sent: %02X %02X %02X %02X", tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3]);

    return 0;
}
