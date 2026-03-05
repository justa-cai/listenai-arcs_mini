/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_display_panel.h"
#include "lisa_display_bus.h"
#include "lisa_gpio.h"
#include <lisa_mutex.h>
#include <lisa_mem.h>
#include <lisa_semaphore.h>

#define LOG_TAG "lisa_display"
#include <lisa_log.h>

typedef struct {
    lisa_mutex_t *mutex;
    lisa_display_panel_t *panel;
    lisa_device_t *te_gpio;
    uint32_t te_pin;
    lisa_semaphore_t *te_sync_sem;
} lisa_display_priv_t;

#define DEVICE_LOCK(priv) if ((priv)->mutex) { lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER); }
#define DEVICE_UNLOCK(priv) if ((priv)->mutex) { lisa_mutex_unlock((priv)->mutex); }

static void te_gpio_irq_handler(uint32_t pin, void *arg)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)arg;
    if (!priv || !priv->te_sync_sem) {
        return;
    }

    lisa_semaphore_give(priv->te_sync_sem);
}

static void arcs_configure_te(lisa_display_priv_t *priv)
{
    if (!priv || !priv->te_gpio || !priv->te_sync_sem) {
        return;
    }

	lisa_gpio_configure(priv->te_gpio, priv->te_pin, LISA_GPIO_INPUT);
    lisa_gpio_configure_irq(priv->te_gpio, priv->te_pin, LISA_GPIO_IRQ_EDGE_RISING, te_gpio_irq_handler, priv);
    lisa_gpio_enable_irq(priv->te_gpio, priv->te_pin);
}

static int arcs_attach_bus(const lisa_device_t *dev, const lisa_display_config_t *config)
{
    if (!dev || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_t *panel = priv->panel;

    lisa_device_t *bus_dev = NULL;
    if (config->bus_type == LISA_DISPLAY_BUS_QSPI) {
        bus_dev = lisa_device_get("panel_bus_qspi");
    }
    else if (config->bus_type == LISA_DISPLAY_BUS_SPI_4WIRE) {
        bus_dev = lisa_device_get("panel_bus_spi_4wire");
    }
    else if (config->bus_type == LISA_DISPLAY_BUS_RGB) {
        bus_dev = lisa_device_get("panel_bus_rgb");
    }
    else {
        LISA_LOGE(LOG_TAG, "Unsupported bus type: %d", config->bus_type);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    if (!bus_dev) {
        LISA_LOGE(LOG_TAG, "Failed to get bus device");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    panel->bus_dev = bus_dev;
    lisa_display_bus_api_t *bus_api = (lisa_display_bus_api_t *)bus_dev->api;
    if (!bus_api || !bus_api->attach) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    lisa_display_panel_driver_t *driver = (lisa_display_panel_driver_t *)panel->panel_dev->api;
    if (!driver) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    panel->rst_gpio  = config->rst_gpio;
    panel->rst_pin   = config->rst_pin;
    panel->backlight = config->backlight;

    /* 配置命令总线(如果指定了独立命令总线) */
    if (config->cmd_bus_type != LISA_DISPLAY_CMD_BUS_NONE) {
        const lisa_display_cmd_bus_api_t *cmd_bus_api = NULL;
        lisa_device_t *cmd_bus_dev = NULL;

        /* 根据命令总线类型获取对应的 API */
        switch (config->cmd_bus_type) {
        case LISA_DISPLAY_CMD_BUS_SW_SPI:
            cmd_bus_dev = lisa_device_get("cmd_bus_sw_spi");
            if (!cmd_bus_dev) {
                LOGE("Failed to get cmd bus device");
                return LISA_DEVICE_ERR_INIT_FAIL;
            }
            cmd_bus_api = (lisa_display_cmd_bus_api_t *)cmd_bus_dev->api;
            break;
        default:
            LOGE("Unsupported cmd_bus_type: %d", config->cmd_bus_type);
            return LISA_DEVICE_ERR_NOT_SUPPORT;
        }

        if (cmd_bus_api && cmd_bus_api->configure) {
            /* 调用配置接口初始化命令总线 */
            int ret = cmd_bus_api->configure(config->cmd_bus_type, &config->cmd_bus_config);
            if (ret != LISA_DEVICE_OK) {
                LOGE("Failed to configure command bus: %d", ret);
                return ret;
            }
            /* 保存命令总线设备到 Panel（关键：自动路由依赖此指针）*/
            panel->cmd_bus_dev = cmd_bus_dev;
            LOGI("Command bus configured successfully");
        }
    }

    if (panel->rst_gpio) {
        lisa_gpio_configure(panel->rst_gpio, panel->rst_pin, LISA_GPIO_CONFIG_OUTPUT_LOW);
    }

    if (config->te_gpio) {
        priv->te_sync_sem = lisa_semaphore_create(1);
        if (!priv->te_sync_sem) {
            lisa_mem_free(panel);
            return LISA_DEVICE_ERR_NO_MEM;
        }
        priv->te_gpio = config->te_gpio;
        priv->te_pin = config->te_pin;
        arcs_configure_te(priv);
    }

    int ret = bus_api->attach(panel->bus_dev, config->bus_type, &config->bus_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Bus attach failed: %d", ret);
        if (priv->te_sync_sem) {
            lisa_semaphore_delete(priv->te_sync_sem);
            priv->te_sync_sem = NULL;
        }
        lisa_mem_free(panel);
        return ret;
    }

    if (driver->init) {
        ret = driver->init(panel);
        if (ret != LISA_DEVICE_OK) {
            if (priv->te_sync_sem) {
                lisa_semaphore_delete(priv->te_sync_sem);
                priv->te_sync_sem = NULL;
            }
            lisa_mem_free(panel);
        }
        return ret;
    }

    return LISA_DEVICE_OK;
}

static int arcs_display_write(lisa_device_t *dev, uint16_t x, uint16_t y, const lisa_display_buffer_desc_t *desc, const void *buf)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->write) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->te_sync_sem) {
        if (lisa_semaphore_take(priv->te_sync_sem, 100) != LISA_OK) {
            LISA_LOGW(LOG_TAG, "TE signal not received in time");
        }
    }

    DEVICE_LOCK(priv);
    int ret = driver->write(priv->panel, x, y, desc, buf);
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_get_capabilities(lisa_device_t *dev, lisa_display_capabilities_t *caps)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->get_capabilities) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    return driver->get_capabilities(priv->panel, caps);
}

