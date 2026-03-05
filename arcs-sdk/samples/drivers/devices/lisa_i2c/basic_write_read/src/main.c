/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA I2C 基础读写示例
 *
 * 演示内容：
 * 1. 使用 lisa_i2c_write() 写入数据的 API 用法
 * 2. 使用 lisa_i2c_read() 读取数据的 API 用法
 * 3. 使用 lisa_i2c_transfer() 实现写后读组合操作的 API 用法
 *
 * 注意：本示例仅演示 API 用法，不依赖特定 I2C 设备硬件
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_i2c.h"
#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "pinmux.h"

#include "FreeRTOS.h"
#include "task.h"

#define I2C_DEVICE       "i2c0"
#define IIC0_GPIO_SCL    23
#define IIC0_GPIO_SDA    22

/* 示例设备地址（用于演示，实际使用时需根据设备手册修改） */
#define DEVICE_ADDR      0x50

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

/**
 * @brief 示例1：使用 write 接口 API 演示
 */
static void example_write_api(lisa_device_t *dev, uint8_t addr)
{
    LISA_LOGI(LOG_TAG, "--- Example 1: Write API ---");
    
    /* API 用法：lisa_i2c_write(device, slave_addr, buffer, length) */
    uint8_t write_data[] = {0x00, 0x55, 0xAA, 0x12, 0x34};
    
    LISA_LOGI(LOG_TAG, "Calling: lisa_i2c_write(dev, 0x%02X, data, %d)", addr, sizeof(write_data));
    LISA_LOGI(LOG_TAG, "Data to write: [0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]", 
              write_data[0], write_data[1], write_data[2], write_data[3], write_data[4]);
    
    int ret = lisa_i2c_write(dev, addr, write_data, sizeof(write_data));
    if (ret == LISA_DEVICE_OK) {
        LISA_LOGI(LOG_TAG, "Write operation completed successfully");
    } else if (ret == LISA_DEVICE_ERR_NACK) {
        LISA_LOGW(LOG_TAG, "Device not responding (NACK) - no device at 0x%02X", addr);
    } else {
        LISA_LOGE(LOG_TAG, "Write failed with error code: %d", ret);
    }
}

/**
 * @brief 示例2：使用 read 接口 API 演示
 */
static void example_read_api(lisa_device_t *dev, uint8_t addr)
{
    LISA_LOGI(LOG_TAG, "--- Example 2: Read API ---");
    
    /* API 用法：lisa_i2c_read(device, slave_addr, buffer, length) */
    uint8_t read_data[4] = {0};
    
    LISA_LOGI(LOG_TAG, "Calling: lisa_i2c_read(dev, 0x%02X, buffer, %d)", addr, sizeof(read_data));
    
    int ret = lisa_i2c_read(dev, addr, read_data, sizeof(read_data));
    if (ret == LISA_DEVICE_OK) {
        LISA_LOGI(LOG_TAG, "Read operation completed successfully");
        LISA_LOGI(LOG_TAG, "Data read: [0x%02X 0x%02X 0x%02X 0x%02X]", 
                  read_data[0], read_data[1], read_data[2], read_data[3]);
    } else if (ret == LISA_DEVICE_ERR_NACK) {
        LISA_LOGW(LOG_TAG, "Device not responding (NACK) - no device at 0x%02X", addr);
    } else {
        LISA_LOGE(LOG_TAG, "Read failed with error code: %d", ret);
    }
}

/**
 * @brief 示例3：使用 transfer 接口实现写后读组合操作
 */
static void example_transfer_api(lisa_device_t *dev, uint8_t addr)
{
    LISA_LOGI(LOG_TAG, "--- Example 3: Transfer API (Write-Read Combined) ---");
    
    /* API 用法：lisa_i2c_transfer(device, messages, num_messages) */
    /* 典型场景：先写寄存器地址，再读取寄存器内容（无 STOP 条件） */
    
    lisa_i2c_msg_t msgs[2];
    
    /* 消息1：写入寄存器地址（不发送 STOP，继续传输） */
    uint8_t reg_addr = 0x00;
    msgs[0].addr = addr;
    msgs[0].flags = LISA_I2C_FLAG_NO_STOP;  /* 关键：写操作，不发送 STOP */
    msgs[0].len = 1;
    msgs[0].buf = &reg_addr;
    
    /* 消息2：读取数据（需设置 READ 标志） */
    uint8_t read_buf[4];
    msgs[1].addr = addr;
    msgs[1].flags = LISA_I2C_FLAG_READ;     /* 关键：读操作标志 */
    msgs[1].len = sizeof(read_buf);
    msgs[1].buf = read_buf;
    
    LISA_LOGI(LOG_TAG, "Calling: lisa_i2c_transfer(dev, msgs, 2)");
    LISA_LOGI(LOG_TAG, "  Message[0]: WRITE reg_addr=0x%02X (NO_STOP)", reg_addr);
    LISA_LOGI(LOG_TAG, "  Message[1]: READ %d bytes (READ flag)", sizeof(read_buf));
    
    int ret = lisa_i2c_transfer(dev, msgs, 2);
    if (ret == LISA_DEVICE_OK) {
        LISA_LOGI(LOG_TAG, "Transfer operation completed successfully");
        LISA_LOGI(LOG_TAG, "Data read: [0x%02X 0x%02X 0x%02X 0x%02X]",
                  read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
    } else if (ret == LISA_DEVICE_ERR_NACK) {
        LISA_LOGW(LOG_TAG, "Device not responding (NACK) - no device at 0x%02X", addr);
    } else {
        LISA_LOGE(LOG_TAG, "Transfer failed with error code: %d", ret);
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA I2C basic write/read example ===");

    /* I2C0 引脚复用配置已由驱动初始化时自动完成（调用示例中重写的 lisa_i2c0_pinmux） */
    LISA_LOGI(LOG_TAG, "I2C0 SDA@PA%02d / SCL@PA%02d configured", IIC0_GPIO_SDA, IIC0_GPIO_SCL);

    /* 获取 I2C 设备 */
    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", I2C_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", I2C_DEVICE);

    /* 配置 I2C 总线 */
    lisa_i2c_config_t cfg = {
        .speed = LISA_I2C_SPEED_STANDARD,
        .master_mode = true,
        .slave_addr = 0,
    };

    if (lisa_i2c_configure(i2c_dev, &cfg) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Error: failed to configure I2C bus");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "I2C configured (speed: %lu Hz)\n", cfg.speed);

    LISA_LOGI(LOG_TAG, "========================================");
    LISA_LOGI(LOG_TAG, "Demonstrating LISA I2C API usage");
    LISA_LOGI(LOG_TAG, "Target device address: 0x%02X", DEVICE_ADDR);
    LISA_LOGI(LOG_TAG, "Note: Examples show API usage, actual results depend on hardware");
    LISA_LOGI(LOG_TAG, "========================================\n");

    /* 示例1: 写操作 API */
    example_write_api(i2c_dev, DEVICE_ADDR);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    /* 示例2: 读操作 API */
    example_read_api(i2c_dev, DEVICE_ADDR);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    /* 示例3: 组合传输 API（写后读） */
    example_transfer_api(i2c_dev, DEVICE_ADDR);

    LISA_LOGI(LOG_TAG, "\n========================================");
    LISA_LOGI(LOG_TAG, "All API examples completed");
    LISA_LOGI(LOG_TAG, "========================================");
    
    return 0;
}

