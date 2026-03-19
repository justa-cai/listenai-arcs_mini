/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file board.c
 * @brief 板级初始化实现
 */

#include "board.h"
#include "lisa_device.h"
#include "lisa_gpio.h"

#define LOG_TAG "board"
#include "lisa_log.h"

static lisa_device_t *s_lcd_cs_l_dev = NULL;
static lisa_device_t *s_lcd_cs_r_dev = NULL;

const char *board_get_name(void)
{
    return "arcs_mini";
}

void board_display_composite_init(void)
{
    s_lcd_cs_l_dev = lisa_device_get(LCD_CS_L_DEVICE_NAME);
    if (!lisa_device_ready(s_lcd_cs_l_dev)) {
        LOGE("LCD_CS_L device not ready");
        s_lcd_cs_l_dev = NULL;
        return;
    }

    s_lcd_cs_r_dev = lisa_device_get(LCD_CS_R_DEVICE_NAME);
    if (!lisa_device_ready(s_lcd_cs_r_dev)) {
        LOGE("LCD_CS_R device not ready");
        s_lcd_cs_r_dev = NULL;
        return;
    }

    lisa_gpio_configure(s_lcd_cs_l_dev, LCD_CS_L_PIN, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    lisa_gpio_configure(s_lcd_cs_r_dev, LCD_CS_R_PIN, LISA_GPIO_CONFIG_OUTPUT_HIGH);
}

void board_display_composite_activate(int disp_idx)
{
    if (disp_idx == 0) {
        lisa_gpio_write_pin(s_lcd_cs_l_dev, LCD_CS_L_PIN, LISA_GPIO_LOW);
    } else if (disp_idx == 1) {
        lisa_gpio_write_pin(s_lcd_cs_r_dev, LCD_CS_R_PIN, LISA_GPIO_LOW);
    }
}

void board_display_composite_deactivate(int disp_idx)
{
    lisa_gpio_write_pin(s_lcd_cs_l_dev, LCD_CS_L_PIN, LISA_GPIO_HIGH);
    lisa_gpio_write_pin(s_lcd_cs_r_dev, LCD_CS_R_PIN, LISA_GPIO_HIGH);
}
