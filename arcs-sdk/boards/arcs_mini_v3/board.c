/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "board.h"
#include "lisa_gpio.h"
#include "sys_init.h"

static int board_4g_gpio_init(void)
{
    lisa_device_t *gpioa = lisa_device_get(CH_4G_PWR_DEVICE_NAME);
    if (!gpioa || !lisa_device_ready(gpioa)) {
        return 0;
    }

    (void)lisa_gpio_configure(gpioa, CH_4G_PWR_PIN, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    (void)lisa_gpio_configure(gpioa, CH_4G_RST_PIN, LISA_GPIO_CONFIG_OUTPUT_HIGH);

    return 0;
}

SYS_INIT(board_4g_gpio_init, SYS_INIT_LEVEL_PRE_KERNEL, SYS_INIT_SUB_PRIORITY_EARLY);

const char *board_get_name(void)
{
    return "arcs_mini_v3";
}
