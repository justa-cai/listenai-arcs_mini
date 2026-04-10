/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_lisa_gpio.h"
#include "lisa_device.h"

DEFINE_FAKE_VALUE_FUNC(int, mock_lisa_gpio_configure, lisa_device_t *, uint32_t, lisa_gpio_flags_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_lisa_gpio_write_pin, lisa_device_t *, uint32_t, uint32_t);

static int mock_lisa_gpio_init(void)
{
    mock_lisa_gpio_reset();
    return 0;
}

static const lisa_gpio_api_t mock_gpio_api = {
    .configure = mock_lisa_gpio_configure,
    .write_pin = mock_lisa_gpio_write_pin,
};

LISA_DEVICE_REGISTER(gpioa,
                     &mock_gpio_api,
                     NULL,
                     NULL,
                     mock_lisa_gpio_init,
                     LISA_DEVICE_LEVEL_NORMAL,
                     LISA_DEVICE_PRIORITY_LOWEST);
LISA_DEVICE_REGISTER(gpiob,
                     &mock_gpio_api,
                     NULL,
                     NULL,
                     mock_lisa_gpio_init,
                     LISA_DEVICE_LEVEL_NORMAL,
                     LISA_DEVICE_PRIORITY_LOWEST);

void mock_lisa_gpio_reset(void)
{
    RESET_FAKE(mock_lisa_gpio_configure);
    RESET_FAKE(mock_lisa_gpio_write_pin);
}