static int arcs_set_orientation(lisa_device_t *dev, lisa_display_orientation_t orientation)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->set_orientation) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (orientation != LISA_DISPLAY_ORIENTATION_0 && orientation != LISA_DISPLAY_ORIENTATION_90
        && orientation != LISA_DISPLAY_ORIENTATION_270) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    DEVICE_LOCK(priv);
    int ret = driver->set_orientation(priv->panel, orientation);
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_blanking_on(lisa_device_t *dev)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->blanking_on) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    DEVICE_LOCK(priv);
    int ret = driver->blanking_on(priv->panel);
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_blanking_off(lisa_device_t *dev)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->blanking_off) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    DEVICE_LOCK(priv);
    int ret = driver->blanking_off(priv->panel);
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_set_brightness(lisa_device_t *dev, uint8_t brightness)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->set_brightness) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    DEVICE_LOCK(priv);
    int ret = driver->set_brightness(priv->panel, brightness);
    DEVICE_UNLOCK(priv);
    return ret;
}

static lisa_display_priv_t arcs_display_priv;

static int lisa_display_device_init(void)
{
    memset(&arcs_display_priv, 0, sizeof(arcs_display_priv));

    lisa_device_t *panel_dev = lisa_device_get("lcd_panel");
    if (!panel_dev) {
        LISA_LOGE(LOG_TAG, "Failed to get panel device");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    arcs_display_priv.mutex = lisa_mutex_create();
    if (!arcs_display_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }


    arcs_display_priv.panel = lisa_mem_alloc(sizeof(lisa_display_panel_t));
    if (!arcs_display_priv.panel) {
        LISA_LOGE(LOG_TAG, "Failed to allocate memory for panel");
        lisa_mutex_delete(arcs_display_priv.mutex);
        return LISA_DEVICE_ERR_NO_MEM;
    }
    memset(arcs_display_priv.panel, 0, sizeof(lisa_display_panel_t));

    arcs_display_priv.panel->panel_dev = panel_dev;

    panel_rotate_init();

    return LISA_DEVICE_OK;
}

static const lisa_display_api_t arcs_display_api = {
    .write            = arcs_display_write,
    .attach_bus       = arcs_attach_bus,
    .blanking_on      = arcs_blanking_on,
    .blanking_off     = arcs_blanking_off,
    .set_brightness   = arcs_set_brightness,
    .get_capabilities = arcs_get_capabilities,
    .set_orientation  = arcs_set_orientation,
};

LISA_DEVICE_REGISTER(display, &arcs_display_api, &arcs_display_priv, NULL, lisa_display_device_init, LISA_DEVICE_PRIORITY_NORMAL);
