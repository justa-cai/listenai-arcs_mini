/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA GPIO 基础输出示例 - LED闪烁
 *
 * 本示例演示如何使用 LISA GPIO 驱动控制LED闪烁：
 * 1. 初始化GPIO设备
 * 2. 配置引脚为输出模式
 * 3. 循环输出高低电平实现LED闪烁
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "pinmux.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef CONFIG_BOARD_ARCS_MINI
#define LED_PIN        9
#endif // !CONFIG_BOARD_ARCS_MINI
#define GPIO_DEVICE    "gpiob"

/*
    为满足不同板型示例场景，重定向gpiob设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LED_PIN, CSK_IOMUX_FUNC_DEFAULT);
}
#endif

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA GPIO output example ===");

    lisa_device_t *gpio_dev = lisa_device_get(GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", GPIO_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", GPIO_DEVICE);

    /* 配置为输出模式，初始电平为低 */
    int ret = lisa_gpio_configure(gpio_dev, LED_PIN, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: GPIO configuration failed (code: %d)", ret);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Start toggling LED...");

    bool led_on = true;
    while (1) {
        lisa_gpio_write_pin(gpio_dev, LED_PIN, led_on ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
        LISA_LOGI(LOG_TAG, "LED is %s", led_on ? "ON" : "OFF");
        vTaskDelay(pdMS_TO_TICKS(1000));
        led_on = !led_on;
    }
    
    return 0;
}
