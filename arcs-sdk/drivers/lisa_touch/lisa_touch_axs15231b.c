/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_touch_axs15231b.c
 * @brief AXS15231B 触摸芯片驱动 - LISA Touch 设备实现
 */

#include "lisa_touch.h"
#include "lisa_i2c.h"
#include "lisa_gpio.h"
#include "lisa_thread.h"
#include <stddef.h>
#include <string.h>
#include <lisa_mutex.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#define LOG_TAG "lisa_touch_axs15231b"
#include <lisa_log.h>

/* AXS15231B I2C 地址 */
#define AXS15231B_I2C_SLAVE_ADDRESS    0x3B

/* AXS15231B 寄存器定义 */
#define AXS_TOUCH_GESTURE_POS   0
#define AXS_TOUCH_POINT_NUM_POS 1
#define AXS_TOUCH_EVENT_POS     2
#define AXS_TOUCH_X_H_POS       2
#define AXS_TOUCH_X_L_POS       3
#define AXS_TOUCH_ID_POS        4
#define AXS_TOUCH_Y_H_POS       4
#define AXS_TOUCH_Y_L_POS       5
#define AXS_TOUCH_WEIGHT_POS    6
#define AXS_TOUCH_AREA_POS      7

#define AXS_TOUCH_DOWN    0
#define AXS_TOUCH_UP      1
#define AXS_TOUCH_CONTACT 2

/* AXS15231B 触摸能力 */
#define AXS15231B_MAX_X                390
#define AXS15231B_MAX_Y                390
#define AXS15231B_MAX_POINTS           1

/* 自动判断上下文的互斥锁宏（lisa_mutex_lock/unlock 已自动判断上下文） */
#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_lock(priv->mutex, LISA_OS_WAIT_FOREVER);                                                       \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_unlock(priv->mutex);                                                                           \
        }                                                                                                              \
    } while (0)

/* ===== AXS15231B 设备私有数据 ===== */
typedef struct {
    lisa_device_t *i2c_dev;                         /* I2C 设备指针 */
    lisa_device_t *int_gpio;                        /* 中断 GPIO 设备指针 */
    lisa_device_t *rst_gpio;                        /* 复位 GPIO 设备指针 */
    uint32_t int_pin;                               /* 中断引脚号 */
    uint32_t rst_pin;                               /* 复位引脚号 */
    lisa_touch_callback_t callback;                 /* 触摸事件回调函数 */
    void *callback_user_data;                       /* 回调用户数据 */
    lisa_mutex_t *mutex;                            /* 互斥锁 */
    bool initialized;                               /* 初始化标志 */
    bool enabled;                                   /* 使能标志 */
    bool last_pressed;                              /* 上次按下状态 */
    TaskHandle_t read_task_handle;                  /* I2C 读取任务句柄（用于中断模式） */
    bool interrupt_mode_active;                     /* 中断模式是否激活 */
} lisa_touch_axs15231b_priv_t;

/* ===== AXS15231B 设备静态实例 ===== */
static lisa_touch_axs15231b_priv_t touch_axs15231b_priv;

/* ===== 内部辅助函数 ===== */

/* 前向声明 */
static int axs15231b_touch_read_event_internal(lisa_touch_axs15231b_priv_t *priv, lisa_touch_event_t *event);

/**
 * @brief 读取 AXS15231B 坐标
 */
static int axs15231b_read_coordinates(lisa_device_t *i2c_dev, uint16_t *x, uint16_t *y, bool *pressed)
{
    if (!i2c_dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    uint8_t read_cmd[13] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t read_buf[14] = {0};
    
    /* 发送读取命令 */
    int ret = lisa_i2c_write(i2c_dev, AXS15231B_I2C_SLAVE_ADDRESS, read_cmd, sizeof(read_cmd));
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "I2C write failed: %d", ret);
        return ret;
    }

    /* 读取数据 */
    ret = lisa_i2c_read(i2c_dev, AXS15231B_I2C_SLAVE_ADDRESS, read_buf, sizeof(read_buf));
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "I2C read failed: %d", ret);
        return ret;
    }

    uint8_t point_num = read_buf[AXS_TOUCH_POINT_NUM_POS] & 0x0F;
    /* 无效数据 */
    if (point_num != 1) {
        return LISA_DEVICE_ERR_IO;
    }

    *x = ((read_buf[AXS_TOUCH_X_H_POS] & 0x0F) << 8) + read_buf[AXS_TOUCH_X_L_POS];
    *y = ((read_buf[AXS_TOUCH_Y_H_POS] & 0x0F) << 8) + read_buf[AXS_TOUCH_Y_L_POS];
    *pressed = ((read_buf[AXS_TOUCH_EVENT_POS] >> 6) == AXS_TOUCH_UP) ? false : true;

    return LISA_DEVICE_OK;
}

