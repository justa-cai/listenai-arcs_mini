/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_touch_chsc6540.c
 * @brief CHSC6540 触摸芯片驱动 - LISA Touch 设备实现
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

#define LOG_TAG "lisa_touch_chsc6540"
#include <lisa_log.h>

#define CHSC6540_I2C_SLAVE_ADDRESS  0x2E
#define CHSC6540_REG_TOUCH_DATA     0x00
#define CHSC6540_REG_CHIP_ID        0xA7
#define CHSC6540_TOUCH_DATA_LEN     15

#define CHSC6540_EVENT_PRESS_DOWN   0x00
#define CHSC6540_EVENT_LIFT_UP      0x01
#define CHSC6540_EVENT_CONTACT      0x02

#define CHSC6540_MAX_X              CONFIG_LISA_TOUCH_ARCS_CHSC6540_MAX_X
#define CHSC6540_MAX_Y              CONFIG_LISA_TOUCH_ARCS_CHSC6540_MAX_Y
#define CHSC6540_MAX_POINTS         1

#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if ((priv)->mutex) {                                                                                           \
            lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER);                                                       \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if ((priv)->mutex) {                                                                                           \
            lisa_mutex_unlock((priv)->mutex);                                                                           \
        }                                                                                                              \
    } while (0)

typedef struct {
    lisa_device_t *i2c_dev;
    lisa_device_t *int_gpio;
    lisa_device_t *rst_gpio;
    uint32_t int_pin;
    uint32_t rst_pin;
    lisa_touch_callback_t callback;
    void *callback_user_data;
    lisa_mutex_t *mutex;
    bool initialized;
    bool enabled;
    TaskHandle_t read_task_handle;
    bool interrupt_mode_active;
    uint8_t chip_id;
} lisa_touch_chsc6540_priv_t;

static lisa_touch_chsc6540_priv_t touch_chsc6540_priv;

static int chsc6540_touch_read_event_internal(lisa_touch_chsc6540_priv_t *priv, lisa_touch_event_t *event);

static int chsc6540_read_reg(lisa_device_t *i2c_dev, uint8_t reg, uint8_t *data, uint32_t len)
{
    if (!i2c_dev || !data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    uint8_t reg_buf = reg;
    lisa_i2c_msg_t msgs[2] = {
        {.addr = CHSC6540_I2C_SLAVE_ADDRESS, .flags = LISA_I2C_FLAG_NONE, .len = sizeof(reg_buf), .buf = &reg_buf},
        {.addr = CHSC6540_I2C_SLAVE_ADDRESS, .flags = LISA_I2C_FLAG_READ, .len = len, .buf = data},
    };

    int ret = lisa_i2c_transfer(i2c_dev, msgs, 2);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "I2C transfer failed: %d", ret);
        return ret;
    }
    return LISA_DEVICE_OK;
}

static void chsc6540_read_task(void *pvParameters)
{
    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)pvParameters;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!priv->enabled || !priv->interrupt_mode_active) {
            continue;
        }

        lisa_touch_event_t event = {0};
        int ret = chsc6540_touch_read_event_internal(priv, &event);
        if (ret == LISA_DEVICE_OK && priv->callback) {
            priv->callback(&event, priv->callback_user_data);
        }
    }
}

static void chsc6540_gpio_irq_callback(uint32_t pin, void *user_data)
{
    (void)pin;
    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)user_data;

    if (!priv || !priv->interrupt_mode_active || priv->read_task_handle == NULL) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(priv->read_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static int chsc6540_hardware_reset(lisa_touch_chsc6540_priv_t *priv)
{
    if (!priv->rst_gpio || !lisa_device_ready(priv->rst_gpio)) {
        LISA_LOGW(LOG_TAG, "Reset GPIO not available, skip reset");
        return LISA_DEVICE_OK;
    }

    int ret = lisa_gpio_configure(priv->rst_gpio, priv->rst_pin, LISA_GPIO_OUTPUT);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to configure reset pin: %d", ret);
        return ret;
    }

    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 0);
    lisa_thread_mdelay(200);
    lisa_gpio_write_pin(priv->rst_gpio, priv->rst_pin, 1);
    lisa_thread_mdelay(200);

    LISA_LOGI(LOG_TAG, "CHSC6540 hardware reset completed");
    return LISA_DEVICE_OK;
}

static int chsc6540_touch_get_capabilities(lisa_device_t *dev, lisa_touch_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    memset(caps, 0, sizeof(*caps));
    caps->max_x = CHSC6540_MAX_X;
    caps->max_y = CHSC6540_MAX_Y;
    caps->max_points = CHSC6540_MAX_POINTS;
    caps->has_pressure = false;
    caps->has_gesture = false;
    caps->supported_gestures = 0;

    return LISA_DEVICE_OK;
}

