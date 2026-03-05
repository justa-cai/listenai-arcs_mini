/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_gpio.h
 * @brief LISA GPIO 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * GPIO 类型定义
 * ======================================================================== */

/**
 * @brief GPIO 引脚模式
 */
typedef enum {
    LISA_GPIO_MODE_INPUT = 0,
    LISA_GPIO_MODE_OUTPUT = 1,
} lisa_gpio_mode_t;

/**
 * @brief GPIO 电平定义
 */
#define LISA_GPIO_LOW  0 /* 低电平 */
#define LISA_GPIO_HIGH 1 /* 高电平 */

/**
 * @brief GPIO 中断触发方式
 */
typedef enum {
    LISA_GPIO_IRQ_EDGE_RISING = 0x01,  /* 上升沿触发 */
    LISA_GPIO_IRQ_EDGE_FALLING = 0x02, /* 下降沿触发 */
    LISA_GPIO_IRQ_EDGE_BOTH = 0x03,    /* 双边沿触发 */
    LISA_GPIO_IRQ_LEVEL_HIGH = 0x04,   /* 高电平触发 */
    LISA_GPIO_IRQ_LEVEL_LOW = 0x08,    /* 低电平触发 */
} lisa_gpio_irq_mode_t;

/**
 * @brief GPIO 配置标志位
 *
 * 用于配置GPIO引脚的标志位类型，可通过按位或（|）组合多个标志
 */
typedef uint32_t lisa_gpio_flags_t;

/**
 * @brief GPIO 方向标志
 */
#define LISA_GPIO_INPUT              (0 << 0)  /**< 输入模式 */
#define LISA_GPIO_OUTPUT             (1 << 0)  /**< 输出模式 */

/**
 * @brief GPIO 上下拉标志
 */
#define LISA_GPIO_PULL_UP            (1 << 1)  /**< 启用上拉 */
#define LISA_GPIO_PULL_DOWN          (1 << 2)  /**< 启用下拉 */

/**
 * @brief GPIO 去抖动标志
 */
#define LISA_GPIO_DEBOUNCE           (1 << 3)  /**< 启用去抖动 */

/**
 * @brief GPIO 初始输出电平标志（仅用于输出模式）
 */
#define LISA_GPIO_OUTPUT_INIT_LOW    (0 << 4)  /**< 输出初始化为低电平 */
#define LISA_GPIO_OUTPUT_INIT_HIGH   (1 << 4)  /**< 输出初始化为高电平 */

/**
 * @brief GPIO 标志位掩码
 */
#define LISA_GPIO_DIR_MASK           (1 << 0)  /**< 方向标志掩码 */
#define LISA_GPIO_PULL_MASK          ((1 << 1) | (1 << 2))  /**< 上下拉标志掩码 */
#define LISA_GPIO_DEBOUNCE_MASK      (1 << 3)  /**< 去抖动标志掩码 */
#define LISA_GPIO_OUTPUT_INIT_MASK   (1 << 4)  /**< 初始电平标志掩码 */

/**
 * @brief GPIO 中断回调函数类型
 *
 * @param pin 触发中断的引脚号
 * @param user_data 用户自定义数据（可传递设备指针或其他上下文）
 */
typedef void (*lisa_gpio_irq_callback_t)(uint32_t pin, void *user_data);

/* ========================================================================
 * GPIO 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*configure)(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t flags);
    int (*get_config)(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t *flags);
    int (*read_pin)(lisa_device_t *dev, uint32_t pin);
    int (*write_pin)(lisa_device_t *dev, uint32_t pin, uint32_t value);
    int (*configure_irq)(lisa_device_t *dev, uint32_t pin, lisa_gpio_irq_mode_t mode, lisa_gpio_irq_callback_t callback,
                         void *user_data);
    int (*enable_irq)(lisa_device_t *dev, uint32_t pin);
    int (*disable_irq)(lisa_device_t *dev, uint32_t pin);
} lisa_gpio_api_t;

/* ========================================================================
 * GPIO 对外接口函数
 * ======================================================================== */

/* ===== 配置接口 ===== */

/**
 * @brief 配置GPIO引脚 (使用标志位)
 *
 * 配置指定引脚的工作模式，使用标志位组合方式。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 * @param flags 配置标志位，可组合使用：
 *              方向: LISA_GPIO_INPUT | LISA_GPIO_OUTPUT
 *              上下拉: LISA_GPIO_PULL_UP | LISA_GPIO_PULL_DOWN
 *              去抖动: LISA_GPIO_DEBOUNCE
 *              初始电平: LISA_GPIO_OUTPUT_INIT_LOW | LISA_GPIO_OUTPUT_INIT_HIGH
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 配置前请确保设备已初始化
 * @note 示例: lisa_gpio_configure(dev, 5, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP)
 */
