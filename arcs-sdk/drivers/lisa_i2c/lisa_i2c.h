/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_i2c.h
 * @brief LISA I2C 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * I2C 类型定义
 * ======================================================================== */

/**
 * @brief I2C 传输标志位
 */
typedef enum {
    LISA_I2C_FLAG_NONE = 0x00,       /* 无特殊标志 */
    LISA_I2C_FLAG_NO_START = 0x01,   /* 不发送START条件 */
    LISA_I2C_FLAG_NO_STOP = 0x02,    /* 不发送STOP条件 */
    LISA_I2C_FLAG_10BIT_ADDR = 0x04, /* 使用10位地址模式 */
    LISA_I2C_FLAG_READ = 0x08,       /* 读操作（默认为写操作） */
} lisa_i2c_flags_t;

/**
 * @brief I2C 速度模式
 */
typedef enum {
    LISA_I2C_SPEED_STANDARD = 100000,  /* 标准模式: 100 kHz */
    LISA_I2C_SPEED_FAST = 400000,      /* 快速模式: 400 kHz */
    LISA_I2C_SPEED_FAST_PLUS = 1000000,/* 快速增强模式: 1 MHz */
} lisa_i2c_speed_t;

/**
 * @brief I2C 消息结构体
 *
 * 用于描述一次 I2C 传输操作
 */
typedef struct {
    uint16_t addr;       /* 从机地址（7位或10位） */
    uint8_t flags;       /* 传输标志位（lisa_i2c_flags_t） */
    uint16_t len;        /* 数据长度 */
    uint8_t *buf;        /* 数据缓冲区指针 */
} lisa_i2c_msg_t;

/**
 * @brief I2C 配置结构体
 */
typedef struct {
    uint32_t speed;      /* 总线速度（Hz） */
    bool master_mode;    /* true: 主机模式, false: 从机模式 */
    uint16_t slave_addr; /* 从机模式下的本机地址 */
} lisa_i2c_config_t;

/* ========================================================================
 * I2C 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*configure)(lisa_device_t *dev, const lisa_i2c_config_t *config);
    int (*get_config)(lisa_device_t *dev, lisa_i2c_config_t *config);
    int (*transfer)(lisa_device_t *dev, lisa_i2c_msg_t *msgs, uint32_t num_msgs);
    int (*write)(lisa_device_t *dev, uint16_t addr, const uint8_t *buf, uint32_t len);
    int (*read)(lisa_device_t *dev, uint16_t addr, uint8_t *buf, uint32_t len);
} lisa_i2c_api_t;

/* ========================================================================
 * I2C 对外接口函数
 * ======================================================================== */

/* ===== 配置接口 ===== */

/**
 * @brief 配置I2C总线
 *
 * 配置I2C总线的工作模式和参数。
 *
 * @param dev I2C设备指针
 * @param config 配置参数结构体指针，包含：
 *               - speed: 总线速度（Hz）
 *               - master_mode: 主机/从机模式
 *               - slave_addr: 从机模式下的本机地址
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 配置前请确保设备已初始化
 */
static inline int lisa_i2c_configure(lisa_device_t *dev, const lisa_i2c_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2c_api_t *api = (lisa_i2c_api_t *)dev->api;
    return api->configure ? api->configure(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取I2C总线当前配置
 *
 * 读取I2C总线的当前配置信息。
 *
 * @param dev I2C设备指针
 * @param config 输出参数，用于接收配置信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_i2c_get_config(lisa_device_t *dev, lisa_i2c_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2c_api_t *api = (lisa_i2c_api_t *)dev->api;
    return api->get_config ? api->get_config(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== 传输接口 ===== */

/**
 * @brief I2C通用传输接口
 *
 * 执行一组I2C消息传输，支持组合传输（如写后读）。
 *
 * @param dev I2C设备指针
 * @param msgs 消息数组指针
 * @param num_msgs 消息数量
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_TIMEOUT 传输超时
 * @return LISA_DEVICE_ERR_NACK 从机无应答
 * @return <0 其他错误
 *
 * @note 消息数组中的传输会按顺序执行
 * @note 使用flags可以控制START/STOP条件的发送
 */
static inline int lisa_i2c_transfer(lisa_device_t *dev, lisa_i2c_msg_t *msgs, uint32_t num_msgs)
{
    if (!dev || !dev->api || !msgs || num_msgs == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2c_api_t *api = (lisa_i2c_api_t *)dev->api;
    return api->transfer ? api->transfer(dev, msgs, num_msgs) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief I2C写数据
 *
 * 向指定从机地址写入数据。
 *
 * @param dev I2C设备指针
 * @param addr 从机地址（7位）
 * @param buf 数据缓冲区指针
 * @param len 数据长度
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_TIMEOUT 传输超时
 * @return LISA_DEVICE_ERR_NACK 从机无应答
 * @return <0 其他错误
 */
static inline int lisa_i2c_write(lisa_device_t *dev, uint16_t addr, const uint8_t *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2c_api_t *api = (lisa_i2c_api_t *)dev->api;
    return api->write ? api->write(dev, addr, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief I2C读数据
 *
 * 从指定从机地址读取数据。
 *
 * @param dev I2C设备指针
 * @param addr 从机地址（7位）
 * @param buf 数据缓冲区指针
 * @param len 要读取的数据长度
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_TIMEOUT 传输超时
 * @return LISA_DEVICE_ERR_NACK 从机无应答
 * @return <0 其他错误
 */
static inline int lisa_i2c_read(lisa_device_t *dev, uint16_t addr, uint8_t *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2c_api_t *api = (lisa_i2c_api_t *)dev->api;
    return api->read ? api->read(dev, addr, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 默认I2C配置（标准速度，主机模式）
 */
#define LISA_I2C_CONFIG_DEFAULT()                                                                                          \
    {                                                                                                                      \
        .speed = LISA_I2C_SPEED_STANDARD,                                                                                  \
        .master_mode = true,                                                                                               \
        .slave_addr = 0,                                                                                                   \
    }

/**
 * @brief 快速模式I2C配置
 */
#define LISA_I2C_CONFIG_FAST()                                                                                             \
    {                                                                                                                      \
        .speed = LISA_I2C_SPEED_FAST,                                                                                      \
        .master_mode = true,                                                                                               \
        .slave_addr = 0,                                                                                                   \
    }

#ifdef __cplusplus
}
#endif
