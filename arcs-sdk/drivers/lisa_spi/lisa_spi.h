/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_spi.h
 * @brief LISA SPI 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SPI 类型定义
 * ======================================================================== */

/**
 * @brief SPI 工作模式
 */
typedef enum {
    LISA_SPI_MODE_0 = 0, /* CPOL=0, CPHA=0 */
    LISA_SPI_MODE_1 = 1, /* CPOL=0, CPHA=1 */
    LISA_SPI_MODE_2 = 2, /* CPOL=1, CPHA=0 */
    LISA_SPI_MODE_3 = 3, /* CPOL=1, CPHA=1 */
} lisa_spi_mode_t;

/**
 * @brief SPI 位序
 */
typedef enum {
    LISA_SPI_BIT_ORDER_MSB_FIRST = 0, /* MSB 先传输 */
    LISA_SPI_BIT_ORDER_LSB_FIRST = 1, /* LSB 先传输 */
} lisa_spi_bit_order_t;

/**
 * @brief SPI 传输标志位
 */
typedef enum {
    LISA_SPI_FLAG_SOFTWARE_CS = 0x00,       /* 软件控制片选（由用户通过GPIO手动控制） */
    LISA_SPI_FLAG_HARDWARE_CS = 0x01,       /* 硬件控制片选（由驱动自动控制） */
} lisa_spi_transfer_flags_t;

/**
 * @brief SPI 传输模式
 */
typedef enum {
    LISA_SPI_DMA_TRANSFER = 0,       /* 使用DMA进行传输(需要用户预留DMA通道) */
    LISA_SPI_INTERRUPT_TRANSFER = 1, /* 使用中断进行传输 */
} lisa_spi_transfer_mode_t;

/**
 * @brief SPI 传输完成回调函数类型
 */
typedef void (*lisa_spi_transfer_callback_t)(void *user_data);

/**
 * @brief SPI 配置结构体
 */
typedef struct {
    uint32_t frequency;                         /* 时钟频率（Hz） */
    lisa_spi_mode_t mode;                       /* SPI 模式 */
    lisa_spi_bit_order_t bit_order;             /* 位序 */
    uint8_t data_bits;                          /* 数据位宽（通常为 8 或 16） */
    lisa_spi_transfer_flags_t flags;            /* 传输标志位 */
    bool master_mode;                           /* true: 主机模式, false: 从机模式 */
    lisa_spi_transfer_mode_t tx_transfer_mode;  /* 发送传输模式 */
    uint8_t tx_dma_channel;                     /* DMA TX通道 (0-3, 或 0xFF 自动分配) */
    lisa_spi_transfer_mode_t rx_transfer_mode;  /* 接收传输模式 */
    uint8_t rx_dma_channel;                     /* DMA RX通道 (0-3, 或 0xFF 自动分配) */
} lisa_spi_config_t;

/**
 * @brief SPI 传输结构体
 *
 * 用于描述一次 SPI 传输操作
 */
typedef struct {
    const uint8_t *tx_buf; /* 发送数据缓冲区指针*/
    uint8_t *rx_buf;       /* 接收数据缓冲区指针*/
    uint32_t len;          /* 传输数据长度（字节） */
} lisa_spi_transfer_t;

/* ========================================================================
 * SPI 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*configure)(lisa_device_t *dev, const lisa_spi_config_t *config);
    int (*get_config)(lisa_device_t *dev, lisa_spi_config_t *config);
    int (*transfer)(lisa_device_t *dev, const lisa_spi_transfer_t *xfer);
    int (*write)(lisa_device_t *dev, const uint8_t *buf, uint32_t len);
    int (*read)(lisa_device_t *dev, uint8_t *buf, uint32_t len);
    int (*register_callback)(lisa_device_t *dev, lisa_spi_transfer_callback_t cb, void *user_data);
} lisa_spi_api_t;

/* ========================================================================
 * SPI 对外接口函数
 * ======================================================================== */

/**
 * @brief 注册 SPI 传输完成回调函数
 *
 * @param dev SPI 设备指针
 * @param cb  回调函数指针
 * @param user_data 用户自定义数据指针
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_spi_register_callback(lisa_device_t *dev, lisa_spi_transfer_callback_t cb, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_api_t *api = (lisa_spi_api_t *)dev->api;
    return api->register_callback ? api->register_callback(dev, cb, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== 配置接口 ===== */

