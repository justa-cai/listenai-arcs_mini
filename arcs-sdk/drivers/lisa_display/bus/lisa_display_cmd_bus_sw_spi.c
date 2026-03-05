/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_display_cmd_bus_sw_spi.c
 * @brief 软件模拟 3 线 SPI 命令总线驱动实现
 *
 * 特性:
 * - 纯 GPIO 模拟 SPI 时序,无需硬件 SPI 外设
 * - 支持 3 线 SPI (CS, SCL, SDA)
 * - 支持 9 位协议 (DC 位 + 8 位数据)
 * - 支持可配置的 SPI 模式 (CPOL/CPHA)
 * - 符合 lisa_device 框架标准
 *
 * 基于 rgb_trans.c 中的实现
 */

#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_display_bus.h"
#include "lisa_gpio.h"
#include "lisa_mem.h"
#include <string.h>

#define LOG_TAG "cmd_bus_sw_spi"
#include "lisa_log.h"

/* ========================================================================
 * 私有数据结构
 * ======================================================================== */

typedef enum {
    SW_SPI_IO_CS = 0,
    SW_SPI_IO_SCL,
    SW_SPI_IO_SDA
} sw_spi_io_line_t;

/**
 * @brief 软件 SPI 命令总线私有数据
 */
typedef struct {
    /* GPIO 设备和引脚 */
    lisa_device_t *cs_gpio;
    uint32_t cs_pin;
    lisa_device_t *scl_gpio;
    uint32_t scl_pin;
    lisa_device_t *sda_gpio;
    uint32_t sda_pin;

    /* 时序参数 */
    uint32_t scl_half_period_us;  /* SCL 半周期时间(微秒) */

    /* 配置标志 */
    uint8_t lsb_first;             /* LSB 优先传输 */
    uint8_t cs_high_active;        /* CS 高电平有效 */
    uint8_t sda_scl_idle_high;     /* SDA/SCL 空闲电平为高 */
    uint8_t scl_active_rising_edge;/* SCL 上升沿采样 */

    /* 3 线 DC 位编码 */
    uint8_t use_dc_bit;            /* 使用 DC 位 */
    uint8_t cmd_dc_bit;            /* 命令帧的 DC 位值 */
    uint8_t data_dc_bit;           /* 数据帧的 DC 位值 */

    /* 状态 */
    uint8_t dc_level_curr;         /* 当前 DC 电平 */
    bool initialized;              /* 初始化标志 */
} sw_spi_cmd_bus_priv_t;

static sw_spi_cmd_bus_priv_t sw_spi_priv = {0};

/**
 * @brief 设置指定 IO 线的电平
 */
static int sw_spi_io_set(sw_spi_cmd_bus_priv_t *priv, sw_spi_io_line_t line, uint32_t level)
{
    lisa_device_t *gpio = NULL;
    uint32_t pin = 0;

    switch (line) {
    case SW_SPI_IO_CS:
        gpio = priv->cs_gpio;
        pin = priv->cs_pin;
        break;
    case SW_SPI_IO_SCL:
        gpio = priv->scl_gpio;
        pin = priv->scl_pin;
        break;
    case SW_SPI_IO_SDA:
        gpio = priv->sda_gpio;
        pin = priv->sda_pin;
        break;
    default:
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!gpio) {
        return LISA_DEVICE_ERR_INVALID;
    }

    return lisa_gpio_write_pin(gpio, pin, level);
}

/**
 * @brief 微秒级延时(简单实现)
 */
static void sw_spi_delay_us(uint32_t us)
{
    /* 简单的忙等待延时
     * TODO: 使用更精确的延时函数,如 SysTick_Delay_Us()
     */
    volatile uint32_t count = us * 10;  /* 粗略估计,实际需要根据 CPU 频率调整 */
    while (count--) {
        __asm volatile ("nop");
    }
}

/* ========================================================================
 * 软件 SPI 时序实现
 * ======================================================================== */

/**
 * @brief 获取写入顺序掩码
 */
static inline uint8_t sw_spi_write_order_mask(sw_spi_cmd_bus_priv_t *priv)
{
    return priv->lsb_first ? 0x01 : 0x80;
}

