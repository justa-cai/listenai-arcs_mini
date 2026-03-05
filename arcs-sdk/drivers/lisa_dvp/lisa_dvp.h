/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_dvp.h
 * @brief LISA DVP 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * DVP 类型定义
 * ======================================================================== */

/**
 * @brief DVP 事件类型 (位掩码)
 */
typedef enum {
    LISA_DVP_EVENT_DONE      = (1 << 0),  /* 普通模式下一帧数据接收完成 */
    LISA_DVP_EVENT_PING_DONE = (1 << 1),  /* Ping-Pong 模式下 Ping 缓冲区接收完成 */
    LISA_DVP_EVENT_PONG_DONE = (1 << 2),  /* Ping-Pong 模式下 Pong 缓冲区接收完成 */
} lisa_dvp_event_t;

/**
 * @brief DVP 回调函数类型
 *
 * @param event DVP 事件
 * @param user_data 用户数据指针
 */
typedef void (*lisa_dvp_callback_t)(lisa_dvp_event_t event, void *user_data);

/**
 * @brief DVP 信号极性
 */
typedef enum {
    LISA_DVP_POL_FALLING = 0, /* 下降沿有效 */
    LISA_DVP_POL_RISING  = 1, /* 上升沿有效 */
} lisa_dvp_polarity_t;

/**
 * @brief DVP 数据对齐方式
 */
typedef enum {
    LISA_DVP_DATA_ALIGN_RIGHT = 0, /* 右对齐 */
    LISA_DVP_DATA_ALIGN_LEFT  = 1, /* 左对齐 */
} lisa_dvp_data_align_t;

/**
 * @brief DVP 输入数据格式
 */
typedef enum {
    LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR, /* YUV422 Y0CbY1Cr */
    LISA_DVP_INPUT_FORM_LUMINA_8BIT,     /* 8位亮度 */
} lisa_dvp_input_format_t;

/**
 * @brief DVP 应用层配置结构体
 */
typedef struct {
    uint16_t frame_width;                   /* 帧宽度 */
    uint16_t frame_height;                  /* 帧高度 */
    uint16_t pixel_offset;                  /* 像素偏移 */
    uint16_t line_offset;                   /* 行偏移 */
    lisa_dvp_input_format_t input_format;   /* 输入数据格式 */
    lisa_dvp_data_align_t data_align;       /* 数据对齐方式 */
    lisa_dvp_polarity_t vsync_polarity;     /* VSYNC 极性 */
    lisa_dvp_polarity_t hsync_polarity;     /* HSYNC 极性 */
    lisa_dvp_polarity_t pclk_polarity;      /* PCLK 极性 */
} lisa_dvp_hal_config_t;

/**
 * @brief DVP 配置结构体
 */
typedef struct {
    lisa_dvp_hal_config_t dvp_hal_config; /* DVP 应用层配置 */
    uint8_t gpdma_ch;                     /* GPDMA 通道号 */
} lisa_dvp_config_t;

/* ========================================================================
 * DVP 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*setup)(const lisa_device_t *dev, const lisa_dvp_config_t *config, lisa_dvp_callback_t callback, void *user_data);
    int (*stop)(const lisa_device_t *dev);
    int (*start)(const lisa_device_t *dev, void *buf, uint32_t len);
    int (*reload)(const lisa_device_t *dev, void *buf, uint32_t len);
    int (*start_pingpong)(const lisa_device_t *dev, void *ping_buf, void *pong_buf, uint32_t len);
    int (*reload_pingpong)(const lisa_device_t *dev, void *buf);
    int (*enable_clockout)(const lisa_device_t *dev, uint32_t clock);
} lisa_dvp_api_t;

/* ========================================================================
 * DVP 对外接口函数
 * ======================================================================== */

#define LISA_DVP0_NAME "dvp0"

/* ===== 配置接口 ===== */

/**
 * @brief 初始化 DVP 设备
 *
 * 配置并初始化 DVP 设备。
 *
 * @param dev DVP 设备指针
 * @param callback DVP 事件回调函数
 * @param user_data 用户数据
 * @param config DVP 配置参数结构体指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 初始化前请确保设备已注册
 */
static inline int lisa_dvp_setup(lisa_device_t *dev, const lisa_dvp_config_t *config, lisa_dvp_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->setup ? api->setup(dev, config, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== 控制接口 ===== */

/**
 * @brief 启动 DVP 设备
 *
 * 启动 DVP 设备开始捕获数据。
 *
 * @param dev DVP 设备指针
 * @param buf 接收数据的缓冲区
 * @param len 缓冲区长度（以字节为单位）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_dvp_start(lisa_device_t *dev, void *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->start ? api->start(dev, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启动 DVP Ping-Pong 捕获
 *
 * @param dev DVP 设备指针
 * @param ping_buf Ping 缓冲区
 * @param pong_buf Pong 缓冲区
 * @param len 单个缓冲区的长度（以字节为单位）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_dvp_start_pingpong(lisa_device_t *dev, void *ping_buf, void *pong_buf, uint32_t len)
{
    if (!dev || !dev->api || !ping_buf || !pong_buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->start_pingpong ? api->start_pingpong(dev, ping_buf, pong_buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 停止 DVP 设备
 *
 * 停止 DVP 设备捕获数据。
 *
 * @param dev DVP 设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_dvp_stop(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->stop ? api->stop(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 重新加载 DVP 缓冲区（普通模式）
 *
 * 在普通模式下，重新启动 DMA 传输到新的缓冲区。
 *
 * @param dev DVP 设备指针
 * @param buf 新的缓冲区地址
 * @param len 缓冲区大小（字节）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_dvp_reload(lisa_device_t *dev, void *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->reload ? api->reload(dev, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 重新加载 DVP Ping-Pong 缓冲区
 *
 * 在 Ping-Pong 模式下，重新加载一个缓冲区。
 *
 * @param dev DVP 设备指针
 * @param buf 新的缓冲区地址
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_dvp_reload_pingpong(lisa_device_t *dev, void *buf)
{
    if (!dev || !dev->api || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->reload_pingpong ? api->reload_pingpong(dev, buf) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启用 DVP 时钟输出
 *
 * 启用 DVP 时钟输出功能。
 *
 * @param dev DVP 设备指针
 * @param clock 时钟频率（Hz）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_dvp_enable_clockout(lisa_device_t *dev, uint32_t clock)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_dvp_api_t *api = (lisa_dvp_api_t *)dev->api;
    return api->enable_clockout ? api->enable_clockout(dev, clock) : LISA_DEVICE_ERR_NOT_SUPPORT;
}


#ifdef __cplusplus
}
#endif
