/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_pwm_ch32.h"
#include "lisa_pwm.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if CONFIG_LISA_CH32V003_PWM

typedef struct {
    uint32_t frequency_hz;
    uint8_t duty_percent;
    lisa_pwm_polarity_t polarity;
    bool configured;
    bool enabled;
} ch32v003_pwm_priv_t;

static ch32v003_pwm_priv_t expwm_priv;

static int ch32v003_pwm_check(lisa_device_t *dev, uint32_t channel)
{
    if (!lisa_device_is_initialized(dev) || !dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    return (channel == CH32V003_PWM_CHANNEL) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_RANGE;
}

static int ch32v003_pwm_write_ctrl_unlocked(ch32v003_priv_t *base, bool enable)
{
    uint8_t ctrl = enable ? CH32V003_PWM_EN : 0;

#if CONFIG_LISA_CH32V003_PWM_DEADTIME > 0
    if (enable) {
        ctrl |= CH32V003_PWM_DEADTIME_EN;
    }
#endif

    return ch32v003_write_checked_unlocked(base, CH32V003_PWM_CTRL_REG, ctrl);
}

static int ch32v003_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config)
{
    ch32v003_pwm_priv_t *pwm;
    int ret;

    if (!config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = ch32v003_pwm_check(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    pwm = (ch32v003_pwm_priv_t *)dev->priv_data;
    pwm->polarity = config->polarity;
    return LISA_DEVICE_OK;
}

static int ch32v003_pwm_get_config(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config)
{
    ch32v003_pwm_priv_t *pwm;
    int ret;

    if (!config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = ch32v003_pwm_check(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    pwm = (ch32v003_pwm_priv_t *)dev->priv_data;
    config->polarity = pwm->polarity;
    return LISA_DEVICE_OK;
}

static int ch32v003_pwm_set(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_percent)
{
    ch32v003_pwm_priv_t *pwm;
    ch32v003_priv_t *base = NULL;
    uint32_t cycles_per_sec;
    uint32_t period_cycles;
    uint32_t pulse_cycles;
    uint16_t arr;
    uint16_t psc;
    uint16_t ccp;
    int ret;

    ret = ch32v003_pwm_check(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    if (frequency_hz == 0 || duty_percent > LISA_PWM_DUTY_PERCENT_MAX ||
        CONFIG_LISA_CH32V003_PWM_CLOCK_PRESCALER == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    cycles_per_sec = CONFIG_LISA_CH32V003_PWM_CLOCK_FREQUENCY /
                     CONFIG_LISA_CH32V003_PWM_CLOCK_PRESCALER;
    period_cycles = cycles_per_sec / frequency_hz;
    if (period_cycles == 0 || period_cycles > 0xFFFFU) {
        return LISA_DEVICE_ERR_RANGE;
    }

    pulse_cycles = (period_cycles * duty_percent) / 100U;

    pwm = (ch32v003_pwm_priv_t *)dev->priv_data;
    if (pwm->polarity == LISA_PWM_POLARITY_INVERTED) {
        pulse_cycles = period_cycles - pulse_cycles;
    }

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    arr = (uint16_t)period_cycles;
    psc = (uint16_t)CONFIG_LISA_CH32V003_PWM_CLOCK_PRESCALER;
    ccp = (uint16_t)pulse_cycles;

    CH32V003_DEVICE_LOCK(base);

    ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_ARR_REG_H, (uint8_t)(arr >> 8));
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_ARR_REG_L, (uint8_t)arr);
    }
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_PSC_REG_H, (uint8_t)(psc >> 8));
    }
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_PSC_REG_L, (uint8_t)psc);
    }
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_CCP_REG_H, (uint8_t)(ccp >> 8));
    }
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_CCP_REG_L, (uint8_t)ccp);
    }
#if CONFIG_LISA_CH32V003_PWM_DEADTIME > 0
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_DTS_REG,
                                              (uint8_t)CONFIG_LISA_CH32V003_PWM_DEADTIME);
    }
#endif
#if CONFIG_LISA_CH32V003_PWM_BRAKE
    if (ret == LISA_DEVICE_OK && CONFIG_LISA_CH32V003_PWM_DEADTIME > 0) {
        ret = ch32v003_write_checked_unlocked(base, CH32V003_PWM_BRAKE_REG, CH32V003_PWM_BRAKE_EN);
    }
#endif
    if (ret == LISA_DEVICE_OK && pwm->enabled) {
        ret = ch32v003_pwm_write_ctrl_unlocked(base, true);
    }

    CH32V003_DEVICE_UNLOCK(base);

    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    pwm->frequency_hz = frequency_hz;
    pwm->duty_percent = duty_percent;
    pwm->configured = true;

    return LISA_DEVICE_OK;
}

static int ch32v003_pwm_enable(lisa_device_t *dev, uint32_t channel)
{
    ch32v003_pwm_priv_t *pwm;
    ch32v003_priv_t *base = NULL;
    int ret;

    ret = ch32v003_pwm_check(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    pwm = (ch32v003_pwm_priv_t *)dev->priv_data;
    if (!pwm->configured) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    CH32V003_DEVICE_LOCK(base);
    ret = ch32v003_pwm_write_ctrl_unlocked(base, true);
    CH32V003_DEVICE_UNLOCK(base);

    if (ret == LISA_DEVICE_OK) {
        pwm->enabled = true;
    }

    return ret;
}

static int ch32v003_pwm_disable(lisa_device_t *dev, uint32_t channel)
{
    ch32v003_priv_t *base = NULL;
    int ret;

    ret = ch32v003_pwm_check(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    CH32V003_DEVICE_LOCK(base);
    ret = ch32v003_pwm_write_ctrl_unlocked(base, false);
    CH32V003_DEVICE_UNLOCK(base);

    if (ret == LISA_DEVICE_OK) {
        ch32v003_pwm_priv_t *pwm = (ch32v003_pwm_priv_t *)dev->priv_data;
        pwm->enabled = false;
    }

    return ret;
}

static const lisa_pwm_api_t ch32v003_pwm_api = {
    .enable = ch32v003_pwm_enable,
    .disable = ch32v003_pwm_disable,
    .set = ch32v003_pwm_set,
    .configure = ch32v003_pwm_configure,
    .get_config = ch32v003_pwm_get_config,
};

static int ch32v003_expwm_init(void)
{
    ch32v003_priv_t *base = NULL;

    memset(&expwm_priv, 0, sizeof(expwm_priv));
    expwm_priv.polarity = LISA_PWM_POLARITY_NORMAL;
    return ch32v003_get_priv(&base);
}

LISA_DEVICE_REGISTER(expwm, &ch32v003_pwm_api, &expwm_priv, NULL, ch32v003_expwm_init,
                     LISA_DEVICE_LEVEL_NORMAL, 63);

#endif /* CONFIG_LISA_CH32V003_PWM */
