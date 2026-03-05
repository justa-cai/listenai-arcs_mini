/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_rgb.h
 * @brief LISA RGB (Parallel) LCD 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include "lisa_display.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * RGB LCD 设备 API
 * ======================================================================== */

typedef struct {
    /**
     * @brief 配置 RGB LCD 硬件参数
     * @param dev 设备实例
     * @param config 配置参数
     * @return 0=成功，负数=错误码
     */
    int (*setup)(lisa_device_t *dev, const lisa_display_bus_rgb_config_t *config);

    /**
     * @brief 启动 RGB 传输
     * @param dev 设备实例
     * @return 0=成功，负数=错误码
     */
    int (*start)(lisa_device_t *dev);

    /**
     * @brief 停止 RGB 传输
     * @param dev 设备实例
     * @return 0=成功，负数=错误码
     */
    int (*stop)(lisa_device_t *dev);

    /**
     * @brief 等待传输完成
     * @param dev 设备实例
     * @param timeout_ms 超时时间（毫秒）
     * @return 0=成功，负数=错误码
     */
    int (*wait_done)(lisa_device_t *dev, uint32_t timeout_ms);

    /**
     * @brief 更新帧缓冲区（单次传输模式）
     * @param dev 设备实例
     * @param framebuffer 新的帧缓冲区地址
     * @return 0=成功，负数=错误码
     */
    int (*update_framebuffer)(lisa_device_t *dev, const void *buf, uint32_t size);
} lisa_rgb_api_t;

/* ========================================================================
 * RGB LCD 辅助函数
 * ======================================================================== */

#define LISA_RGB0_NAME "rgb0"

static inline int lisa_rgb_setup(lisa_device_t *dev, const lisa_display_bus_rgb_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rgb_api_t *api = (lisa_rgb_api_t *)dev->api;
    return api->setup ? api->setup(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启动 RGB 传输
 * @param dev 设备实例
 * @return 0 = 成功，负数 = 错误码
 */
static inline int lisa_rgb_start(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rgb_api_t *api = (lisa_rgb_api_t *)dev->api;
    return api->start ? api->start(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 停止 RGB 传输
 * @param dev 设备实例
 * @return 0 = 成功，负数 = 错误码
 */
static inline int lisa_rgb_stop(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rgb_api_t *api = (lisa_rgb_api_t *)dev->api;
    return api->stop ? api->stop(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 等待 RGB 传输完成
 * @param dev 设备实例
 * @param timeout_ms 超时时间（毫秒），0 表示不等待
 * @return 0 = 成功，负数 = 错误码
 */
static inline int lisa_rgb_wait_done(lisa_device_t *dev, uint32_t timeout_ms)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rgb_api_t *api = (lisa_rgb_api_t *)dev->api;
    return api->wait_done ? api->wait_done(dev, timeout_ms) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 更新帧缓冲区（用于 Bounce Buffer 模式）
 * @param dev 设备实例
 * @param buf 帧缓冲区数据指针
 * @param size 数据大小（字节）
 * @return 0 = 成功，负数 = 错误码
 *
 * @note 此函数在 Bounce Buffer 模式下自动管理双缓冲，应用层无需关心缓冲区切换
 */
static inline int lisa_rgb_update_framebuffer(lisa_device_t *dev, const void *buf, uint32_t size) {
    if (!dev || !dev->api || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rgb_api_t *api = (lisa_rgb_api_t *)dev->api;
    return api->update_framebuffer ? api->update_framebuffer(dev, buf, size) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

#ifdef __cplusplus
}
#endif
