/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA I2C 基础扫描示例
 *
 * 演示内容：
 * 1. 初始化 I2C 设备并配置速率
 * 2. 通过 IOMUX 将引脚切换为 I2C 功能
 * 3. 遍历 7 位地址空间并检测 ACK
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_i2c.h"
#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "pinmux.h"

#define I2C_DEVICE       "i2c0"
#ifdef CONFIG_BOARD_ARCS_MINI
/* ARCS_MINI I2C0: PB6=SDA, PB7=SCL, 已在 pinmux.c 中配置 */
#define IIC0_GPIO_SCL    7
#define IIC0_GPIO_SDA    6
#define IIC0_PAD_NAME    "PB"
#else
#define IIC0_GPIO_SCL    23
#define IIC0_GPIO_SDA    22
#define IIC0_PAD_NAME    "PA"
#endif

/*
    为满足不同板型示例场景，重定向 I2C0 设备的 pinmux 配置
    
    注意：此重定向会覆盖 boards/arcs_evb/pinmux.c 中的默认实现（使用 weak 符号机制）
    - 默认实现仅配置了 PA23 (SCL)，缺少 PA22 (SDA) 配置
    - 示例中需要完整配置 I2C0 的 SDA 和 SCL 引脚（PA22=SDA, PA23=SCL，功能码 8）
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC0_GPIO_SDA, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC0_GPIO_SCL, CSK_IOMUX_FUNC_ALTER8);
}
#endif

static bool probe_address(lisa_device_t *dev, uint8_t addr)
{
    lisa_i2c_msg_t msg = {
        .addr = addr,
        .flags = LISA_I2C_FLAG_NONE,
        .len = 0,
        .buf = NULL,
    };
    return lisa_i2c_transfer(dev, &msg, 1) == LISA_DEVICE_OK;
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA I2C bus scan example ===");

    /* I2C0 引脚复用配置已由驱动初始化时自动完成（调用示例中重写的 lisa_i2c0_pinmux） */
    LISA_LOGI(LOG_TAG, "I2C0 SDA@%s%02d / SCL@%s%02d configured", IIC0_PAD_NAME, IIC0_GPIO_SDA, IIC0_PAD_NAME, IIC0_GPIO_SCL);

    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", I2C_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", I2C_DEVICE);

    lisa_i2c_config_t cfg = {
        .speed = LISA_I2C_SPEED_STANDARD,
        .master_mode = true,
        .slave_addr = 0,
    };

    if (lisa_i2c_configure(i2c_dev, &cfg) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: failed to configure I2C bus");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Start scanning I2C bus...");

    uint32_t found = 0;
    for (uint8_t addr = 1; addr < 0x80; addr++) {
        if (probe_address(i2c_dev, addr)) {
            LISA_LOGI(LOG_TAG, "Found I2C device at address: 0x%02X", addr);
            found++;
        }
    }

    if (found == 0) {
        LISA_LOGW(LOG_TAG, "No I2C devices detected");
    }
    LISA_LOGI(LOG_TAG, "I2C bus scan finished");

    return 0;
}


