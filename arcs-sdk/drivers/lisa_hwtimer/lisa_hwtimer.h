/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_hwtimer.h
 * @brief LISA 硬件定时器设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 硬件定时器类型定义
 * ======================================================================== */

/**
 * @brief 定时器模式
 */
typedef enum {
    LISA_HWTIMER_MODE_ONESHOT = 0,  /* 单次触发模式 */
    LISA_HWTIMER_MODE_PERIODIC = 1, /* 周期触发模式 */
} lisa_hwtimer_mode_t;

/**
 * @brief 定时器配置结构体
 */
typedef struct {
    lisa_hwtimer_mode_t mode;   /* 定时器模式 */
    uint32_t count;             /* 计数值 */
} lisa_hwtimer_config_t;

/**
 * @brief 定时器能力结构体
 */
typedef struct {
    uint8_t channel_count;      /* 支持的通道数量 */
    uint32_t max_count;         /* 最大计数值 */
    uint32_t min_count;         /* 最小计数值 */
    uint32_t max_freq_hz;       /* 最大频率（Hz） */
    uint32_t min_freq_hz;       /* 最小频率（Hz） */
    bool support_oneshot;       /* 是否支持单次模式 */
    bool support_periodic;      /* 是否支持周期模式 */
} lisa_hwtimer_capabilities_t;

/**
 * @brief 定时器超时回调函数类型
 *
 * @param user_data 用户数据指针
 */
typedef void (*lisa_hwtimer_callback_t)(void *user_data);

/* ========================================================================
 * 硬件定时器设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*get_capabilities)(lisa_device_t *dev, lisa_hwtimer_capabilities_t *caps);
    int (*set_frequency)(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz);
    int (*start)(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode);
    int (*stop)(lisa_device_t *dev, uint8_t channel);
    int (*reset)(lisa_device_t *dev, uint8_t channel);
    int (*get_value)(lisa_device_t *dev, uint8_t channel, uint32_t *count);
    int (*set_callback)(lisa_device_t *dev, uint8_t channel, lisa_hwtimer_callback_t callback, void *user_data);
} lisa_hwtimer_api_t;

/* ========================================================================
 * 硬件定时器对外接口函数
 * ======================================================================== */

/**
 * @brief 获取定时器硬件能力
 *
 * @param dev 定时器设备指针
 * @param caps 输出参数，用于接收设备能力信息
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 使用定时器前应先调用此接口获取支持的通道数和其他能力信息
 */
static inline int lisa_hwtimer_get_capabilities(lisa_device_t *dev,
                                                lisa_hwtimer_capabilities_t *caps)
{
    if (!dev || !dev->api || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->get_capabilities ? api->get_capabilities(dev, caps) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置硬件定时器频率
 *
 * @param dev 定时器设备指针
 * @param channel 通道号（从0开始）
 * @param freq_hz 定时器频率（Hz）
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_RANGE 频率超出范围或通道号无效
 * @return <0 其他错误
 *
 * @note 设置频率后，start 接口传入的 count 值会根据该频率计算实际时间
 * @note 例如：频率设为 1000000Hz (1MHz)，count 为 1000，则定时 1ms
 */
static inline int lisa_hwtimer_set_frequency(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->set_frequency ? api->set_frequency(dev, channel, freq_hz) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启动硬件定时器
 *
 * @param dev 定时器设备指针
 * @param channel 通道号（从0开始）
 * @param count 计数值（根据设置的频率计算实际时间）
 * @param mode 定时器模式（单次或周期）
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效或未设置回调函数
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_RANGE 计数值超出范围或通道号无效
 * @return <0 其他错误
 *
 * @note 启动定时器前必须先调用 lisa_hwtimer_set_callback 设置回调函数，否则会返回错误
 * @note 在单次模式下，定时器触发一次后自动停止
 * @note 在周期模式下，定时器会持续触发直到调用停止函数
 * @note 实际定时时间 = count / 频率。例如：频率1MHz，count=1000，则定时1ms
 */
static inline int lisa_hwtimer_start(lisa_device_t *dev, uint8_t channel, uint32_t count,
                                     lisa_hwtimer_mode_t mode)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->start ? api->start(dev, channel, count, mode) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 停止硬件定时器
 *
 * @param dev 定时器设备指针
 * @param channel 通道号（从0开始）
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 * @return <0 其他错误
 */
static inline int lisa_hwtimer_stop(lisa_device_t *dev, uint8_t channel)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->stop ? api->stop(dev, channel) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 重置硬件定时器
 *
 * 重置定时器计数值到初始状态，但不停止定时器
 *
 * @param dev 定时器设备指针
 * @param channel 通道号（从0开始）
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 * @return <0 其他错误
 */
static inline int lisa_hwtimer_reset(lisa_device_t *dev, uint8_t channel)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->reset ? api->reset(dev, channel) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取当前定时器计数值
 *
 * @param dev 定时器设备指针
 * @param channel 通道号（从0开始）
 * @param count 输出参数，返回当前计数值
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 * @return <0 其他错误
 *
 * @note 返回的是硬件计数器的当前计数值，不是时间值
 * @note 如需转换为时间，需结合设置的频率进行计算：时间(秒) = count / 频率(Hz)
 */
static inline int lisa_hwtimer_get_value(lisa_device_t *dev, uint8_t channel, uint32_t *count)
{
    if (!dev || !dev->api || !count) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->get_value ? api->get_value(dev, channel, count) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置定时器超时回调函数
 *
 * @param dev 定时器设备指针
 * @param channel 通道号（从0开始）
 * @param callback 回调函数指针
 * @param user_data 用户数据指针
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 * @return <0 其他错误
 */
static inline int lisa_hwtimer_set_callback(lisa_device_t *dev,
                                            uint8_t channel,
                                            lisa_hwtimer_callback_t callback,
                                            void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_hwtimer_api_t *api = (lisa_hwtimer_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, channel, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 启动单次定时器
 *
 * @param dev 定时器设备指针
 * @param channel 通道号
 * @param count 计数值
 */
#define LISA_HWTIMER_START_ONESHOT(dev, channel, count) \
    lisa_hwtimer_start((dev), (channel), (count), LISA_HWTIMER_MODE_ONESHOT)

/**
 * @brief 启动周期定时器
 *
 * @param dev 定时器设备指针
 * @param channel 通道号
 * @param count 计数值
 */
#define LISA_HWTIMER_START_PERIODIC(dev, channel, count) \
    lisa_hwtimer_start((dev), (channel), (count), LISA_HWTIMER_MODE_PERIODIC)

#ifdef __cplusplus
}
#endif
