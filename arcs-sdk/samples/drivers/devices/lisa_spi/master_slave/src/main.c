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

#define SPI0_DEVICE         "spi0"
#define SPI0_CLK_PAD        CSK_IOMUX_PAD_A
#define SPI0_CLK_PIN        15
#define SPI0_CLK_FUNC       CSK_IOMUX_FUNC_ALTER5

#define SPI0_MOSI_PAD       CSK_IOMUX_PAD_A
#define SPI0_MOSI_PIN       14
#define SPI0_MOSI_FUNC      CSK_IOMUX_FUNC_ALTER5

#define SPI0_MISO_PAD       CSK_IOMUX_PAD_A
#define SPI0_MISO_PIN       13
#define SPI0_MISO_FUNC      CSK_IOMUX_FUNC_ALTER5
#define SPI0_CS_PAD         CSK_IOMUX_PAD_A
#define SPI0_CS_PIN         12
#define SPI0_CS_FUNC        CSK_IOMUX_FUNC_ALTER5

#define SPI1_DEVICE         "spi1"
#define SPI1_CLK_PAD        CSK_IOMUX_PAD_A
#define SPI1_CLK_PIN        25
#define SPI1_CLK_FUNC       CSK_IOMUX_FUNC_ALTER6

#define SPI1_MOSI_PAD       CSK_IOMUX_PAD_A
#define SPI1_MOSI_PIN       24
#define SPI1_MOSI_FUNC      CSK_IOMUX_FUNC_ALTER6

#define SPI1_MISO_PAD       CSK_IOMUX_PAD_A
#define SPI1_MISO_PIN       23
#define SPI1_MISO_FUNC      CSK_IOMUX_FUNC_ALTER6
#define SPI1_CS_PAD         CSK_IOMUX_PAD_A
#define SPI1_CS_PIN         22
#define SPI1_CS_FUNC        CSK_IOMUX_FUNC_ALTER6

static lisa_semaphore_t *tx_complete_sem = NULL;
static lisa_semaphore_t *slave_transfer_complete_sem = NULL;
static lisa_device_t *spi0_dev = NULL;
static lisa_device_t *spi1_dev = NULL;

/*
    为满足不同板型示例场景，重定向SPI设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_spi0_pinmux()
{
    IOMuxManager_PinConfigure(SPI0_CLK_PAD, SPI0_CLK_PIN, SPI0_CLK_FUNC);
    IOMuxManager_PinConfigure(SPI0_MOSI_PAD, SPI0_MOSI_PIN, SPI0_MOSI_FUNC);
    IOMuxManager_PinConfigure(SPI0_MISO_PAD, SPI0_MISO_PIN, SPI0_MISO_FUNC);
    IOMuxManager_PinConfigure(SPI0_CS_PAD, SPI0_CS_PIN, SPI0_CS_FUNC);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(SPI1_CLK_PAD, SPI1_CLK_PIN, SPI1_CLK_FUNC);
    IOMuxManager_PinConfigure(SPI1_MOSI_PAD, SPI1_MOSI_PIN, SPI1_MOSI_FUNC);
    IOMuxManager_PinConfigure(SPI1_MISO_PAD, SPI1_MISO_PIN, SPI1_MISO_FUNC);
    IOMuxManager_PinConfigure(SPI1_CS_PAD, SPI1_CS_PIN, SPI1_CS_FUNC);
}
#endif

static void spi0_transfer_callback(void *user_data)
{
    LISA_LOGI(LOG_TAG, "SPI0 transfer completed");
    lisa_semaphore_give(tx_complete_sem);
}

static void spi1_transfer_callback(void *user_data)
{
    LISA_LOGI(LOG_TAG, "SPI1 transfer completed");
    lisa_semaphore_give(slave_transfer_complete_sem);
}

static int spi0_init(void)
{
    int ret;
    
    /* 获取 SPI0 设备 */
    spi0_dev = lisa_device_get(SPI0_DEVICE);
    if (!lisa_device_ready(spi0_dev)) {
        LISA_LOGE(LOG_TAG, "Failed to get SPI0 device: %s", SPI0_DEVICE);
        return -1;
    }

    /* 创建信号量 */
    tx_complete_sem = lisa_semaphore_create(1);
    if (!tx_complete_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create semaphore");
        return -1;
    }

    /* 配置 SPI0 参数 (使用DMA模式) */
    lisa_spi_config_t spi0_config = LISA_SPI_CONFIG_DEFAULT();
    spi0_config.tx_transfer_mode = LISA_SPI_DMA_TRANSFER;
    spi0_config.rx_transfer_mode = LISA_SPI_DMA_TRANSFER;
    spi0_config.tx_dma_channel = 0;
    spi0_config.rx_dma_channel = 1;
    if (lisa_spi_configure(spi0_dev, &spi0_config) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to configure SPI0 device");
        return -1;
    }

    // 注册回调函数
    ret = lisa_spi_register_callback(spi0_dev, spi0_transfer_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to register SPI0 callback");
        return -1;
    }

    return 0;
}

