/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_touch_cst816d.c
 * @brief CST816D 触摸芯片驱动 - LISA Touch 设备实现
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

#define LOG_TAG "lisa_touch_cst816d"
#include <lisa_log.h>

/* CST816D I2C 地址 */
#define CST816D_I2C_SLAVE_ADDRESS  0x15

/* CST816D 寄存器定义 */
#define CST816D_TOUCH_INFO_REG     0xD000

/* CST816D 触摸能力 */
#define CST816D_MAX_X              240
#define CST816D_MAX_Y              240
#define CST816D_MAX_POINTS         1

/* 自动判断上下文的互斥锁宏（支持中断和线程上下文） */
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

/* ===== CST816D 设备私有数据 ===== */
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
} lisa_touch_cst816d_priv_t;

/* ===== CST816D 设备静态实例 ===== */
static lisa_touch_cst816d_priv_t touch_cst816d_priv;

/* ===== 内部辅助函数 ===== */

/* 前向声明 */
static int cst816d_touch_read_event_internal(lisa_touch_cst816d_priv_t *priv, lisa_touch_event_t *event);

/**
 * @brief 读取 CST816D 寄存器
 */
static int cst816d_read_reg(lisa_device_t *i2c_dev, uint16_t reg, uint8_t *data, uint32_t len)
{
    if (!i2c_dev || !data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    /* 准备寄存器地址（小端序：低字节在前） */
    uint8_t reg_buf[2] = {(uint8_t)(reg & 0xFF), (uint8_t)((reg >> 8) & 0xFF)};
    
    /* 构造 I2C 消息：先写寄存器地址，再读数据 */
    lisa_i2c_msg_t msgs[2] = {
        {.addr = CST816D_I2C_SLAVE_ADDRESS, .flags = LISA_I2C_FLAG_NONE, .len = sizeof(reg_buf), .buf = reg_buf},
        {.addr = CST816D_I2C_SLAVE_ADDRESS, .flags = LISA_I2C_FLAG_READ, .len = len, .buf = data}
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
static void cst816d_read_task(void *pvParameters)
{
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)pvParameters;
    
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
        int ret = cst816d_touch_read_event_internal(priv, &event);
        
        if (ret == LISA_DEVICE_OK && priv->callback) {
            /* 通过回调函数返回完整数据 */
            priv->callback(&event, priv->callback_user_data);
        }
    }
}

/**
 * @brief GPIO 中断回调
 */
static void cst816d_gpio_irq_callback(uint32_t pin, void *user_data)
{
    (void)pin;
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)user_data;
    
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
static int cst816d_hardware_reset(lisa_touch_cst816d_priv_t *priv)
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
    lisa_thread_mdelay(20);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 0);
    lisa_thread_mdelay(20);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 1);
    lisa_thread_mdelay(400);
    
    LISA_LOGI(LOG_TAG, "CST816D hardware reset completed");
    return LISA_DEVICE_OK;
}

/* ===== LISA Touch API 实现 ===== */

/**
 * @brief 获取触摸设备能力
 */
static int cst816d_touch_get_capabilities(lisa_device_t *dev, lisa_touch_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    memset(caps, 0, sizeof(lisa_touch_capabilities_t));
    caps->max_x = CST816D_MAX_X;
    caps->max_y = CST816D_MAX_Y;
    caps->max_points = CST816D_MAX_POINTS;
    caps->has_pressure = false;
    caps->has_gesture = false;
    caps->supported_gestures = 0;
    
    return LISA_DEVICE_OK;
}

/**
 * @brief 内部读取触摸事件函数（供任务和 API 调用）
 */
static int cst816d_touch_read_event_internal(lisa_touch_cst816d_priv_t *priv, lisa_touch_event_t *event)
{
    if (!priv || !event || !priv->enabled) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    
    DEVICE_LOCK(priv);
    
    /* 读取触摸信息寄存器（6字节） */
    uint8_t read_buf[6] = {0};
    int ret = cst816d_read_reg(priv->i2c_dev, CST816D_TOUCH_INFO_REG, read_buf, sizeof(read_buf));
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }
    
    /* 解析触摸状态和坐标 */
    bool pressed = (read_buf[0] != 0);
    uint16_t x = ((read_buf[2] & 0x0F) << 8) | read_buf[3];
    uint16_t y = ((read_buf[4] & 0x0F) << 8) | read_buf[5];
    
    /* 填充事件结构体 */
    memset(event, 0, sizeof(lisa_touch_event_t));
    if (pressed) {
        event->type = LISA_TOUCH_EVENT_PRESS;
        event->point_count = 1;
        event->points[0].id = 0;
        event->points[0].x = x;
        event->points[0].y = y;
        event->points[0].state = LISA_TOUCH_POINT_PRESSED;
        LISA_LOGD(LOG_TAG, "Touch: x=%d, y=%d", x, y);
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
static int cst816d_touch_read_event(lisa_device_t *dev, lisa_touch_event_t *event)
{
    if (!lisa_device_is_initialized(dev) || !event) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)dev->priv_data;
    
    /* 中断模式下禁止轮询读取，避免与中断任务产生资源竞争 */
    if (priv->interrupt_mode_active) {
        LISA_LOGW(LOG_TAG, "Cannot read_event in interrupt mode, use callback instead");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    
    return cst816d_touch_read_event_internal(priv, event);
}

/**
 * @brief 启用触摸设备
 */
static int cst816d_touch_enable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->enabled = true;
    DEVICE_UNLOCK(priv);
    
    LISA_LOGI(LOG_TAG, "CST816D touch device enabled");
    return LISA_DEVICE_OK;
}

