/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "lisa_device.h"
#include "lisa_display_bus.h"
#include "lisa_spi.h"
#include "lisa_gpio.h"
#include "lisa_semaphore.h"
#include "lisa_thread.h"

#define LOG_TAG "bus.spi4"
#include <lisa_log.h>

typedef struct {
    lisa_device_t *spi_dev;
    lisa_semaphore_t *tx_complete_sem;
    lisa_device_t *dc_gpio;
    uint32_t dc_pin;
    lisa_device_t *cs_gpio;
    uint32_t cs_pin;
} bus_spi4_priv_t;

static bus_spi4_priv_t display_bus_spi4_priv;

static inline void set_dc_pin(bus_spi4_priv_t *priv, int level)
{
    lisa_gpio_write_pin(priv->dc_gpio, priv->dc_pin, level);
}

static void spi_transfer_callback(void *user_data)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)user_data;
    if (priv->tx_complete_sem) {
        lisa_semaphore_give(priv->tx_complete_sem);
    }
}

static int bus_spi4_attach(lisa_device_t *bus_dev, lisa_display_bus_type_t bus_type, const lisa_display_bus_config_u *bus_config)
{
    if (!bus_dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (bus_type != LISA_DISPLAY_BUS_SPI_4WIRE) {
        LISA_LOGE(LOG_TAG, "Bus type mismatch: %d", bus_type);
        return LISA_DEVICE_ERR_INVALID;
    }

    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;
    const lisa_display_bus_spi_4wire_config_t *spi_config = &bus_config->spi_4wire;

    priv->dc_gpio = spi_config->dc_gpio;
    priv->dc_pin  = spi_config->dc_pin;
    priv->cs_gpio = spi_config->cs_gpio;
    priv->cs_pin  = spi_config->cs_pin;
    priv->spi_dev = spi_config->spi_dev;

    lisa_gpio_configure(priv->cs_gpio, priv->cs_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    lisa_gpio_configure(priv->dc_gpio, priv->dc_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);

    /* 初始配置为中断模式 */
    lisa_spi_config_t spi_cfg = {
        .frequency        = spi_config->spi_freq ? spi_config->spi_freq : 50*1000*1000,
        .bit_order        = LISA_SPI_BIT_ORDER_MSB_FIRST,
        .flags            = LISA_SPI_FLAG_SOFTWARE_CS,
        .tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,
        .rx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,
        .tx_dma_channel   = CONFIG_LISA_DISPLAY_SPI_4WIRE_DMA_CH,
        .mode             = LISA_SPI_MODE_3,
        .master_mode      = true,
        .data_bits        = 8,
    };
    int ret = lisa_spi_configure(priv->spi_dev, &spi_cfg);
    if (ret) {
        LISA_LOGE(LOG_TAG, "Failed to configure SPI: %d", ret);
        return ret;
    }
    lisa_spi_register_callback(priv->spi_dev, spi_transfer_callback, priv);


    return LISA_DEVICE_OK;
}

static int bus_spi4_trans_cmd_data(lisa_device_t *bus_dev, uint32_t cmd, uint8_t cmd_bits, const void *data, size_t len)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;

    if (cmd_bits > 0) {
        set_dc_pin(priv, 0); // Command mode
        lisa_spi_write(priv->spi_dev, (uint8_t *)&cmd, cmd_bits / 8);
        
        if (lisa_semaphore_take(priv->tx_complete_sem, 100) != LISA_OK) {
            LISA_LOGE(LOG_TAG, "SPI cmd transfer timeout");
            return LISA_DEVICE_ERR_TIMEOUT;
        }
    }
    if (data && len > 0) {
        set_dc_pin(priv, 1); // Data mode
        lisa_spi_write(priv->spi_dev, data, len);
        
        if (lisa_semaphore_take(priv->tx_complete_sem, 100) != LISA_OK) {
            LISA_LOGE(LOG_TAG, "SPI data transfer timeout");
            return LISA_DEVICE_ERR_TIMEOUT;
        }
    }

    return LISA_DEVICE_OK;
}

static void bus_spi4_transfer_control(lisa_device_t *bus_dev, bool enable)
{
    if (!bus_dev || !bus_dev->priv_data) {
        return;
    }
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;

    if (enable) {
        lisa_gpio_write_pin(priv->cs_gpio, priv->cs_pin, 0);
    }
    else {
        lisa_gpio_write_pin(priv->cs_gpio, priv->cs_pin, 1);
    }
}

static int bus_spi4_write_pixels(lisa_device_t *bus_dev, const void *pixels, size_t len)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;

    /* 切换到DMA模式用于像素传输 */
    lisa_spi_config_t spi_config;
    lisa_spi_get_config(priv->spi_dev, &spi_config);

    spi_config.tx_transfer_mode = LISA_SPI_DMA_TRANSFER;
    spi_config.data_bits = 16;
    int ret = lisa_spi_configure(priv->spi_dev, &spi_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "%s: spi configure failed.", __func__);
        return ret;
    }

    set_dc_pin(priv, 1); // Data mode
    ret = lisa_spi_write(priv->spi_dev, pixels, len/2);
    return ret;
}

static int bus_spi4_wait_for_completion(lisa_device_t *bus_dev, int32_t timeout_ms)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;
    if (lisa_semaphore_take(priv->tx_complete_sem, timeout_ms) != LISA_OK) {
        LISA_LOGE(LOG_TAG, "%s: semaphore take failed.", __func__);
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    /* 切换回中断模式用于命令传输 */
    lisa_spi_config_t spi_config;
    lisa_spi_get_config(priv->spi_dev, &spi_config);

    spi_config.tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER;
    spi_config.data_bits = 8;
    int ret = lisa_spi_configure(priv->spi_dev, &spi_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "%s: spi configure failed.", __func__);
        return ret;
    }

    return LISA_DEVICE_OK;
}

static const lisa_display_bus_api_t display_bus_spi4_api = {
    .attach              = bus_spi4_attach,
    .write_pixels        = bus_spi4_write_pixels,
    .trans_cmd_data      = bus_spi4_trans_cmd_data,
    .transfer_control    = bus_spi4_transfer_control,
    .wait_for_completion = bus_spi4_wait_for_completion,
};

static int bus_spi4_init(void)
{
    memset(&display_bus_spi4_priv, 0, sizeof(display_bus_spi4_priv));

    display_bus_spi4_priv.tx_complete_sem = lisa_semaphore_create(1);

    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(panel_bus_spi_4wire, &display_bus_spi4_api, &display_bus_spi4_priv, NULL, bus_spi4_init, LISA_DEVICE_PRIORITY_HIGH);