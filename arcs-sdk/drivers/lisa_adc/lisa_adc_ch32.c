/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_adc_ch32.h"
#include "lisa_adc.h"

#include <stdint.h>

#include "systick.h"

#define LOG_TAG "lisa_ch32v003"
#include <lisa_log.h>

#if CONFIG_LISA_CH32V003_ADC

static int ch32v003_adc_check_channel(uint32_t channel)
{
    return (channel < 8U) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_RANGE;
}

static int ch32v003_adc_channel_setup(lisa_device_t *dev, uint32_t channel,
                                      const lisa_adc_channel_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (ch32v003_adc_check_channel(channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    if (config->resolution != LISA_ADC_RESOLUTION_10BIT) {
        LISA_LOGW(LOG_TAG, "exadc returns 12-bit raw data; LISA ADC config only declares %d-bit",
                  LISA_ADC_RESOLUTION_10BIT);
    }

    return LISA_DEVICE_OK;
}

static int ch32v003_adc_read(lisa_device_t *dev, uint32_t channel, uint16_t *value)
{
    ch32v003_priv_t *base = NULL;
    uint8_t status = 0;
    uint8_t high = 0;
    uint8_t low = 0;
    uint32_t elapsed_ms = 0;
    int ret;

    if (!lisa_device_is_initialized(dev) || !value) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = ch32v003_adc_check_channel(channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    CH32V003_DEVICE_LOCK(base);

    ret = ch32v003_write_checked_unlocked(base, CH32V003_ADC_CONFIG_0,
                                          (CH32V003_ADC_SAMPLING_CYCLE_30 << 1) |
                                          CH32V003_ADC_MODE_ONCE);
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_ADC_CONFIG_1, (uint8_t)(1U << channel));
    }

    while (ret == LISA_DEVICE_OK) {
        ret = ch32v003_read_reg_unlocked(base, CH32V003_ADC_STATUS_REG, &status);
        if (ret != LISA_DEVICE_OK || (status & CH32V003_ADC_EOC)) {
            break;
        }

        if (elapsed_ms >= CONFIG_LISA_CH32V003_ADC_TIMEOUT_MS) {
            ret = LISA_DEVICE_ERR_TIMEOUT;
            break;
        }

        SysTick_Delay_Ms(CONFIG_LISA_CH32V003_ADC_POLL_MS);
        elapsed_ms += CONFIG_LISA_CH32V003_ADC_POLL_MS;
    }

    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_read_reg_unlocked(base, CH32V003_ADC_CH_DATA_H_REG(channel), &high);
    }
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_read_reg_unlocked(base, CH32V003_ADC_CH_DATA_L_REG(channel), &low);
    }

    CH32V003_DEVICE_UNLOCK(base);

    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    *value = ((uint16_t)high << 8) | low;
    return LISA_DEVICE_OK;
}

static const lisa_adc_api_t ch32v003_adc_api = {
    .read = ch32v003_adc_read,
    .channel_setup = ch32v003_adc_channel_setup,
};

static int ch32v003_exadc_init(void)
{
    ch32v003_priv_t *base = NULL;

    return ch32v003_get_priv(&base);
}

LISA_DEVICE_REGISTER(exadc, &ch32v003_adc_api, NULL, NULL, ch32v003_exadc_init,
                     LISA_DEVICE_LEVEL_NORMAL, 62);

#endif /* CONFIG_LISA_CH32V003_ADC */