/**
 * @brief I2C 读取任务（用于中断模式）
 * @note 此任务在中断模式下运行，等待任务通知后读取 I2C 数据
 */
static void axs15231b_read_task(void *pvParameters)
{
    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)pvParameters;
    
    while (1) {
        /* 等待任务通知（中断会发送通知）
         * 使用 pdFALSE 清除所有累积的通知，避免重复处理
         */
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        
        if (!priv->enabled || !priv->interrupt_mode_active) {
            continue;
        }
        
        /* 在任务上下文中读取完整的触摸数据 */
        lisa_touch_event_t event = {0};
        int ret = axs15231b_touch_read_event_internal(priv, &event);
        
        if (ret == LISA_DEVICE_OK && priv->callback) {
            /* 通过回调函数返回完整数据 */
            priv->callback(&event, priv->callback_user_data);
        }
    }
}

/**
 * @brief GPIO 中断回调
 */
static void axs15231b_gpio_irq_callback(uint32_t pin, void *user_data)
{
    (void)pin;
    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)user_data;
    
    if (!priv || !priv->interrupt_mode_active) {
        return;
    }

    /* 在中断上下文中发送任务通知，唤醒读取任务 */
    if (priv->read_task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(priv->read_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief 硬件复位芯片
 */
static int axs15231b_hardware_reset(lisa_touch_axs15231b_priv_t *priv)
{
    if (!priv->rst_gpio || !lisa_device_ready(priv->rst_gpio)) {
        LISA_LOGW(LOG_TAG, "Reset GPIO not available, skip reset");
        return LISA_DEVICE_OK;
    }

    /* 配置复位引脚为输出 */
    int ret = lisa_gpio_configure(priv->rst_gpio, priv->rst_pin, LISA_GPIO_OUTPUT);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to configure reset pin: %d", ret);
        return ret;
    }
    
    /* 复位序列：高->低->高 */
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 1);
    lisa_thread_mdelay(10);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 0);
    lisa_thread_mdelay(10);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 1);
    lisa_thread_mdelay(300);
    
    LISA_LOGI(LOG_TAG, "AXS15231B hardware reset completed");
    return LISA_DEVICE_OK;
}

/* ===== LISA Touch API 实现 ===== */

/**
 * @brief 获取触摸设备能力
 */
static int axs15231b_touch_get_capabilities(lisa_device_t *dev, lisa_touch_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memset(caps, 0, sizeof(lisa_touch_capabilities_t));
    caps->max_x = AXS15231B_MAX_X;
    caps->max_y = AXS15231B_MAX_Y;
    caps->max_points = AXS15231B_MAX_POINTS;
    caps->has_pressure = false;
    caps->has_gesture = false;
    caps->supported_gestures = 0;
    
    LISA_LOGI(LOG_TAG, "Get capabilities: max_x=%d, max_y=%d, max_points=%d",
              caps->max_x, caps->max_y, caps->max_points);
    
    return LISA_DEVICE_OK;
}

/**
 * @brief 内部读取触摸事件函数（供任务和 API 调用）
 */