static int chsc6540_touch_read_event_internal(lisa_touch_chsc6540_priv_t *priv, lisa_touch_event_t *event)
{
    if (!priv || !event || !priv->enabled) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    DEVICE_LOCK(priv);

    uint8_t buf[CHSC6540_TOUCH_DATA_LEN] = {0};
    int ret = chsc6540_read_reg(priv->i2c_dev, CHSC6540_REG_TOUCH_DATA, buf, sizeof(buf));
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    uint8_t point_num = buf[2] & 0x0F;
    if (point_num > CHSC6540_MAX_POINTS) {
        point_num = CHSC6540_MAX_POINTS;
    }

    memset(event, 0, sizeof(*event));
    if (point_num > 0) {
        uint8_t evt = (buf[3] >> 4) & 0x0F;
        uint16_t x = (((uint16_t)(buf[3] & 0x0F)) << 8) | buf[4];
        uint16_t y = (((uint16_t)(buf[5] & 0x0F)) << 8) | buf[6];

        if (evt == CHSC6540_EVENT_LIFT_UP) {
            event->type = LISA_TOUCH_EVENT_RELEASE;
            event->point_count = 0;
            LISA_LOGD(LOG_TAG, "Touch release: x=%d, y=%d", x, y);
        } else {
            event->type = LISA_TOUCH_EVENT_PRESS;
            event->point_count = 1;
            event->points[0].id = 0;
            event->points[0].x = x;
            event->points[0].y = y;
            event->points[0].state = LISA_TOUCH_POINT_PRESSED;
            LISA_LOGD(LOG_TAG, "Touch press: evt=%d, x=%d, y=%d", evt, x, y);
        }
    } else {
        event->type = LISA_TOUCH_EVENT_RELEASE;
        event->point_count = 0;
    }

    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

static int chsc6540_touch_read_event(lisa_device_t *dev, lisa_touch_event_t *event)
{
    if (!lisa_device_is_initialized(dev) || !event) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    if (priv->interrupt_mode_active) {
        LISA_LOGW(LOG_TAG, "Cannot read_event in interrupt mode, use callback instead");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    return chsc6540_touch_read_event_internal(priv, event);
}

static int chsc6540_touch_enable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->enabled = true;
    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "CHSC6540 touch device enabled");
    return LISA_DEVICE_OK;
}

static int chsc6540_touch_disable(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->enabled = false;
    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "CHSC6540 touch device disabled");
    return LISA_DEVICE_OK;
}

static int chsc6540_touch_attach_bus(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config)
{
    if (!lisa_device_is_initialized(dev) || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (bus_config->bus_type != LISA_TOUCH_BUS_I2C) {
        LISA_LOGE(LOG_TAG, "CHSC6540 only supports I2C bus");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);

    priv->i2c_dev = bus_config->config.i2c.i2c_dev;
    priv->int_gpio = bus_config->config.i2c.int_gpio;
    priv->rst_gpio = bus_config->config.i2c.rst_gpio;
    priv->int_pin = bus_config->config.i2c.int_pin;
    priv->rst_pin = bus_config->config.i2c.rst_pin;

    if (!priv->i2c_dev || !lisa_device_ready(priv->i2c_dev)) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "I2C device not ready");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_i2c_config_t i2c_config = {
        .speed = LISA_I2C_SPEED_FAST,
        .master_mode = true,
        .slave_addr = 0,
    };
    int ret = lisa_i2c_configure(priv->i2c_dev, &i2c_config);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "I2C configuration failed: %d", ret);
        return ret;
    }

    ret = chsc6540_hardware_reset(priv);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    if (priv->int_gpio && lisa_device_ready(priv->int_gpio)) {
        ret = lisa_gpio_configure(priv->int_gpio, priv->int_pin, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
        if (ret != LISA_DEVICE_OK) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to configure interrupt pin: %d", ret);
            return ret;
        }
    }

    uint8_t chip_id = 0;
    ret = chsc6540_read_reg(priv->i2c_dev, CHSC6540_REG_CHIP_ID, &chip_id, sizeof(chip_id));
    if (ret == LISA_DEVICE_OK) {
        priv->chip_id = chip_id;
        LISA_LOGI(LOG_TAG, "CHSC6540 chip id=0x%x", chip_id);
    } else {
        LISA_LOGW(LOG_TAG, "Failed to read chip id: %d", ret);
    }

    DEVICE_UNLOCK(priv);
    LISA_LOGI(LOG_TAG, "CHSC6540 touch bus attached successfully");
    return LISA_DEVICE_OK;
}