/**
 * @brief 配置SPI总线
 *
 * 配置SPI总线的工作模式和参数。
 *
 * @param dev SPI设备指针
 * @param config 配置参数结构体指针，包含：
 *               - frequency: 时钟频率（Hz）
 *               - mode: SPI 工作模式（0-3）
 *               - bit_order: 位序（MSB/LSB first）
 *               - data_bits: 数据位宽
 *               - flags: 片选控制标志位（硬件/软件CS）
 *               - master_mode: 主机/从机模式
 *               - tx_transfer_mode: 发送传输模式（DMA/中断）
 *               - tx_dma_channel: DMA TX通道号（0-3）
 *               - rx_transfer_mode: 接收传输模式（DMA/中断）
 *               - rx_dma_channel: DMA RX通道号（0-3）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 配置前请确保设备已初始化
 */
static inline int lisa_spi_configure(lisa_device_t *dev, const lisa_spi_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_api_t *api = (lisa_spi_api_t *)dev->api;
    return api->configure ? api->configure(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取SPI总线当前配置
 *
 * 读取SPI总线的当前配置信息。
 *
 * @param dev SPI设备指针
 * @param config 输出参数，用于接收配置信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_spi_get_config(lisa_device_t *dev, lisa_spi_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_api_t *api = (lisa_spi_api_t *)dev->api;
    return api->get_config ? api->get_config(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== 传输接口 ===== */

/**
 * @brief SPI通用传输接口
 *
 * 执行SPI数据传输。
 *
 * @param dev SPI设备指针
 * @param xfer 传输描述结构体指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note tx_buf 和 rx_buf 都不为 NULL
 * @note 全双工模式下同时发送和接收数据
 */
static inline int lisa_spi_transfer(lisa_device_t *dev, const lisa_spi_transfer_t *xfer)
{
    if (!dev || !dev->api || !xfer || (!xfer->tx_buf || !xfer->rx_buf) || xfer->len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_api_t *api = (lisa_spi_api_t *)dev->api;
    return api->transfer ? api->transfer(dev, xfer) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief SPI写数据
 *
 * 向SPI总线写入数据（只发送）。
 *
 * @param dev SPI设备指针
 * @param buf 数据缓冲区指针
 * @param len 数据长度
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_spi_write(lisa_device_t *dev, const uint8_t *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_api_t *api = (lisa_spi_api_t *)dev->api;
    return api->write ? api->write(dev, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief SPI读数据
 *
 * 从SPI总线读取数据（只接收）。
 *
 * @param dev SPI设备指针
 * @param buf 数据缓冲区指针
 * @param len 要读取的数据长度
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 读取时会发送虚拟数据（通常为 0xFF）以产生时钟
 */
static inline int lisa_spi_read(lisa_device_t *dev, uint8_t *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_api_t *api = (lisa_spi_api_t *)dev->api;
    return api->read ? api->read(dev, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 默认SPI配置（模式0，8位，MSB先行，1MHz，中断模式）
 */
#define LISA_SPI_CONFIG_DEFAULT()                                                                                          \
    {                                                                                                                      \
        .frequency = 1000000,                                                                                              \
        .mode = LISA_SPI_MODE_0,                                                                                           \
        .bit_order = LISA_SPI_BIT_ORDER_MSB_FIRST,                                                                         \
        .data_bits = 8,                                                                                                    \
        .flags = LISA_SPI_FLAG_HARDWARE_CS,                                                                                \
        .master_mode = true,                                                                                               \
        .tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,                                                                   \
        .rx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,                                                                   \
    }

/**
 * @brief 高速SPI配置（10MHz，中断模式）
 */
#define LISA_SPI_CONFIG_HIGH_SPEED()                                                                                       \
    {                                                                                                                      \
        .frequency = 10000000,                                                                                             \
        .mode = LISA_SPI_MODE_0,                                                                                           \
        .bit_order = LISA_SPI_BIT_ORDER_MSB_FIRST,                                                                         \
        .data_bits = 8,                                                                                                    \
        .flags = LISA_SPI_FLAG_HARDWARE_CS,                                                                                \
        .master_mode = true,                                                                                               \
        .tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,                                                                   \
        .rx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,                                                                   \
    }

#ifdef __cplusplus
}
#endif
