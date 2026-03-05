/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_touch_bl6133.c
 * @brief BL6133 触摸芯片驱动 - LISA Touch 设备实现
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

#define LOG_TAG "lisa_touch_bl6133"
#include <lisa_log.h>

/* BL6133 I2C 地址 */
#define BL6133_I2C_SLAVE_ADDRESS  0x2C

/* BL6133 寄存器定义 */
#define BL6XXX_POINT_REG          0x01

/* BL6133 触摸事件类型 */
enum {
    BL6XXX_TOUCH_EVENT_DOWN,
    BL6XXX_TOUCH_EVENT_UP,
    BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE,
    BL6XXX_TOUCH_EVENT_NONE,
};

/* BL6133 触摸能力 */
#define BL6133_MAX_X              320
#define BL6133_MAX_Y              386
#define BL6133_MAX_POINTS         1

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

/* ===== BL6133 设备私有数据 ===== */
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
    TaskHandle_t read_task_handle;                  /* I2C 读取任务句柄（用于中断模式） */
    bool interrupt_mode_active;                     /* 中断模式是否激活 */
} lisa_touch_bl6133_priv_t;

/* ===== BL6133 设备静态实例 ===== */
static lisa_touch_bl6133_priv_t touch_bl6133_priv;

/* ===== 内部辅助函数 ===== */

/* 前向声明 */
static int bl6133_touch_read_event_internal(lisa_touch_bl6133_priv_t *priv, lisa_touch_event_t *event);

/**
 * @brief 读取 BL6133 寄存器
 */
static int bl6133_read_reg(lisa_device_t *i2c_dev, uint8_t reg, uint8_t *data, uint32_t len)
{
    if (!i2c_dev || !data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    /* BL6133 使用 8 位寄存器地址 */
    lisa_i2c_msg_t msgs[2] = {
        {.addr = BL6133_I2C_SLAVE_ADDRESS, .flags = LISA_I2C_FLAG_NONE, .len = 1, .buf = &reg},
        {.addr = BL6133_I2C_SLAVE_ADDRESS, .flags = LISA_I2C_FLAG_READ, .len = len, .buf = data}
    };
    
    int ret = lisa_i2c_transfer(i2c_dev, msgs, 2);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "I2C transfer failed: %d", ret);
        return ret;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief I2C 读取任务（用于中断模式）
 * @note 此任务在中断模式下运行，等待任务通知后读取 I2C 数据
 */
static void bl6133_read_task(void *pvParameters)
{
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)pvParameters;
    
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
        int ret = bl6133_touch_read_event_internal(priv, &event);
        
        if (ret == LISA_DEVICE_OK && priv->callback) {
            /* 通过回调函数返回完整数据 */
            priv->callback(&event, priv->callback_user_data);
        }
    }
}

/**
 * @brief GPIO 中断回调
 */
static void bl6133_gpio_irq_callback(uint32_t pin, void *user_data)
{
    (void)pin;
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)user_data;
    
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
static int bl6133_hardware_reset(lisa_touch_bl6133_priv_t *priv)
{
    if (!priv->rst_gpio || !lisa_device_ready(priv->rst_gpio)) {
        LISA_LOGW(LOG_TAG, "Reset GPIO not available");
        return LISA_DEVICE_OK;
    }
    
    lisa_gpio_configure(priv->rst_gpio, priv->rst_pin, LISA_GPIO_OUTPUT);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 1);
    lisa_thread_mdelay(10);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 0);
    lisa_thread_mdelay(100);  /* BL6133 需要较长的低电平时间 */
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 1);
    lisa_thread_mdelay(300);
    
    LISA_LOGI(LOG_TAG, "BL6133 reset completed");
    return LISA_DEVICE_OK;
}

/* ===== LISA Touch API 实现 ===== */

/**
 * @brief 获取触摸设备能力
 */
static int bl6133_touch_get_capabilities(lisa_device_t *dev, lisa_touch_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    memset(caps, 0, sizeof(lisa_touch_capabilities_t));
    caps->max_x = BL6133_MAX_X;
    caps->max_y = BL6133_MAX_Y;
    caps->max_points = BL6133_MAX_POINTS;
    caps->has_pressure = false;
    caps->has_gesture = false;
    caps->supported_gestures = 0;
    
    return LISA_DEVICE_OK;
}

