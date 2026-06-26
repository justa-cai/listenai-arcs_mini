/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_gpio_ch32.h"
#include "lisa_gpio.h"

#include <stdint.h>
#include <string.h>

#if CONFIG_LISA_CH32V003_GPIO

typedef struct {
    uint8_t port;
    uint8_t max_pins;
    uint8_t dir;
    uint8_t output;
    uint8_t pull_en;
    uint8_t pull_ctrl;
    uint8_t od;
} ch32v003_gpio_priv_t;

static ch32v003_gpio_priv_t exgpioa_priv = {.port = 0};
static ch32v003_gpio_priv_t exgpiob_priv = {.port = 1};
static ch32v003_gpio_priv_t exgpioc_priv = {.port = 2};
static ch32v003_gpio_priv_t exgpiod_priv = {.port = 3};

static int ch32v003_gpio_check(lisa_device_t *dev, uint32_t pin)
{
    ch32v003_gpio_priv_t *priv;

    if (!lisa_device_is_initialized(dev) || !dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    priv = (ch32v003_gpio_priv_t *)dev->priv_data;
    if (pin >= priv->max_pins) {
        return LISA_DEVICE_ERR_RANGE;
    }

    return LISA_DEVICE_OK;
}

static void ch32v003_gpio_priv_init(ch32v003_gpio_priv_t *priv, uint8_t port)
{
    memset(priv, 0, sizeof(*priv));
    priv->port = port;
    priv->max_pins = CH32V003_GPIO_MAX_PINS;
    priv->pull_ctrl = 0xFF;
}

static int ch32v003_gpio_configure(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t flags)
{
    ch32v003_gpio_priv_t *gpio;
    ch32v003_priv_t *base = NULL;
    uint8_t mask = (uint8_t)(1U << pin);
    int ret;

    ret = ch32v003_gpio_check(dev, pin);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    if ((flags & LISA_GPIO_PULL_UP) && (flags & LISA_GPIO_PULL_DOWN)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    gpio = (ch32v003_gpio_priv_t *)dev->priv_data;

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    CH32V003_DEVICE_LOCK(base);

    if (flags & LISA_GPIO_OUTPUT) {
        gpio->dir |= mask;
        gpio->od = 0;
        if (flags & LISA_GPIO_OUTPUT_INIT_HIGH) {
            gpio->output |= mask;
        } else {
            gpio->output &= (uint8_t)~mask;
        }

        ret = ch32v003_write_checked_unlocked(base, CH32V003_DIR_REG(gpio->port), gpio->dir);
        if (ret == LISA_DEVICE_OK) {
            ret = ch32v003_write_checked_unlocked(base, CH32V003_OD_REG(gpio->port), gpio->od);
        }
        if (ret == LISA_DEVICE_OK) {
            ret = ch32v003_write_checked_unlocked(base, CH32V003_OUT_REG(gpio->port), gpio->output);
        }
    } else {
        gpio->dir &= (uint8_t)~mask;
        ret = ch32v003_write_checked_unlocked(base, CH32V003_DIR_REG(gpio->port), gpio->dir);
    }

    if (ret == LISA_DEVICE_OK && ((flags & LISA_GPIO_PULL_UP) || (flags & LISA_GPIO_PULL_DOWN))) {
        gpio->pull_en |= mask;
        if (flags & LISA_GPIO_PULL_UP) {
            gpio->pull_ctrl |= mask;
        } else {
            gpio->pull_ctrl &= (uint8_t)~mask;
        }

        ret = ch32v003_write_checked_unlocked(base, CH32V003_PULL_EN_REG(gpio->port), gpio->pull_en);
        if (ret == LISA_DEVICE_OK) {
            ret = ch32v003_write_checked_unlocked(base, CH32V003_PULL_CTRL_REG(gpio->port), gpio->pull_ctrl);
        }
    }

    CH32V003_DEVICE_UNLOCK(base);
    return ret;
}

static int ch32v003_gpio_get_config(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t *flags)
{
    ch32v003_gpio_priv_t *gpio;
    uint8_t mask = (uint8_t)(1U << pin);
    int ret;

    if (!flags) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = ch32v003_gpio_check(dev, pin);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    gpio = (ch32v003_gpio_priv_t *)dev->priv_data;

    *flags = 0;
    if (gpio->dir & mask) {
        *flags |= LISA_GPIO_OUTPUT;
        if (gpio->output & mask) {
            *flags |= LISA_GPIO_OUTPUT_INIT_HIGH;
        }
    }

    if (gpio->pull_en & mask) {
        if (gpio->pull_ctrl & mask) {
            *flags |= LISA_GPIO_PULL_UP;
        } else {
            *flags |= LISA_GPIO_PULL_DOWN;
        }
    }

    return LISA_DEVICE_OK;
}

static int ch32v003_gpio_read_pin(lisa_device_t *dev, uint32_t pin)
{
    ch32v003_gpio_priv_t *gpio;
    ch32v003_priv_t *base = NULL;
    uint8_t mask = (uint8_t)(1U << pin);
    uint8_t value = 0;
    int ret;

    ret = ch32v003_gpio_check(dev, pin);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    gpio = (ch32v003_gpio_priv_t *)dev->priv_data;
    if (gpio->dir & mask) {
        return (gpio->output & mask) ? LISA_GPIO_HIGH : LISA_GPIO_LOW;
    }

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    CH32V003_DEVICE_LOCK(base);
    ret = ch32v003_probe_unlocked(base);
    if (ret == LISA_DEVICE_OK) {
        ret = ch32v003_read_reg_unlocked(base, CH32V003_IN_REG(gpio->port), &value);
    }
    CH32V003_DEVICE_UNLOCK(base);

    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return (value & mask) ? LISA_GPIO_HIGH : LISA_GPIO_LOW;
}

static int ch32v003_gpio_write_pin(lisa_device_t *dev, uint32_t pin, uint32_t value)
{
    ch32v003_gpio_priv_t *gpio;
    ch32v003_priv_t *base = NULL;
    uint8_t mask = (uint8_t)(1U << pin);
    int ret;

    ret = ch32v003_gpio_check(dev, pin);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    gpio = (ch32v003_gpio_priv_t *)dev->priv_data;
    if ((gpio->dir & mask) == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = ch32v003_get_priv(&base);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    CH32V003_DEVICE_LOCK(base);

    if (value) {
        gpio->output |= mask;
    } else {
        gpio->output &= (uint8_t)~mask;
    }
    ret = ch32v003_write_checked_unlocked(base, CH32V003_OUT_REG(gpio->port), gpio->output);

    CH32V003_DEVICE_UNLOCK(base);
    return ret;
}

static int ch32v003_gpio_irq_not_support(lisa_device_t *dev, uint32_t pin)
{
    (void)dev;
    (void)pin;
    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

static int ch32v003_gpio_configure_irq(lisa_device_t *dev, uint32_t pin, lisa_gpio_irq_mode_t mode,
                                       lisa_gpio_irq_callback_t callback, void *user_data)
{
    (void)mode;
    (void)callback;
    (void)user_data;
    return ch32v003_gpio_irq_not_support(dev, pin);
}

static const lisa_gpio_api_t ch32v003_gpio_api = {
    .configure = ch32v003_gpio_configure,
    .get_config = ch32v003_gpio_get_config,
    .read_pin = ch32v003_gpio_read_pin,
    .write_pin = ch32v003_gpio_write_pin,
    .configure_irq = ch32v003_gpio_configure_irq,
    .enable_irq = ch32v003_gpio_irq_not_support,
    .disable_irq = ch32v003_gpio_irq_not_support,
};

static int ch32v003_exgpioa_init(void)
{
    ch32v003_priv_t *base = NULL;

    ch32v003_gpio_priv_init(&exgpioa_priv, 0);
    return ch32v003_get_priv(&base);
}

static int ch32v003_exgpiob_init(void)
{
    ch32v003_priv_t *base = NULL;

    ch32v003_gpio_priv_init(&exgpiob_priv, 1);
    return ch32v003_get_priv(&base);
}

static int ch32v003_exgpioc_init(void)
{
    ch32v003_priv_t *base = NULL;

    ch32v003_gpio_priv_init(&exgpioc_priv, 2);
    return ch32v003_get_priv(&base);
}

static int ch32v003_exgpiod_init(void)
{
    ch32v003_priv_t *base = NULL;

    ch32v003_gpio_priv_init(&exgpiod_priv, 3);
    return ch32v003_get_priv(&base);
}

LISA_DEVICE_REGISTER(exgpioa, &ch32v003_gpio_api, &exgpioa_priv, NULL, ch32v003_exgpioa_init,
                     LISA_DEVICE_LEVEL_NORMAL, 61);
LISA_DEVICE_REGISTER(exgpiob, &ch32v003_gpio_api, &exgpiob_priv, NULL, ch32v003_exgpiob_init,
                     LISA_DEVICE_LEVEL_NORMAL, 61);
LISA_DEVICE_REGISTER(exgpioc, &ch32v003_gpio_api, &exgpioc_priv, NULL, ch32v003_exgpioc_init,
                     LISA_DEVICE_LEVEL_NORMAL, 61);
LISA_DEVICE_REGISTER(exgpiod, &ch32v003_gpio_api, &exgpiod_priv, NULL, ch32v003_exgpiod_init,
                     LISA_DEVICE_LEVEL_NORMAL, 61);

#endif /* CONFIG_LISA_CH32V003_GPIO */
