/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_touch.h
 * @brief LISA Touch 触摸设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Touch 类型定义
 * ======================================================================== */

/**
 * @brief Touch 总线接口类型
 */
typedef enum {
    LISA_TOUCH_BUS_I2C = 0,      /* I2C 接口 */
} lisa_touch_bus_type_t;

/**
 * @brief Touch 总线配置（I2C）
 */
typedef struct {
    lisa_device_t *i2c_dev;      /* I2C设备指针 */
    lisa_device_t *int_gpio;     /* 中断信号的GPIO设备（可选）*/
    uint32_t int_pin;            /* 中断引脚号 */
    lisa_device_t *rst_gpio;     /* 复位信号的GPIO设备（可选）*/
    uint32_t rst_pin;            /* 复位引脚号 */
} lisa_touch_bus_i2c_config_t;

/**
 * @brief Touch 总线配置联合体
 */
typedef union {
    lisa_touch_bus_i2c_config_t i2c;
} lisa_touch_bus_config_u;

/**
 * @brief Touch 总线配置结构体
 */
typedef struct {
    lisa_touch_bus_type_t bus_type;     /* 总线类型 */
    lisa_touch_bus_config_u config;     /* 总线配置 */
} lisa_touch_bus_config_t;

/**
 * @brief Touch 点状态
 */
typedef enum {
    LISA_TOUCH_POINT_RELEASED = 0,   /* 释放 */
    LISA_TOUCH_POINT_PRESSED = 1,    /* 按下 */
    LISA_TOUCH_POINT_CONTACT = 2,    /* 持续接触 */
} lisa_touch_point_state_t;

/**
 * @brief Touch 触摸点数据
 */
typedef struct {
    uint16_t x;                      /* X 坐标 */
    uint16_t y;                      /* Y 坐标 */
    uint8_t id;                      /* 触摸点ID（用于多点触摸） */
    lisa_touch_point_state_t state;  /* 触摸点状态 */
    uint16_t pressure;               /* 压力值（可选，如果硬件支持）*/
    uint16_t area;                   /* 接触面积（可选，如果硬件支持）*/
} lisa_touch_point_t;

/**
 * @brief Touch 事件类型
 */
typedef enum {
    LISA_TOUCH_EVENT_NONE = 0,       /* 无事件 */
    LISA_TOUCH_EVENT_PRESS = 1,      /* 按下事件 */
    LISA_TOUCH_EVENT_RELEASE = 2,    /* 释放事件 */
    LISA_TOUCH_EVENT_CONTACT = 3,    /* 持续接触事件 */
    LISA_TOUCH_EVENT_GESTURE = 4,    /* 手势事件 */
} lisa_touch_event_type_t;

/**
 * @brief Touch 手势类型
 */
typedef enum {
    LISA_TOUCH_GESTURE_NONE = 0,         /* 无手势 */
    LISA_TOUCH_GESTURE_SWIPE_UP = 1,     /* 向上滑动 */
    LISA_TOUCH_GESTURE_SWIPE_DOWN = 2,   /* 向下滑动 */
    LISA_TOUCH_GESTURE_SWIPE_LEFT = 3,   /* 向左滑动 */
    LISA_TOUCH_GESTURE_SWIPE_RIGHT = 4,  /* 向右滑动 */
    LISA_TOUCH_GESTURE_PINCH_IN = 5,     /* 缩小 */
    LISA_TOUCH_GESTURE_PINCH_OUT = 6,    /* 放大 */
    LISA_TOUCH_GESTURE_ROTATE_CW = 7,    /* 顺时针旋转 */
    LISA_TOUCH_GESTURE_ROTATE_CCW = 8,   /* 逆时针旋转 */
} lisa_touch_gesture_t;

/**
 * @brief Touch 事件数据
 */
typedef struct {
    lisa_touch_event_type_t type;    /* 事件类型 */
    uint8_t point_count;             /* 触摸点数量 */
    lisa_touch_point_t points[10];   /* 触摸点数据（支持最多10点触摸）*/
    lisa_touch_gesture_t gesture;    /* 手势类型（仅当事件类型为GESTURE时有效）*/
} lisa_touch_event_t;

/**
 * @brief Touch 能力结构体
 */
typedef struct {
    uint16_t max_x;                  /* 最大 X 坐标 */
    uint16_t max_y;                  /* 最大 Y 坐标 */
    uint8_t max_points;              /* 最大触摸点数 */
    bool has_pressure;               /* 是否支持压力检测 */
    bool has_gesture;                /* 是否支持手势识别 */
    uint32_t supported_gestures;     /* 支持的手势位掩码 */
} lisa_touch_capabilities_t;