static int axs15231b_touch_read_event_internal(lisa_touch_axs15231b_priv_t *priv, lisa_touch_event_t *event)
{
    if (!priv || !event || !priv->enabled) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    DEVICE_LOCK(priv);

    /* 读取触摸坐标 */
    uint16_t x, y;
    bool pressed;
    int ret = axs15231b_read_coordinates(priv->i2c_dev, &x, &y, &pressed);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    /* 过滤异常报点：滑动过程中偶现异常报点 */
    if (priv->last_pressed && pressed) {
        if (x <= 1 || y <= 1) {
            DEVICE_UNLOCK(priv);
            return LISA_DEVICE_ERR_IO;
        }
    }

    /* 填充事件结构体 */
    memset(event, 0, sizeof(lisa_touch_event_t));
    
    if (pressed) {
        event->type = LISA_TOUCH_EVENT_PRESS;
        event->point_count = 1;
        event->points[0].id = 0;
        event->points[0].x = x;
        event->points[0].y = y;
        event->points[0].state = LISA_TOUCH_POINT_PRESSED;
        event->points[0].pressure = 0;
        event->points[0].area = 0;
        
        LISA_LOGD(LOG_TAG, "Touch event: x=%d, y=%d", x, y);
    } else {
        event->type = LISA_TOUCH_EVENT_RELEASE;
        event->point_count = 0;
    }

    priv->last_pressed = pressed;

    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

/**
 * @brief 读取触摸事件（API 接口）
 */
static int axs15231b_touch_read_event(lisa_device_t *dev, lisa_touch_event_t *event)
{
    if (!lisa_device_is_initialized(dev) || !event) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)dev->priv_data;
    
    /* 中断模式下禁止轮询读取，避免与中断任务产生资源竞争 */
    if (priv->interrupt_mode_active) {
        LISA_LOGW(LOG_TAG, "Cannot read_event in interrupt mode, use callback instead");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    
    return axs15231b_touch_read_event_internal(priv, event);
}

/**
 * @brief 启用触摸设备
 */
static int axs15231b_touch_enable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)dev->priv_data;
    
    DEVICE_LOCK(priv);
    priv->enabled = true;
    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "AXS15231B touch device enabled");
    return LISA_DEVICE_OK;
}

/**
 * @brief 禁用触摸设备
 */
static int axs15231b_touch_disable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)dev->priv_data;
    
    DEVICE_LOCK(priv);
    priv->enabled = false;
    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "AXS15231B touch device disabled");
    return LISA_DEVICE_OK;
}

/**
 * @brief 附加触摸总线接口
 */
static int axs15231b_touch_attach_bus(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config)
{
    if (!lisa_device_is_initialized(dev) || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)dev->priv_data;
    
    if (bus_config->bus_type != LISA_TOUCH_BUS_I2C) {
        LISA_LOGE(LOG_TAG, "AXS15231B only supports I2C bus");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    DEVICE_LOCK(priv);

    /* 保存总线配置 */
    priv->i2c_dev = bus_config->config.i2c.i2c_dev;
    priv->int_gpio = bus_config->config.i2c.int_gpio;
    priv->rst_gpio = bus_config->config.i2c.rst_gpio;
    priv->int_pin = bus_config->config.i2c.int_pin;
    priv->rst_pin = bus_config->config.i2c.rst_pin;

    /* 检查必需的设备 */
    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "I2C device not ready");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 自动配置 I2C 总线（标准速度，主机模式） */
    lisa_i2c_config_t i2c_config = {
        .speed = LISA_I2C_SPEED_STANDARD,
        .master_mode = true,
        .slave_addr = 0,
    };
    int ret = lisa_i2c_configure(priv->i2c_dev, &i2c_config);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "I2C configuration failed: %d", ret);
        return ret;
    }
    LISA_LOGI(LOG_TAG, "I2C auto-configured (speed: %d Hz)", i2c_config.speed);

    /* 硬件复位芯片 */
    ret = axs15231b_hardware_reset(priv);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    /* 配置中断引脚（如果提供） */
    if (priv->int_gpio && lisa_device_ready(priv->int_gpio)) {
        ret = lisa_gpio_configure(priv->int_gpio, priv->int_pin,
                                LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
        if (ret != LISA_DEVICE_OK) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to configure interrupt pin: %d", ret);
            return ret;
        }
        LISA_LOGI(LOG_TAG, "AXS15231B interrupt pin configured");
    }

    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "AXS15231B touch bus attached successfully");
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置触摸事件回调函数
 */