/**
 * @brief 内部读取触摸事件函数（供任务和 API 调用）
 */
static int bl6133_touch_read_event_internal(lisa_touch_bl6133_priv_t *priv, lisa_touch_event_t *event)
{
    if (!priv || !event || !priv->enabled) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    
    DEVICE_LOCK(priv);
    
    /* 读取触摸点数据（6字节） */
    uint8_t buf[6] = {0};
    int ret = bl6133_read_reg(priv->i2c_dev, BL6XXX_POINT_REG, buf, sizeof(buf));
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }
    
    /*
     * 数据格式：
     * BYTE0 | gesture_code[7:0]
     * BYTE1 | point_number[7:0]
     * BYTE2 | event[7:6]    | x_high[5:0]
     * BYTE3 | x_low[7:0]
     * BYTE4 | touch_id[7:4] | y_high[3:0]
     * BYTE5 | y_low[7:0]
     */
    uint8_t event_type = (buf[2] >> 6) & 0x03;
    bool pressed = (event_type == BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE) ? true : false;
    
    /* 注意：该芯片在触摸时总是输出 event 2 (CONTACT_OR_MOVE)，而不是 event 0 (DOWN) */
    uint16_t x = (((uint16_t)(buf[2] & 0x3F)) << 8) | buf[3];
    uint16_t y = (((uint16_t)(buf[4] & 0x0F)) << 8) | buf[5];
    uint8_t touch_id = (buf[4] >> 4) & 0x0F;
    
    memset(event, 0, sizeof(lisa_touch_event_t));
    if (pressed) {
        event->type = LISA_TOUCH_EVENT_PRESS;
        event->point_count = 1;
        event->points[0].id = (buf[4] >> 4) & 0x0F;
        event->points[0].x = x;
        event->points[0].y = y;
        event->points[0].state = LISA_TOUCH_POINT_PRESSED;
        LISA_LOGD(LOG_TAG, "Touch: x=%d, y=%d, id=%d", x, y, event->points[0].id);
    } else {
        event->type = LISA_TOUCH_EVENT_RELEASE;
        event->point_count = 0;
    }
    
    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

/**
 * @brief 读取触摸事件（API 接口）
 */
static int bl6133_touch_read_event(lisa_device_t *dev, lisa_touch_event_t *event)
{
    if (!lisa_device_is_initialized(dev) || !event) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)dev->priv_data;
    
    /* 中断模式下禁止轮询读取，避免与中断任务产生资源竞争 */
    if (priv->interrupt_mode_active) {
        LISA_LOGW(LOG_TAG, "Cannot read_event in interrupt mode, use callback instead");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    
    return bl6133_touch_read_event_internal(priv, event);
}

/**
 * @brief 启用触摸设备
 */
static int bl6133_touch_enable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->enabled = true;
    DEVICE_UNLOCK(priv);
    
    LISA_LOGI(LOG_TAG, "BL6133 touch device enabled");
    return LISA_DEVICE_OK;
}

/**
 * @brief 禁用触摸设备
 */
static int bl6133_touch_disable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->enabled = false;
    DEVICE_UNLOCK(priv);
    
    LISA_LOGI(LOG_TAG, "BL6133 touch device disabled");
    return LISA_DEVICE_OK;
}

/**
 * @brief 附加触摸总线接口
 */