/**
 * @brief Touch 事件回调函数类型
 *
 * @param event 触摸事件数据
 * @param user_data 用户数据指针
 *
 * @note 如果需要访问设备，可以通过 user_data 传入设备指针
 */
typedef void (*lisa_touch_callback_t)(const lisa_touch_event_t *event, void *user_data);

/**
 * @brief Touch 中断模式
 */
typedef enum {
    LISA_TOUCH_INT_MODE_POLLING = 0,     /* 轮询模式 */
    LISA_TOUCH_INT_MODE_INTERRUPT = 1,   /* 中断模式 */
} lisa_touch_int_mode_t;

/* ========================================================================
 * Touch 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*get_capabilities)(lisa_device_t *dev, lisa_touch_capabilities_t *caps);
    int (*read_event)(lisa_device_t *dev, lisa_touch_event_t *event);
    int (*enable)(lisa_device_t *dev);
    int (*disable)(lisa_device_t *dev);
    int (*attach_bus)(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config);
    int (*set_callback)(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data);
    int (*set_int_mode)(lisa_device_t *dev, lisa_touch_int_mode_t mode);
} lisa_touch_api_t;

/* ========================================================================
 * Touch 对外接口函数
 * ======================================================================== */

/**
 * @brief 获取触摸设备能力
 *
 * @param dev Touch设备指针
 * @param caps 输出参数，用于接收设备能力信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_get_capabilities(lisa_device_t *dev, lisa_touch_capabilities_t *caps)
{
    if (!dev || !dev->api || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->get_capabilities ? api->get_capabilities(dev, caps) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 读取触摸事件
 *
 * @param dev Touch设备指针
 * @param event 输出参数，用于接收触摸事件数据
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_read_event(lisa_device_t *dev, lisa_touch_event_t *event)
{
    if (!dev || !dev->api || !event) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->read_event ? api->read_event(dev, event) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启用触摸设备
 *
 * @param dev Touch设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_enable(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->enable ? api->enable(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 禁用触摸设备
 *
 * @param dev Touch设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_disable(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->disable ? api->disable(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 附加触摸总线接口
 *
 * 用于配置触摸总线接口（I2C/SPI等）及相关引脚。
 *
 * @param dev Touch设备指针
 * @param bus_config 总线配置结构体
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_attach_bus(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config)
{
    if (!dev || !dev->api || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->attach_bus ? api->attach_bus(dev, bus_config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置触摸事件回调函数
 *
 * @param dev Touch设备指针
 * @param callback 回调函数指针
 * @param user_data 用户数据指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_set_callback(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置中断模式
 *
 * @param dev Touch设备指针
 * @param mode 中断模式
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_touch_set_int_mode(lisa_device_t *dev, lisa_touch_int_mode_t mode)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_touch_api_t *api = (lisa_touch_api_t *)dev->api;
    return api->set_int_mode ? api->set_int_mode(dev, mode) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 手势支持位掩码
 */
#define LISA_TOUCH_GESTURE_MASK_SWIPE_UP      (1 << LISA_TOUCH_GESTURE_SWIPE_UP)
#define LISA_TOUCH_GESTURE_MASK_SWIPE_DOWN    (1 << LISA_TOUCH_GESTURE_SWIPE_DOWN)
#define LISA_TOUCH_GESTURE_MASK_SWIPE_LEFT    (1 << LISA_TOUCH_GESTURE_SWIPE_LEFT)
#define LISA_TOUCH_GESTURE_MASK_SWIPE_RIGHT   (1 << LISA_TOUCH_GESTURE_SWIPE_RIGHT)
#define LISA_TOUCH_GESTURE_MASK_PINCH_IN      (1 << LISA_TOUCH_GESTURE_PINCH_IN)
#define LISA_TOUCH_GESTURE_MASK_PINCH_OUT     (1 << LISA_TOUCH_GESTURE_PINCH_OUT)
#define LISA_TOUCH_GESTURE_MASK_ROTATE_CW     (1 << LISA_TOUCH_GESTURE_ROTATE_CW)
#define LISA_TOUCH_GESTURE_MASK_ROTATE_CCW    (1 << LISA_TOUCH_GESTURE_ROTATE_CCW)

#ifdef __cplusplus
}
#endif
