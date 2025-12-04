/**
 * @file power_manager.c
 * @brief Power management functions for system boot and shutdown
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#include <stdio.h>

#include "power_manager.h"
#include "lisa_time.h"
#include "lisa_log.h"

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "battery/battery.h"

#define TAG "power_mgr"

static void (*shutdown_cb)(void) = NULL;

static void power_gpio_init(void)
{
    // 初始化GPIOB
    GPIO_Initialize(POWER_BUTTON_GPIO_PORT, NULL, NULL);

    // 配置电源按键引脚 (PB4) 为输入
    IOMuxManager_PinConfigure(POWER_IOMUX_PAD, POWER_BUTTON_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(POWER_BUTTON_GPIO_PORT, POWER_BUTTON_PIN_MASK, 0);
    GPIO_Control(POWER_BUTTON_GPIO_PORT, CSK_GPIO_MODE_PULL_UP | CSK_GPIO_DEBOUNCE_DISABLE, POWER_BUTTON_PIN_MASK);

    // 配置电源锁存引脚 (PB3) 为输出并设置为高电平
    IOMuxManager_PinConfigure(POWER_IOMUX_PAD, POWER_LATCH_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(POWER_LATCH_GPIO_PORT, POWER_LATCH_PIN_MASK, 1);
    GPIO_PinWrite(POWER_LATCH_GPIO_PORT, POWER_LATCH_PIN_MASK, 1);

    LISA_LOGI(TAG, "Power GPIO initialized - Button: PB%d, Latch: PB%d", POWER_BUTTON_PIN_NUM, POWER_LATCH_PIN_NUM);
}

static bool power_button_pressed(void)
{
    return GPIO_PinRead(POWER_BUTTON_GPIO_PORT, POWER_BUTTON_PIN_MASK) == 0;
}

static bool power_latch_set(bool state)
{
    return GPIO_PinWrite(POWER_LATCH_GPIO_PORT, POWER_LATCH_PIN_MASK, state ? 1 : 0);
}

void power_init(const power_config_t *config)
{
    if (config && config->on_shutdown) {
        shutdown_cb = config->on_shutdown;
    }

    power_gpio_init();
}

bool power_wait_settle(void)
{
    if (get_usb_status() == USB_STATUS_PLUG) {
        power_latch_set(true);
        LISA_LOGI(TAG, "USB connected, power on directly");
        return true;
    }

    uint64_t start_time = lisa_os_get_tick_ms();
    while (1) {
        uint64_t elapsed = lisa_os_get_tick_ms() - start_time;

        if (!power_button_pressed()) {
            LISA_LOGI(TAG, "Power button released at %llu ms before settle time", elapsed);
            return false;
        }

        if (elapsed >= POWER_BUTTON_HOLD_TIME_MS) {
            LISA_LOGI(TAG, "Power button held for required settle time: %llu ms", elapsed);
            break;
        }

        lisa_thread_mdelay(POWER_SAMPLE_INTERVAL_MS);
    }

    power_latch_set(true);
    LISA_LOGI(TAG, "Latch set, system powered on");

    return true;
}

void power_shutdown(void)
{
    LISA_LOGI(TAG, "Executing system shutdown...");

    if (shutdown_cb) {
        LISA_LOGI(TAG, "Calling shutdown callback...");
        shutdown_cb();
    }

    LISA_LOGI(TAG, "Goodbye!");
    power_latch_set(false);
}