static int bl6133_touch_attach_bus(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config)
{
    if (!lisa_device_is_initialized(dev) || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)dev->priv_data;
    
    if (bus_config->bus_type != LISA_TOUCH_BUS_I2C) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    
    DEVICE_LOCK(priv);
    
    priv->i2c_dev = bus_config->config.i2c.i2c_dev;
    priv->int_gpio = bus_config->config.i2c.int_gpio;
    priv->rst_gpio = bus_config->config.i2c.rst_gpio;
    priv->int_pin = bus_config->config.i2c.int_pin;
    priv->rst_pin = bus_config->config.i2c.rst_pin;
    
    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_ERR_NOT_READY;
    }
    
    lisa_i2c_config_t i2c_config = {
        .speed = LISA_I2C_SPEED_STANDARD,
        .master_mode = true,
        .slave_addr = 0,
    };
    int ret = lisa_i2c_configure(priv->i2c_dev, &i2c_config);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }
    
    ret = bl6133_hardware_reset(priv);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }
    
    if (priv->int_gpio && lisa_device_ready(priv->int_gpio)) {
        lisa_gpio_configure(priv->int_gpio, priv->int_pin, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
    }
    
    DEVICE_UNLOCK(priv);
    LISA_LOGI(LOG_TAG, "BL6133 touch bus attached");
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置触摸事件回调函数
 */
static int bl6133_touch_set_callback(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->callback = callback;
    priv->callback_user_data = user_data;
    DEVICE_UNLOCK(priv);
    
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置中断模式
 */
static int bl6133_touch_set_int_mode(lisa_device_t *dev, lisa_touch_int_mode_t mode)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_bl6133_priv_t *priv = (lisa_touch_bl6133_priv_t *)dev->priv_data;
    
    if (!priv->int_gpio || !lisa_device_ready(priv->int_gpio)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    
    DEVICE_LOCK(priv);
    int ret = LISA_DEVICE_OK;
    
    if (mode == LISA_TOUCH_INT_MODE_INTERRUPT) {
        /* 如果任务不存在，创建读取任务 */
        if (priv->read_task_handle == NULL) {
            /* 使用 Kconfig 配置的任务优先级 */
            UBaseType_t task_priority = tskIDLE_PRIORITY + CONFIG_LISA_TOUCH_ARCS_BL6133_READ_TASK_PRIORITY;
            xTaskCreate(bl6133_read_task, "bl6133_read", 1024, priv, 
                       task_priority, &priv->read_task_handle);
            if (priv->read_task_handle == NULL) {
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to create read task");
                return LISA_DEVICE_ERR_INIT_FAIL;
            }
            LISA_LOGI(LOG_TAG, "BL6133 read task created with priority %d", task_priority);
        }
        
        ret = lisa_gpio_configure_irq(priv->int_gpio, priv->int_pin,
                                     LISA_GPIO_IRQ_EDGE_FALLING,
                                     bl6133_gpio_irq_callback, priv);
        if (ret == LISA_DEVICE_OK) {
            ret = lisa_gpio_enable_irq(priv->int_gpio, priv->int_pin);
            if (ret == LISA_DEVICE_OK) {
                priv->interrupt_mode_active = true;
                LISA_LOGI(LOG_TAG, "BL6133 interrupt mode enabled");
            }
        }
    } else {
        /* 禁用中断 */
        priv->interrupt_mode_active = false;
        ret = lisa_gpio_disable_irq(priv->int_gpio, priv->int_pin);
        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "BL6133 interrupt mode disabled (polling mode)");
        }
        
        /* 注意：不删除任务，以便下次切换回中断模式时重用 */
    }
    
    DEVICE_UNLOCK(priv);
    return ret;
}

/* ===== BL6133 Touch API 实例 ===== */
static const lisa_touch_api_t bl6133_api = {
    .get_capabilities = bl6133_touch_get_capabilities,
    .read_event = bl6133_touch_read_event,
    .enable = bl6133_touch_enable,
    .disable = bl6133_touch_disable,
    .attach_bus = bl6133_touch_attach_bus,
    .set_callback = bl6133_touch_set_callback,
    .set_int_mode = bl6133_touch_set_int_mode,
};

/* ===== 设备初始化函数 ===== */

static int lisa_touch_bl6133_init(void)
{
    memset(&touch_bl6133_priv, 0, sizeof(lisa_touch_bl6133_priv_t));
    
    touch_bl6133_priv.mutex = lisa_mutex_create();
    if (!touch_bl6133_priv.mutex) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    touch_bl6133_priv.enabled = false;
    touch_bl6133_priv.initialized = true;
    
    LISA_LOGI(LOG_TAG, "BL6133 touch device initialized");
    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(touch_bl6133,
                     &bl6133_api,
                     &touch_bl6133_priv,
                     NULL,
                     lisa_touch_bl6133_init,
                     LISA_DEVICE_PRIORITY_NORMAL);
