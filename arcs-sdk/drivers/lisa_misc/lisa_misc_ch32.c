/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_misc_ch32.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lisa_i2c.h"
#include "systick.h"

#define LOG_TAG "lisa_ch32v003"
#include <lisa_log.h>

static ch32v003_priv_t ch32v003_priv;

static void ch32v003_print_info(uint8_t id, uint8_t version);
static int ch32v003_wait_idle_unlocked(ch32v003_priv_t *priv);
static int ch32v003_write_reg_unlocked(ch32v003_priv_t *priv, uint8_t reg, uint8_t value);

int ch32v003_read_reg_unlocked(ch32v003_priv_t *priv, uint8_t reg, uint8_t *value)
{
    if (!priv || !priv->i2c_dev || !value) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2c_msg_t msgs[2] = {
        {
            .addr = priv->i2c_addr,
            .flags = LISA_I2C_FLAG_NO_STOP,
            .len = 1,
            .buf = &reg,
        },
        {
            .addr = priv->i2c_addr,
            .flags = LISA_I2C_FLAG_READ,
            .len = 1,
            .buf = value,
        },
    };

    return lisa_i2c_transfer(priv->i2c_dev, msgs, 2);
}

static int ch32v003_write_reg_unlocked(ch32v003_priv_t *priv, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};

    if (!priv || !priv->i2c_dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    return lisa_i2c_write(priv->i2c_dev, priv->i2c_addr, buf, sizeof(buf));
}

static int ch32v003_wait_idle_unlocked(ch32v003_priv_t *priv)
{
    uint8_t busy = 0;
    int ret = LISA_DEVICE_OK;

    for (uint32_t retry = 0; retry < CONFIG_LISA_CH32V003_BUSY_WAIT_RETRY; retry++) {
        ret = ch32v003_read_reg_unlocked(priv, CH32V003_STA_REG, &busy);
        if (ret != LISA_DEVICE_OK) {
            return ret;
        }

        if ((busy & CH32V003_STATUS_BUSY) == 0) {
            return LISA_DEVICE_OK;
        }

        SysTick_Delay_Ms(CONFIG_LISA_CH32V003_BUSY_WAIT_MS);
    }

    LISA_LOGE(LOG_TAG, "wait busy timeout");
    return LISA_DEVICE_ERR_BUSY;
}

int ch32v003_probe_unlocked(ch32v003_priv_t *priv)
{
    int ret;

    if (!priv || !priv->i2c_dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (priv->probed) {
        return LISA_DEVICE_OK;
    }

    SysTick_Delay_Ms(CONFIG_LISA_CH32V003_INIT_DELAY_MS);

    ret = ch32v003_read_reg_unlocked(priv, CH32V003_ID_REG, &priv->chip_id);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "read id failed: %d", ret);
        return ret;
    }

    ret = ch32v003_read_reg_unlocked(priv, CH32V003_VER_REG, &priv->version);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "read version failed: %d", ret);
        return ret;
    }

    priv->probed = true;
    ch32v003_print_info(priv->chip_id, priv->version);
    return LISA_DEVICE_OK;
}

int ch32v003_write_checked_unlocked(ch32v003_priv_t *priv, uint8_t reg, uint8_t value)
{
    int ret = ch32v003_probe_unlocked(priv);

    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = ch32v003_wait_idle_unlocked(priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return ch32v003_write_reg_unlocked(priv, reg, value);
}

int ch32v003_get_priv(ch32v003_priv_t **out)
{
    lisa_device_t *dev;

    if (!out) {
        return LISA_DEVICE_ERR_INVALID;
    }

    dev = lisa_device_get(LISA_CH32V003_DEVICE_NAME);
    if (!lisa_device_ready(dev) || !dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    *out = (ch32v003_priv_t *)dev->priv_data;
    return LISA_DEVICE_OK;
}

int lisa_ch32v003_read_reg(lisa_device_t *dev, uint8_t reg, uint8_t *value)
{
    ch32v003_priv_t *priv;
    int ret;

    if (!value) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!dev) {
        dev = lisa_device_get(LISA_CH32V003_DEVICE_NAME);
    }
    if (!lisa_device_is_initialized(dev) || !dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    priv = (ch32v003_priv_t *)dev->priv_data;
    CH32V003_DEVICE_LOCK(priv);
    ret = ch32v003_probe_unlocked(priv);
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_read_reg_unlocked(priv, reg, value);
    }
    CH32V003_DEVICE_UNLOCK(priv);

    return ret;
}

int lisa_ch32v003_write_reg(lisa_device_t *dev, uint8_t reg, uint8_t value)
{
    ch32v003_priv_t *priv;
    int ret;

    if (!dev) {
        dev = lisa_device_get(LISA_CH32V003_DEVICE_NAME);
    }
    if (!lisa_device_is_initialized(dev) || !dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    priv = (ch32v003_priv_t *)dev->priv_data;
    CH32V003_DEVICE_LOCK(priv);
    ret = ch32v003_write_checked_unlocked(priv, reg, value);
    CH32V003_DEVICE_UNLOCK(priv);

    return ret;
}

uint8_t lisa_ch32v003_get_version(void)
{
    return ch32v003_priv.version;
}

static void ch32v003_print_info(uint8_t id, uint8_t version)
{
    const char *chip = "unknown";

    if (id == 0x01) {
        chip = "ch32v003";
    } else if (id == 0x02) {
        chip = "ch32v203";
    }

    LISA_LOGI(LOG_TAG, "exmcu info, chip:%s, id:0x%02x, version:v%d.%d",
              chip, id, version / 10, version % 10);

    if (version != CONFIG_LISA_CH32V003_EXPECTED_PROTOCOL_VERSION) {
        LISA_LOGW(LOG_TAG, "protocol version mismatch, expected v%d.%d, got v%d.%d",
                  CONFIG_LISA_CH32V003_EXPECTED_PROTOCOL_VERSION / 10,
                  CONFIG_LISA_CH32V003_EXPECTED_PROTOCOL_VERSION % 10,
                  version / 10, version % 10);
    }
}

static int ch32v003_init(void)
{
    lisa_i2c_config_t i2c_cfg = LISA_I2C_CONFIG_FAST();
    int ret;

    memset(&ch32v003_priv, 0, sizeof(ch32v003_priv));

    ch32v003_priv.i2c_name = CONFIG_LISA_CH32V003_I2C_DEVICE_NAME;
    ch32v003_priv.i2c_addr = CONFIG_LISA_CH32V003_I2C_ADDR;
    ch32v003_priv.i2c_dev = lisa_device_get(ch32v003_priv.i2c_name);
    if (!lisa_device_ready(ch32v003_priv.i2c_dev)) {
        LISA_LOGE(LOG_TAG, "I2C device %s not ready", ch32v003_priv.i2c_name);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    ch32v003_priv.mutex = lisa_mutex_create();
    if (!ch32v003_priv.mutex) {
        LISA_LOGE(LOG_TAG, "failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = lisa_i2c_configure(ch32v003_priv.i2c_dev, &i2c_cfg);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "configure %s failed: %d", ch32v003_priv.i2c_name, ret);
        return ret;
    }

    /*
     * Do not probe CH32V003 here. Normal-level LISA devices are initialized
     * before the FreeRTOS scheduler starts, while the ARCS I2C transfer path
     * waits on a FreeRTOS semaphore. Probe lazily on first real EXMCU access.
     */
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(ch32v003, NULL, &ch32v003_priv, NULL, ch32v003_init, LISA_DEVICE_LEVEL_NORMAL, 60);
