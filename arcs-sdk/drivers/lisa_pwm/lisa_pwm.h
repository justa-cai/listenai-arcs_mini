/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_pwm.h
 * @brief LISA PWM 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * PWM 类型定义
 * ======================================================================== */

/**
 * @brief PWM 输出极性
 */
typedef enum {
    LISA_PWM_POLARITY_NORMAL = 0,   /* 正常极性：高电平有效 */
    LISA_PWM_POLARITY_INVERTED = 1, /* 反转极性：低电平有效 */
} lisa_pwm_polarity_t;

/**
 * @brief PWM 通道配置结构体
 */
typedef struct {
    lisa_pwm_polarity_t polarity; /**< 输出极性 */
} lisa_pwm_config_t;

/* ========================================================================
 * PWM 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*enable)(lisa_device_t *dev, uint32_t channel);
    int (*disable)(lisa_device_t *dev, uint32_t channel);
    int (*set)(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_cycle_percent);
    int (*configure)(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config);
    int (*get_config)(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config);
} lisa_pwm_api_t;

/* ========================================================================
 * PWM 对外接口函数
 * ======================================================================== */

/* ===== 控制接口 ===== */

/**
 * @brief 配置PWM通道属性
 *
 * 当前仅支持配置极性。
 *
 * @param dev PWM设备指针
 * @param channel PWM通道号
 * @param config 配置参数指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_pwm_api_t *api = (lisa_pwm_api_t *)dev->api;
    return api->configure ? api->configure(dev, channel, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取PWM通道配置
 *
 * @param dev PWM设备指针
 * @param channel PWM通道号
 * @param config 输出参数指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_pwm_get_config(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_pwm_api_t *api = (lisa_pwm_api_t *)dev->api;
    return api->get_config ? api->get_config(dev, channel, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 使能PWM通道
 *
 * 启动指定PWM通道的信号输出。
 *
 * @param dev PWM设备指针
 * @param channel PWM通道号
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 调用前需要先配置PWM通道参数
 */
static inline int lisa_pwm_enable(lisa_device_t *dev, uint32_t channel)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_pwm_api_t *api = (lisa_pwm_api_t *)dev->api;
    return api->enable ? api->enable(dev, channel) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 禁用PWM通道
 *
 * 停止指定PWM通道的信号输出。
 *
 * @param dev PWM设备指针
 * @param channel PWM通道号
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_pwm_disable(lisa_device_t *dev, uint32_t channel)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_pwm_api_t *api = (lisa_pwm_api_t *)dev->api;
    return api->disable ? api->disable(dev, channel) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/** 占空比百分比上限（100%） */
#define LISA_PWM_DUTY_PERCENT_MAX 100U

/**
 * @brief PWM 参数配置便捷调用
 *
 * @param dev 设备指针
 * @param ch 通道号
 * @param freq_hz 频率（Hz）
 * @param duty_percent 占空比百分比
 */
#define LISA_PWM_SET(dev, ch, freq_hz, duty_percent) lisa_pwm_set((dev), (ch), (freq_hz), (uint8_t)(duty_percent))

/**
 * @brief 配置PWM通道参数
 *
 * 同时设置指定PWM通道的频率与占空比。
 *
 * @param dev PWM设备指针
 * @param channel PWM通道号
 * @param frequency_hz 频率（Hz）
 * @param duty_cycle_percent 占空比（百分比，0-100）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 占空比范围为 0-100（对应 0%-100%）
 */
static inline int lisa_pwm_set(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_cycle_percent)
{
    if (!dev || !dev->api || duty_cycle_percent > LISA_PWM_DUTY_PERCENT_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_pwm_api_t *api = (lisa_pwm_api_t *)dev->api;
    return api->set ? api->set(dev, channel, frequency_hz, duty_cycle_percent) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

#ifdef __cplusplus
}
#endif