static inline int lisa_gpio_configure(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t flags)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->configure ? api->configure(dev, pin, flags) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取GPIO引脚当前配置 (使用标志位)
 *
 * 读取引脚的当前配置信息，以标志位形式返回。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 * @param flags 输出参数，用于接收配置标志位
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_gpio_get_config(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t *flags)
{
    if (!dev || !dev->api || !flags) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->get_config ? api->get_config(dev, pin, flags) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== IO操作接口 ===== */

/**
 * @brief 读取GPIO引脚电平
 *
 * 读取指定引脚的当前电平状态，可用于输入引脚或输出引脚的回读。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 *
 * @return LISA_GPIO_LOW 低电平
 * @return LISA_GPIO_HIGH 高电平
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 对于输出引脚，读取的是实际输出的电平值
 * @note 对于输入引脚，读取的是当前引脚上的电平状态
 */
static inline int lisa_gpio_read_pin(lisa_device_t *dev, uint32_t pin)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->read_pin ? api->read_pin(dev, pin) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 写GPIO引脚电平
 *
 * 设置输出引脚的电平状态。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 * @param value 电平值（LISA_GPIO_LOW=低电平，LISA_GPIO_HIGH或非0=高电平）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 引脚必须配置为输出模式才能正常工作
 * @note value参数：LISA_GPIO_LOW表示低电平，LISA_GPIO_HIGH或任何非0值表示高电平
 */
static inline int lisa_gpio_write_pin(lisa_device_t *dev, uint32_t pin, uint32_t value)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->write_pin ? api->write_pin(dev, pin, value) : LISA_DEVICE_ERR_NOT_SUPPORT;
}


/* ===== 中断管理接口 ===== */

/**
 * @brief 配置GPIO中断
 *
 * 配置引脚的中断触发方式和回调函数。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 * @param mode 中断触发模式：
 *             - LISA_GPIO_IRQ_EDGE_RISING: 上升沿触发
 *             - LISA_GPIO_IRQ_EDGE_FALLING: 下降沿触发
 *             - LISA_GPIO_IRQ_EDGE_BOTH: 双边沿触发
 *             - LISA_GPIO_IRQ_LEVEL_HIGH: 高电平触发
 *             - LISA_GPIO_IRQ_LEVEL_LOW: 低电平触发
 * @param callback 中断回调函数指针，传NULL则禁用中断
 * @param user_data 用户自定义数据，将传递给回调函数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 配置后中断默认不启用，需调用 lisa_gpio_enable_irq() 启用
 * @note 回调函数在中断上下文中执行，应尽量简短
 */
static inline int lisa_gpio_configure_irq(lisa_device_t *dev, uint32_t pin, lisa_gpio_irq_mode_t mode,
                                          lisa_gpio_irq_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->configure_irq ? api->configure_irq(dev, pin, mode, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启用GPIO中断
 *
 * 启用指定引脚的中断功能。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 启用前需先通过 lisa_gpio_configure_irq() 配置中断
 */
static inline int lisa_gpio_enable_irq(lisa_device_t *dev, uint32_t pin)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->enable_irq ? api->enable_irq(dev, pin) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 禁用GPIO中断
 *
 * 禁用指定引脚的中断功能，中断配置保持不变。
 *
 * @param dev GPIO设备指针
 * @param pin 引脚号(0-31)
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 禁用后可通过 lisa_gpio_enable_irq() 重新启用
 */
static inline int lisa_gpio_disable_irq(lisa_device_t *dev, uint32_t pin)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    return api->disable_irq ? api->disable_irq(dev, pin) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 预定义GPIO配置标志组合
 *
 * 这些宏提供常用的GPIO配置组合
 */

/* 输入模式预定义配置 */
#define LISA_GPIO_CONFIG_DEFAULT          (LISA_GPIO_INPUT)
#define LISA_GPIO_CONFIG_INPUT_PULLUP     (LISA_GPIO_INPUT | LISA_GPIO_PULL_UP)
#define LISA_GPIO_CONFIG_INPUT_PULLDOWN   (LISA_GPIO_INPUT | LISA_GPIO_PULL_DOWN)

/* 输出模式预定义配置 */
#define LISA_GPIO_CONFIG_OUTPUT_LOW       (LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW)
#define LISA_GPIO_CONFIG_OUTPUT_HIGH      (LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_HIGH)

#ifdef __cplusplus
}
#endif