/**
 * @brief 写入一个字节(支持 9 位协议)
 *
 * @param priv 私有数据
 * @param dc_bit DC 位值(仅在 9 位模式下使用)
 * @param data 要发送的数据字节
 * @return 0 成功, <0 失败
 */
static int sw_spi_write_byte(sw_spi_cmd_bus_priv_t *priv, uint8_t dc_bit, uint8_t data)
{
    uint16_t temp = data;
    const uint8_t bits = priv->use_dc_bit ? 9 : 8;
    const uint32_t before = priv->scl_active_rising_edge ? 0 : 1;
    const uint32_t after = !before;
    const uint8_t mask = sw_spi_write_order_mask(priv);

    for (uint8_t i = 0; i < bits; i++) {
        if (bits == 9 && i == 0) {
            /* 第一位是 DC 位 */
            sw_spi_io_set(priv, SW_SPI_IO_SDA, dc_bit);
        } else {
            /* 数据位 */
            sw_spi_io_set(priv, SW_SPI_IO_SDA, temp & mask);
            temp = (mask == 0x01) ? (temp >> 1) : (temp << 1);
        }

        /* SCL 时钟 */
        sw_spi_io_set(priv, SW_SPI_IO_SCL, before);
        sw_spi_delay_us(priv->scl_half_period_us);
        sw_spi_io_set(priv, SW_SPI_IO_SCL, after);
        sw_spi_delay_us(priv->scl_half_period_us);
    }

    return 0;
}

/**
 * @brief 发送多个字节
 *
 * @param priv 私有数据
 * @param is_cmd 是否为命令(影响 DC 位)
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 0 成功, <0 失败
 */