static int spi1_init(void)
{
    int ret;

    /* 获取 SPI1 设备 */
    spi1_dev = lisa_device_get(SPI1_DEVICE);
    if (!lisa_device_ready(spi1_dev)) {
        LISA_LOGE(LOG_TAG, "Failed to get SPI1 device: %s", SPI1_DEVICE);
        return -1;
    }

    /* 创建信号量 */
    slave_transfer_complete_sem = lisa_semaphore_create(1);
    if (!slave_transfer_complete_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create semaphore");
        return -1;
    }

    /* 配置 SPI1 参数 (从机模式，使用DMA) */
    lisa_spi_config_t spi1_config = LISA_SPI_CONFIG_DEFAULT();
    spi1_config.master_mode = false; // 从机模式
    spi1_config.tx_transfer_mode = LISA_SPI_DMA_TRANSFER;
    spi1_config.rx_transfer_mode = LISA_SPI_DMA_TRANSFER;
    spi1_config.tx_dma_channel = 2;
    spi1_config.rx_dma_channel = 3;
    if (lisa_spi_configure(spi1_dev, &spi1_config) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to configure SPI1 device");
        return -1;
    }

    // 注册回调函数
    ret = lisa_spi_register_callback(spi1_dev, spi1_transfer_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to register SPI1 callback");
        return -1;
    }

    return 0;
}

static void spi0_master_thread(void *arg)
{
    uint8_t *tx_data = lisa_mem_align_alloc(32, 10);
    uint8_t *rx_data = lisa_mem_align_alloc(32, 10);

    memcpy(tx_data, (uint8_t[]){1,2,3,4,5,6,7,8}, 8);
    lisa_spi_transfer_t transfer = {
        .tx_buf = tx_data,
        .rx_buf = rx_data,
        .len = 8,
    };

    vTaskDelay(pdMS_TO_TICKS(500));
    while (1) {
        memset(rx_data, 0, 10);
        int ret = lisa_spi_transfer(spi0_dev, &transfer);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "SPI0 transfer failed");
        }

        ret = lisa_semaphore_take(tx_complete_sem, pdMS_TO_TICKS(100));
        if (ret != LISA_OK) {
            LISA_LOGE(LOG_TAG, "SPI transfer timeout");
        }

        LISA_LOGI(LOG_TAG, "SPI0 Received: %d %d %d %d %d %d %d %d", 
            rx_data[0], rx_data[1], rx_data[2], rx_data[3], rx_data[4], 
            rx_data[5], rx_data[6], rx_data[7]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void spi1_slave_thread(void *arg)
{
    uint8_t *tx_data = lisa_mem_align_alloc(32, 10);
    uint8_t *rx_data = lisa_mem_align_alloc(32, 10);

    memcpy(tx_data, (uint8_t[]){8,7,6,5,4,3,2,1}, 8);
    lisa_spi_transfer_t transfer = {
        .tx_buf = tx_data,
        .rx_buf = rx_data,
        .len = 8,
    };

    while (1) {
        memset(rx_data, 0, 10);
        int ret = lisa_spi_transfer(spi1_dev, &transfer);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "SPI1 transfer failed");
        }

        ret = lisa_semaphore_take(slave_transfer_complete_sem, pdMS_TO_TICKS(1000));
        if (ret != LISA_OK) {
            LISA_LOGE(LOG_TAG, "SPI transfer timeout");
        }

        LISA_LOGI(LOG_TAG, "SPI1 Received: %d %d %d %d %d %d %d %d", 
            rx_data[0], rx_data[1], rx_data[2], rx_data[3], rx_data[4], 
            rx_data[5], rx_data[6], rx_data[7]);
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA SPI master-slave thread demo ===");

    if (spi0_init() != 0) {
        return -1;
    }
    if (spi1_init() != 0) {
        return -1;
    }

    xTaskCreate(spi1_slave_thread, "spi1_slave", 4096, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(spi0_master_thread, "spi0_master", 4096, NULL, configMAX_PRIORITIES - 2, NULL);

    LISA_LOGI(LOG_TAG, "SPI master-slave threads started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