static int axs15231b_touch_set_callback(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)dev->priv_data;
    
    DEVICE_LOCK(priv);
    priv->callback = callback;
    priv->callback_user_data = user_data;
    DEVICE_UNLOCK(priv);

    LISA_LOGD(LOG_TAG, "AXS15231B touch callback %s", callback ? "set" : "cleared");
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置中断模式
 */
static int axs15231b_touch_set_int_mode(lisa_device_t *dev, lisa_touch_int_mode_t mode)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_axs15231b_priv_t *priv = (lisa_touch_axs15231b_priv_t *)dev->priv_data;
    
    if (!priv->int_gpio || !lisa_device_ready(priv->int_gpio)) {
        LISA_LOGW(LOG_TAG, "Interrupt GPIO not available");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    DEVICE_LOCK(priv);

    int ret = LISA_DEVICE_OK;
    
    if (mode == LISA_TOUCH_INT_MODE_INTERRUPT) {
        /* 如果任务不存在，创建读取任务 */
        if (priv->read_task_handle == NULL) {
            /* 使用 Kconfig 配置的任务优先级 */
            UBaseType_t task_priority = tskIDLE_PRIORITY + CONFIG_LISA_TOUCH_ARCS_AXS15231B_READ_TASK_PRIORITY;
            xTaskCreate(axs15231b_read_task, "axs15231b_read", 1024, priv, 
                       task_priority, &priv->read_task_handle);
            if (priv->read_task_handle == NULL) {
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to create read task");
                return LISA_DEVICE_ERR_INIT_FAIL;
            }
            LISA_LOGI(LOG_TAG, "AXS15231B read task created with priority %d", task_priority);
        }
        
        /* 配置并启用中断 (下降沿触发) */
        ret = lisa_gpio_configure_irq(priv->int_gpio, priv->int_pin,
                                     LISA_GPIO_IRQ_EDGE_FALLING,
                                     axs15231b_gpio_irq_callback, priv);
        if (ret == LISA_DEVICE_OK) {
            ret = lisa_gpio_enable_irq(priv->int_gpio, priv->int_pin);
            if (ret == LISA_DEVICE_OK) {
                priv->interrupt_mode_active = true;
                LISA_LOGI(LOG_TAG, "AXS15231B interrupt mode enabled");
            }
        }
    } else {
        /* 禁用中断 */
        priv->interrupt_mode_active = false;
        ret = lisa_gpio_disable_irq(priv->int_gpio, priv->int_pin);
        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "AXS15231B interrupt mode disabled (polling mode)");
        }
        
        /* 注意：不删除任务，以便下次切换回中断模式时重用 */
    }

    DEVICE_UNLOCK(priv);
    return ret;
}

/* ===== AXS15231B Touch API 实例 ===== */
static const lisa_touch_api_t axs15231b_touch_api = {
    .get_capabilities = axs15231b_touch_get_capabilities,
    .read_event = axs15231b_touch_read_event,
    .enable = axs15231b_touch_enable,
    .disable = axs15231b_touch_disable,
    .attach_bus = axs15231b_touch_attach_bus,
    .set_callback = axs15231b_touch_set_callback,
    .set_int_mode = axs15231b_touch_set_int_mode,
};

/* ===== 设备初始化函数 ===== */

static int lisa_touch_axs15231b_init(void)
{
    memset(&touch_axs15231b_priv, 0, sizeof(lisa_touch_axs15231b_priv_t));

    touch_axs15231b_priv.mutex = lisa_mutex_create();
    if (!touch_axs15231b_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    touch_axs15231b_priv.enabled = false;
    touch_axs15231b_priv.initialized = true;
    touch_axs15231b_priv.last_pressed = false;

    LISA_LOGI(LOG_TAG, "AXS15231B touch device initialized successfully");
    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(touch_axs15231b,              /* 设备名称 */
                     &axs15231b_touch_api,         /* API指针 */
                     &touch_axs15231b_priv,        /* 私有数据指针 */
                     NULL,                         /* 用户数据 */
                     lisa_touch_axs15231b_init,    /* 初始化函数 */
                     LISA_DEVICE_PRIORITY_NORMAL); /* 优先级 */



