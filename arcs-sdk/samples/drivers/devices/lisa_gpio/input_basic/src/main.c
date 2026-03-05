/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA GPIO 基础输入示例 - 读取引脚电平
 *
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"

#include "FreeRTOS.h"
#include "task.h"

#define GPIO_DEVICE "gpioa"
#define INPUT_PIN   23

/*
    为满足不同板型示例场景，重定向gpioa设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, INPUT_PIN, CSK_IOMUX_FUNC_DEFAULT);
}
#endif

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA GPIO input example ===");

    lisa_device_t *gpio_dev = lisa_device_get(GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", GPIO_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", GPIO_DEVICE);

    /* 配置为输入模式，启用上拉 */
    int ret = lisa_gpio_configure(gpio_dev, INPUT_PIN, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: configure pin failed");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Start input monitoring...");

    while (1) {
        int ret = lisa_gpio_read_pin(gpio_dev, INPUT_PIN);
        if (ret < 0) {
            LISA_LOGE(LOG_TAG, "Error: read pin failed");
        }
        LISA_LOGI(LOG_TAG, "PA%d level: %s", INPUT_PIN, (ret == LISA_GPIO_HIGH) ? "HIGH" : "LOW");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