static int chsc6540_touch_set_callback(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    DEVICE_LOCK(priv);
    priv->callback = callback;
    priv->callback_user_data = user_data;
    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

static int chsc6540_touch_set_int_mode(lisa_device_t *dev, lisa_touch_int_mode_t mode)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    if (!priv->int_gpio || !lisa_device_ready(priv->int_gpio)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    DEVICE_LOCK(priv);
    int ret = LISA_DEVICE_OK;

    if (mode == LISA_TOUCH_INT_MODE_INTERRUPT) {
        if (priv->read_task_handle == NULL) {
            UBaseType_t task_priority = tskIDLE_PRIORITY + CONFIG_LISA_TOUCH_ARCS_CHSC6540_READ_TASK_PRIORITY;
            xTaskCreate(chsc6540_read_task, "chsc6540_read", 1024, priv, task_priority, &priv->read_task_handle);
            if (priv->read_task_handle == NULL) {
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to create read task");
                return LISA_DEVICE_ERR_INIT_FAIL;
            }
        }

        ret = lisa_gpio_configure_irq(priv->int_gpio, priv->int_pin,
                                      LISA_GPIO_IRQ_EDGE_FALLING,
                                      chsc6540_gpio_irq_callback, priv);
        if (ret == LISA_DEVICE_OK) {
            ret = lisa_gpio_enable_irq(priv->int_gpio, priv->int_pin);
            if (ret == LISA_DEVICE_OK) {
                priv->interrupt_mode_active = true;
                LISA_LOGI(LOG_TAG, "CHSC6540 interrupt mode enabled");
            }
        }
    } else {
        priv->interrupt_mode_active = false;
        ret = lisa_gpio_disable_irq(priv->int_gpio, priv->int_pin);
        if (ret == LISA_DEVICE_OK) {
            LISA_LOGI(LOG_TAG, "CHSC6540 interrupt mode disabled");
        }
    }

    DEVICE_UNLOCK(priv);
    return ret;
}

static int chsc6540_touch_read_chip_id(lisa_device_t *dev, uint32_t *chip_id)
{
    if (!lisa_device_is_initialized(dev) || !chip_id) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_touch_chsc6540_priv_t *priv = (lisa_touch_chsc6540_priv_t *)dev->priv_data;
    *chip_id = priv->chip_id;
    return LISA_DEVICE_OK;
}

static const lisa_touch_api_t chsc6540_api = {
    .get_capabilities = chsc6540_touch_get_capabilities,
    .read_event = chsc6540_touch_read_event,
    .enable = chsc6540_touch_enable,
    .disable = chsc6540_touch_disable,
    .attach_bus = chsc6540_touch_attach_bus,
    .set_callback = chsc6540_touch_set_callback,
    .set_int_mode = chsc6540_touch_set_int_mode,
    .read_chip_id = chsc6540_touch_read_chip_id,
};

static int lisa_touch_chsc6540_init(void)
{
    memset(&touch_chsc6540_priv, 0, sizeof(touch_chsc6540_priv));

    touch_chsc6540_priv.mutex = lisa_mutex_create();
    if (!touch_chsc6540_priv.mutex) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    touch_chsc6540_priv.enabled = false;
    touch_chsc6540_priv.initialized = true;

    LISA_LOGI(LOG_TAG, "CHSC6540 touch device initialized");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止并释放 CHSC6540 触摸设备的全部软硬件资源，恢复上电初始状态
 *
 * 由 lisa_device_destroy() 调用：关中断 → 删读取任务 → 删 mutex → memset 归零
 * （i2c/gpio 为外部设备引用、非本驱动持有）。
 */
static int lisa_touch_chsc6540_deinit(void)
{
    lisa_touch_chsc6540_priv_t *priv = &touch_chsc6540_priv;

    if (priv->interrupt_mode_active && priv->int_gpio) {
        lisa_gpio_disable_irq(priv->int_gpio, priv->int_pin);
    }
    if (priv->read_task_handle) {
        vTaskDelete(priv->read_task_handle);
    }
    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&touch_chsc6540_priv, 0, sizeof(lisa_touch_chsc6540_priv_t));
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER_DEINIT(touch_chsc6540,
                            &chsc6540_api,
                            &touch_chsc6540_priv,
                            NULL,
                            lisa_touch_chsc6540_init,
                            lisa_touch_chsc6540_deinit,
                            LISA_DEVICE_LEVEL_NORMAL,
                            LISA_DEVICE_PRIORITY_NORMAL);
