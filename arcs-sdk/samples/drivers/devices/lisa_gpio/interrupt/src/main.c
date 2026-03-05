/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA GPIO 中断示例
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

#define GPIO_DEVICE "gpioa"
#define GPIO_PIN  23

/*
    为满足不同板型示例场景，重定向gpioa设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIO_PIN, CSK_IOMUX_FUNC_DEFAULT);
}
#endif

static void button_irq_callback(uint32_t pin, void *user_data)
{
    lisa_device_t *gpio_dev = (lisa_device_t *)user_data;

    LISA_LOGI(LOG_TAG, "Interrupt triggered on %s pin %d", gpio_dev->name, pin);

    lisa_gpio_disable_irq(gpio_dev, pin);
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA GPIO interrupt example ===");

    lisa_device_t *gpio_dev = lisa_device_get(GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", GPIO_DEVICE);
        return -1;
    }

    /* 配置为输入模式，启用上拉 */
    int ret = lisa_gpio_configure(gpio_dev, GPIO_PIN, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: configure pin failed");
        return -1;
    }

    ret = lisa_gpio_configure_irq(gpio_dev, GPIO_PIN,
                                      LISA_GPIO_IRQ_EDGE_FALLING,
                                      button_irq_callback,
                                      gpio_dev);  /* 传递设备指针作为 user_data */
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: interrupt configuration failed");
        return -1;
    }

    ret = lisa_gpio_enable_irq(gpio_dev, GPIO_PIN);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: enable interrupt failed");
        return -1;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        LISA_LOGI(LOG_TAG, "Waiting for interrupt...");
    }
}
