/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_wdt.h
 * @brief LISA Watchdog Timer 看门狗设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * WDT 类型定义
 * ======================================================================== */

/**
 * @brief WDT 超时行为
 */
typedef enum {
    LISA_WDT_ACTION_RESET = 0,       /* 系统复位 */
    LISA_WDT_ACTION_INTERRUPT = 1,   /* 触发中断 */
} lisa_wdt_action_t;

/**
 * @brief WDT 配置结构体
 */
typedef struct {
    uint32_t int_timeout_ms;             /* 中断超时时间（毫秒）*/
    uint32_t rst_timeout_ms;             /* 复位超时时间（毫秒）从中断超时时间开始计算*/
} lisa_wdt_config_t;

/**
 * @brief WDT 状态
 */
typedef enum {
    LISA_WDT_STATE_IDLE = 0,         /* 空闲状态 */
    LISA_WDT_STATE_RUNNING = 1,      /* 运行中 */
    LISA_WDT_STATE_EXPIRED = 2,      /* 已超时 */
} lisa_wdt_state_t;

/**
 * @brief WDT 超时回调函数类型
 *
 * @param user_data 用户数据指针
 *
 * @note 此回调在超时中断中执行，应尽快返回
 * @note 如果需要访问设备，可以通过 user_data 传入设备指针
 */
typedef void (*lisa_wdt_callback_t)(void *user_data);

/* ========================================================================
 * WDT 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*setup)(lisa_device_t *dev, const lisa_wdt_config_t *config);
    int (*start)(lisa_device_t *dev);
    int (*stop)(lisa_device_t *dev);
    int (*feed)(lisa_device_t *dev);
    int (*get_remaining_time)(lisa_device_t *dev, uint32_t *remaining_ms);
    int (*get_state)(lisa_device_t *dev, lisa_wdt_state_t *state);
    int (*set_callback)(lisa_device_t *dev, lisa_wdt_callback_t callback, void *user_data);
} lisa_wdt_api_t;

/* ========================================================================
 * WDT 对外接口函数
 * ======================================================================== */

/**
 * @brief 配置看门狗设备
 *
 * @param dev WDT设备指针
 * @param config 配置参数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_wdt_setup(lisa_device_t *dev, const lisa_wdt_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->setup ? api->setup(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启动看门狗
 *
 * @param dev WDT设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 某些硬件看门狗一旦启动无法停止，直到系统复位
 */
static inline int lisa_wdt_start(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->start ? api->start(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 停止看门狗
 *
 * @param dev WDT设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作（某些硬件不支持停止）
 * @return <0 其他错误
 *
 * @note 某些硬件看门狗一旦启动无法停止
 */
static inline int lisa_wdt_stop(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->stop ? api->stop(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 喂狗（重置看门狗计数器）
 *
 * @param dev WDT设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 必须在超时之前调用此函数，否则将触发超时行为
 */
static inline int lisa_wdt_feed(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->feed ? api->feed(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取剩余时间
 *
 * @param dev WDT设备指针
 * @param remaining_ms 输出参数，剩余时间（毫秒）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_wdt_get_remaining_time(lisa_device_t *dev, uint32_t *remaining_ms)
{
    if (!dev || !dev->api || !remaining_ms) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->get_remaining_time ? api->get_remaining_time(dev, remaining_ms) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取看门狗状态
 *
 * @param dev WDT设备指针
 * @param state 输出参数，当前状态
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_wdt_get_state(lisa_device_t *dev, lisa_wdt_state_t *state)
{
    if (!dev || !dev->api || !state) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->get_state ? api->get_state(dev, state) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置超时回调函数
 *
 * @param dev WDT设备指针
 * @param callback 回调函数指针
 * @param user_data 用户数据指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 仅当超时行为设置为 LISA_WDT_ACTION_INTERRUPT 时有效
 */
static inline int lisa_wdt_set_callback(lisa_device_t *dev, lisa_wdt_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_wdt_api_t *api = (lisa_wdt_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 超时行为支持位掩码
 */
#define LISA_WDT_ACTION_MASK_RESET      (1 << LISA_WDT_ACTION_RESET)
#define LISA_WDT_ACTION_MASK_INTERRUPT  (1 << LISA_WDT_ACTION_INTERRUPT)

#ifdef __cplusplus
}
#endif