/**
 * @brief 禁用触摸设备
 */
static int cst816d_touch_disable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->enabled = false;
    DEVICE_UNLOCK(priv);
    
    LISA_LOGI(LOG_TAG, "CST816D touch device disabled");
    return LISA_DEVICE_OK;
}

/**
 * @brief 附加触摸总线接口
 */
static int cst816d_touch_attach_bus(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config)
{
    if (!lisa_device_is_initialized(dev) || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)dev->priv_data;
    
    if (bus_config->bus_type != LISA_TOUCH_BUS_I2C) {
        LISA_LOGE(LOG_TAG, "CST816D only supports I2C bus");
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
    ret = cst816d_hardware_reset(priv);
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
        LISA_LOGI(LOG_TAG, "CST816D interrupt pin configured");
    }
    
    DEVICE_UNLOCK(priv);
    
    LISA_LOGI(LOG_TAG, "CST816D touch bus attached successfully");
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置触摸事件回调函数
 */
static int cst816d_touch_set_callback(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->callback = callback;
    priv->callback_user_data = user_data;
    DEVICE_UNLOCK(priv);
    
    LISA_LOGD(LOG_TAG, "CST816D touch callback %s", callback ? "set" : "cleared");
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置中断模式
 */
static int cst816d_touch_set_int_mode(lisa_device_t *dev, lisa_touch_int_mode_t mode)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_touch_cst816d_priv_t *priv = (lisa_touch_cst816d_priv_t *)dev->priv_data;
    
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
            UBaseType_t task_priority = tskIDLE_PRIORITY + CONFIG_LISA_TOUCH_ARCS_CST816D_READ_TASK_PRIORITY;
            xTaskCreate(cst816d_read_task, "cst816d_read", 1024, priv, 
                       task_priority, &priv->read_task_handle);
            if (priv->read_task_handle == NULL) {
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to create read task");
                return LISA_DEVICE_ERR_INIT_FAIL;
            }
            LISA_LOGI(LOG_TAG, "CST816D read task created with priority %d", task_priority);
        }
        
        /* 配置并启用中断（下降沿触发） */
        ret = lisa_gpio_configure_irq(priv->int_gpio, priv->int_pin,
                                     LISA_GPIO_IRQ_EDGE_FALLING,
                                     cst816d_gpio_irq_callback, priv);
        if (ret == LISA_DEVICE_OK) {
            ret = lisa_gpio_enable_irq(priv->int_gpio, priv->int_pin);
            if (ret == LISA_DEVICE_OK) {
                priv->interrupt_mode_active = true;
                LISA_LOGI(LOG_TAG, "CST816D interrupt mode enabled");
            }
        }
    } else {
        /* 禁用中断 */
        priv->interrupt_mode_active = false;
        ret = lisa_gpio_disable_irq(priv->int_gpio, priv->int_pin);
        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "CST816D interrupt mode disabled (polling mode)");
        }
        
        /* 注意：不删除任务，以便下次切换回中断模式时重用 */
    }
    
    DEVICE_UNLOCK(priv);
    return ret;
}

/* ===== CST816D Touch API 实例 ===== */
static const lisa_touch_api_t cst816d_touch_api = {
    .get_capabilities = cst816d_touch_get_capabilities,
    .read_event = cst816d_touch_read_event,
    .enable = cst816d_touch_enable,
    .disable = cst816d_touch_disable,
    .attach_bus = cst816d_touch_attach_bus,
    .set_callback = cst816d_touch_set_callback,
    .set_int_mode = cst816d_touch_set_int_mode,
};

/* ===== 设备初始化函数 ===== */

static int lisa_touch_cst816d_init(void)
{
    memset(&touch_cst816d_priv, 0, sizeof(lisa_touch_cst816d_priv_t));
    
    touch_cst816d_priv.mutex = lisa_mutex_create();
    if (!touch_cst816d_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    touch_cst816d_priv.enabled = false;
    touch_cst816d_priv.initialized = true;
    
    LISA_LOGI(LOG_TAG, "CST816D touch device initialized successfully");
    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(touch_cst816d,                  /* 设备名称 */
                     &cst816d_touch_api,             /* API指针 */
                     &touch_cst816d_priv,            /* 私有数据指针 */
                     NULL,                           /* 用户数据 */
                     lisa_touch_cst816d_init,        /* 初始化函数 */
                     LISA_DEVICE_PRIORITY_NORMAL);   /* 优先级 */
