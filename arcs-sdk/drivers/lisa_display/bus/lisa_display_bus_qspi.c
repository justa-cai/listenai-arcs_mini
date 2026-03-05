/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "lisa_qspilcd.h"
#include "lisa_display_bus.h"

#define LOG_TAG "bus.qspi"
#include <lisa_log.h>

#define DRV_QSPI_CLK_HZ_DEFAULT (50*1000*1000) // 50MHz

typedef struct {
    lisa_device_t *qspi_dev;
} display_bus_qspi_priv_t;

static int bus_qspi_attach(lisa_device_t *bus_dev, lisa_display_bus_type_t bus_type, const lisa_display_bus_config_u *bus_config)
{
    if (!bus_dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (bus_type != LISA_DISPLAY_BUS_QSPI) {
        LISA_LOGE(LOG_TAG, "Bus type unmatch!!!!");
        return LISA_DEVICE_ERR_INVALID;
    }

    display_bus_qspi_priv_t *priv = (display_bus_qspi_priv_t *)bus_dev->priv_data;
    priv->qspi_dev                = bus_config->qspi.qspi_dev;

    uint32_t control = LISA_QSPILCD_TXIO_PIO | LISA_QSPILCD_CPOL0_CPHA0 | 
                        LISA_QSPILCD_MSB_LSB | LISA_QSPILCD_MODE_MASTER |
                        LISA_QSPILCD_DATA_BITS(8);
    int ret = lisa_qspilcd_control(priv->qspi_dev, control,
        bus_config->qspi.qspi_freq ? bus_config->qspi.qspi_freq : DRV_QSPI_CLK_HZ_DEFAULT);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_qspilcd_cs_configure(priv->qspi_dev, bus_config->qspi.cs_gpio, bus_config->qspi.cs_pin);

    return LISA_DEVICE_OK;
}

static int bus_qspi_trans_cmd_data(lisa_device_t *bus_dev, uint32_t cmd, uint8_t cmd_bits, const void *data, size_t len)
{
    int ret = LISA_DEVICE_OK;
    display_bus_qspi_priv_t *priv = (display_bus_qspi_priv_t *)bus_dev->priv_data;

    lisa_qspilcd_xfer_t xfer = {
        .data_bits = 8,
        .size_bytes = 0,
        .use_dma = false,
        .buf = (void *)NULL,
        .lane = LISA_QSPILCD_LANE_SINGLE,
    };

    xfer.buf = (void *)&cmd;
    xfer.size_bytes = cmd_bits / 8;
    ret = lisa_qspilcd_transfer(priv->qspi_dev, &xfer);

    if (data && len > 0) {
        xfer.buf = (void *)data;
        xfer.size_bytes = len;
        ret = lisa_qspilcd_transfer(priv->qspi_dev, &xfer);
    }

    return ret;
}

static void bus_qspi_transfer_control(lisa_device_t *bus_dev, bool enable)
{
    if (!bus_dev || !bus_dev->priv_data) {
        return;
    }

    display_bus_qspi_priv_t *priv = (display_bus_qspi_priv_t *)bus_dev->priv_data;

    if (enable) {
        lisa_qspilcd_cs_control(priv->qspi_dev, 0);
    } else {
        lisa_qspilcd_cs_control(priv->qspi_dev, 1);
    }
}

static int bus_qspi_wait_for_completion(lisa_device_t *bus_dev, int32_t timeout_ms)
{
    display_bus_qspi_priv_t *priv = (display_bus_qspi_priv_t *)bus_dev->priv_data;

    // 等待 DMA 传输完成
    int ret = lisa_qspilcd_wait_done(priv->qspi_dev, timeout_ms);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return LISA_DEVICE_OK;
}

static int bus_qspi_write_pixels(lisa_device_t *bus_dev, const void *pixels, size_t len)
{
    display_bus_qspi_priv_t *priv = (display_bus_qspi_priv_t *)bus_dev->priv_data;
    int ret = 0;

    lisa_qspilcd_xfer_t xfer = {
        .data_bits = 16,
        .buf = (void *)pixels,
        .size_bytes = len,
        .lane = LISA_QSPILCD_LANE_QUAD,
        .use_dma = true,
    };
    ret = lisa_qspilcd_transfer(priv->qspi_dev, &xfer);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return LISA_DEVICE_OK;
}

static const lisa_display_bus_api_t display_bus_qspi_api = {
    .attach              = bus_qspi_attach,
    .write_pixels        = bus_qspi_write_pixels,
    .trans_cmd_data      = bus_qspi_trans_cmd_data,
    .transfer_control    = bus_qspi_transfer_control,
    .wait_for_completion = bus_qspi_wait_for_completion,
};

static display_bus_qspi_priv_t display_bus_qspi_priv;

static int bus_qspi_init(void)
{
    memset(&display_bus_qspi_priv, 0, sizeof(display_bus_qspi_priv));

    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(panel_bus_qspi, &display_bus_qspi_api, &display_bus_qspi_priv, NULL, bus_qspi_init, LISA_DEVICE_PRIORITY_HIGH);