static int sw_spi_send_bytes(sw_spi_cmd_bus_priv_t *priv, int is_cmd,
                              const uint8_t *data, uint32_t len)
{
    if (!data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    const uint32_t cs_idle = priv->cs_high_active ? 0 : 1;
    const uint32_t idle = priv->sda_scl_idle_high ? 1 : 0;

    /* 拉低 CS */
    sw_spi_io_set(priv, SW_SPI_IO_CS, !cs_idle);

    /* 发送每个字节 */
    for (uint32_t i = 0; i < len; i++) {
        const uint8_t dc_bit = is_cmd ? priv->cmd_dc_bit : priv->data_dc_bit;
        sw_spi_write_byte(priv, dc_bit, data[i]);
    }

    /* 拉高 CS 并恢复空闲状态 */
    sw_spi_io_set(priv, SW_SPI_IO_SCL, idle);
    sw_spi_io_set(priv, SW_SPI_IO_SDA, idle);
    sw_spi_io_set(priv, SW_SPI_IO_CS, cs_idle);

    return 0;
}

/* ========================================================================
 * lisa_display_cmd_bus_api_t 接口实现
 * ======================================================================== */

/**
 * @brief 配置软件 SPI 命令总线
 *
 * @param cmd_bus_type 命令总线类型
 * @param cmd_bus_config 命令总线配置
 * @return 0 成功, <0 失败
 */
static int sw_spi_cmd_bus_configure(lisa_display_cmd_bus_type_t cmd_bus_type,
                                      const lisa_display_cmd_bus_config_u *cmd_bus_config)
{
    if (cmd_bus_type != LISA_DISPLAY_CMD_BUS_SW_SPI) {
        LOGE("Invalid cmd_bus_type: %d", cmd_bus_type);
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!cmd_bus_config) {
        LOGE("cmd_bus_config is NULL");
        return LISA_DEVICE_ERR_INVALID;
    }

    sw_spi_cmd_bus_priv_t *priv = &sw_spi_priv;
    const lisa_display_cmd_sw_spi_config_t *config = &cmd_bus_config->sw_spi;

    /* 保存 GPIO 设备和引脚 */
    priv->cs_gpio = config->cs_gpio;
    priv->cs_pin = config->cs_pin;
    priv->scl_gpio = config->scl_gpio;
    priv->scl_pin = config->scl_pin;
    priv->sda_gpio = config->sda_gpio;
    priv->sda_pin = config->sda_pin;

    /* 计算 SCL 半周期时间 */
    uint32_t freq = config->spi_freq ? config->spi_freq : 500000;  /* 默认 500kHz */
    priv->scl_half_period_us = (1000000U / (freq * 2U));

    /* 保存配置标志 */
    priv->lsb_first = config->lsb_first;
    priv->cs_high_active = config->cs_high_active;

    /* 根据 SPI 模式配置时序 */
    priv->sda_scl_idle_high = (config->spi_mode & 0x1) ? 1 : 0;
    if (priv->sda_scl_idle_high) {
        priv->scl_active_rising_edge = (config->spi_mode & 0x2) ? 1 : 0;
    } else {
        priv->scl_active_rising_edge = (config->spi_mode & 0x2) ? 0 : 1;
    }

    /* 配置 DC 位 */
    priv->use_dc_bit = config->use_dc_bit;
    if (priv->use_dc_bit) {
        priv->data_dc_bit = config->dc_zero_on_data ? 0 : 1;
        priv->cmd_dc_bit = config->dc_zero_on_data ? 1 : 0;
    } else {
        priv->data_dc_bit = 0;
        priv->cmd_dc_bit = 0;
    }

    /* 配置 GPIO 为输出模式 */
    if (priv->cs_gpio) {
        lisa_gpio_configure(priv->cs_gpio, priv->cs_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    }
    if (priv->scl_gpio) {
        lisa_gpio_configure(priv->scl_gpio, priv->scl_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    }
    if (priv->sda_gpio) {
        lisa_gpio_configure(priv->sda_gpio, priv->sda_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    }

    /* 设置空闲状态 */
    const uint32_t cs_idle = priv->cs_high_active ? 0 : 1;
    const uint32_t idle = priv->sda_scl_idle_high ? 1 : 0;
    sw_spi_io_set(priv, SW_SPI_IO_CS, cs_idle);
    sw_spi_io_set(priv, SW_SPI_IO_SCL, idle);
    sw_spi_io_set(priv, SW_SPI_IO_SDA, idle);

    priv->initialized = true;

    LOGI("Software SPI command bus configured: freq=%lu Hz, mode=%d, dc_bit=%d",
              (unsigned long)freq, config->spi_mode, config->use_dc_bit);

    return LISA_DEVICE_OK;
}

/**
 * @brief 发送命令和参数数据
 *
 * @param cmd 命令字节
 * @param cmd_bits 命令位宽
 * @param data 参数数据缓冲区
 * @param len 参数数据长度
 * @return 0 成功, <0 失败
 */
static int sw_spi_cmd_bus_write_cmd(uint32_t cmd, uint8_t cmd_bits,
                                      const void *data, size_t len)
{
    sw_spi_cmd_bus_priv_t *priv = &sw_spi_priv;

    if (!priv->initialized) {
        LOGE("Bus not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 发送命令字节(is_cmd=1) */
    uint8_t cmd_byte = (uint8_t)cmd;
    int ret = sw_spi_send_bytes(priv, 1, &cmd_byte, 1);
    if (ret != 0) {
        return ret;
    }

    /* 发送参数数据(is_cmd=0) */
    if (data && len > 0) {
        ret = sw_spi_send_bytes(priv, 0, data, len);
    }

    return ret;
}

/**
 * @brief 读取屏幕配置参数
 *
 * 注意: 软件模拟 SPI 的读取功能较为复杂,当前版本暂不实现。
 * 如需读取功能,建议使用硬件 SPI。
 *
 * @param cmd 命令字节
 * @param cmd_bits 命令位宽
 * @param data 接收数据缓冲区
 * @param len 要读取的数据长度
 * @return 0 成功, <0 失败
 */
static int sw_spi_cmd_bus_read_cmd(uint32_t cmd, uint8_t cmd_bits,
                                     void *data, size_t len)
{
    /* 软件 SPI 读取功能暂不实现 */
    LOGW("Software SPI read not implemented yet");
    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * API 结构体
 * ======================================================================== */

static const lisa_display_cmd_bus_api_t sw_spi_cmd_bus_api = {
    .configure = sw_spi_cmd_bus_configure,
    .write_cmd = sw_spi_cmd_bus_write_cmd,
    .read_cmd = sw_spi_cmd_bus_read_cmd,
};

static int bus_sw_spi_init(void)
{
    memset(&sw_spi_priv, 0, sizeof(sw_spi_priv));

    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(cmd_bus_sw_spi, &sw_spi_cmd_bus_api, &sw_spi_priv, NULL, bus_sw_spi_init, LISA_DEVICE_PRIORITY_HIGH);
