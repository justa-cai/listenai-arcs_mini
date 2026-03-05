/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA QSPILCD DMA 传输示例
 *
 * 演示使用 QSPI LCD 驱动进行 DMA 模式的数据传输
 */

#include "IOMuxManager.h"
#include "lisa_device.h"
#include "lisa_qspilcd.h"
#include "pinmux.h"

#define LOG_TAG "qspilcd_dma"
#include "lisa_log.h"

/*
    为满足不同板型示例场景，重定向设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
#define LCD_CS_PIN 5
#define LCD_SPI_CLK_PIN 3
#define LCD_SPI_DATA_PIN 1

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_qspi_lcd_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER30);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER30);   // D0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, CSK_IOMUX_FUNC_ALTER30);                  // D1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, CSK_IOMUX_FUNC_ALTER30);                  // D2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, CSK_IOMUX_FUNC_ALTER30);                  // D3
}
#endif

/* DMA 模式需要 32 字节对齐的缓冲区 */
static uint8_t tx_buf[256] __attribute__((aligned(32)));

int main(int argc, char **argv)
{
    int ret;

    LISA_LOGI(LOG_TAG, "=== LISA QSPILCD DMA example ===");

    /* 1. 获取 QSPI LCD 设备 */
    lisa_device_t *qspi = lisa_device_get("qspilcd0");
    if (!qspi || !lisa_device_ready(qspi)) {
        LISA_LOGE(LOG_TAG, "Failed to get QSPILCD device");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "QSPILCD device ready");

    /* 2. 配置 CS 引脚（使用 GPIO 手动控制） */
    lisa_device_t *gpio = lisa_device_get("gpiob");
    if (gpio && lisa_device_ready(gpio)) {
        ret = lisa_qspilcd_cs_configure(qspi, gpio, LCD_CS_PIN);
        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "CS GPIO configured");
        }
    }

    uint32_t control = LISA_QSPILCD_TXIO_PIO | LISA_QSPILCD_CPOL0_CPHA0 | 
                        LISA_QSPILCD_MSB_LSB | LISA_QSPILCD_MODE_MASTER |
                        LISA_QSPILCD_DATA_BITS(8);
    ret = lisa_qspilcd_control(qspi, control, 50*1000*1000);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    /* 3. 准备测试数据 */
    for (size_t i = 0; i < sizeof(tx_buf); i++) {
        tx_buf[i] = (uint8_t)i;
    }

    /* ========== 示例 1: PIO 模式传输（单线） ========== */
    {
        uint8_t cmd[] = {0x2A, 0x00, 0x00, 0x00, 0xEF};  /* 示例命令 */

        lisa_qspilcd_xfer_t xfer = {
            .buf = cmd,
            .size_bytes = sizeof(cmd),
            .lane = LISA_QSPILCD_LANE_QUAD,  /* 单线模式 */
            .data_bits = 8,
            .use_dma = false,                   /* PIO 模式 */
        };

        /* 拉低 CS */
        lisa_qspilcd_cs_control(qspi, false);

        /* PIO 传输（同步，函数返回即完成） */
        ret = lisa_qspilcd_transfer(qspi, &xfer);

        /* 拉高 CS */
        lisa_qspilcd_cs_control(qspi, true);

        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "PIO transfer (Single lane, 8-bit): OK");
        } else {
            LISA_LOGE(LOG_TAG, "PIO transfer failed: %d", ret);
        }
    }

    /* ========== DMA 模式传输（四线，大数据量） ========== */
    {
        lisa_qspilcd_xfer_t xfer = {
            .buf = tx_buf,
            .size_bytes = sizeof(tx_buf),
            .lane = LISA_QSPILCD_LANE_QUAD,    /* 四线模式 */
            .data_bits = 16,
            .use_dma = true,                    /* DMA 模式 */
        };

        /* 拉低 CS */
        lisa_qspilcd_cs_control(qspi, false);

        /* 启动 DMA 传输（异步，立即返回） */
        ret = lisa_qspilcd_transfer(qspi, &xfer);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "DMA transfer start failed: %d", ret);
            lisa_qspilcd_cs_control(qspi, true);
            return -1;
        }
        LISA_LOGI(LOG_TAG, "DMA transfer started (Quad lane, 8-bit)");

        /* 等待 DMA 完成 */
        ret = lisa_qspilcd_wait_done(qspi, 1000);

        /* 拉高 CS */
        lisa_qspilcd_cs_control(qspi, true);

        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "DMA transfer complete");
        } else {
            LISA_LOGE(LOG_TAG, "DMA transfer timeout");
            return -1;
        }
    }

    LISA_LOGI(LOG_TAG, "All transfers completed successfully!");

    return 0;
}
