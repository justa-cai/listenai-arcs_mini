/**
 * @file power_manager.c
 * @brief Power management functions for system boot and shutdown
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#include <stdio.h>

#include "power_manager.h"
#include "lisa_time.h"
#include "lisa_log.h"
#include "lisa_gpio.h"
#include "board.h"

#define TAG "power_mgr"

#define POWER_KEY_PAD CONFIG_POWER_MANAGER_POWER_KEY_GPIO_PAD
#define POWER_EN_PAD  CONFIG_POWER_MANAGER_POWER_EN_GPIO_PAD
#define USB_DET_PAD   CONFIG_POWER_MANAGER_USB_DETECT_GPIO_PAD

static lisa_device_t *power_key_dev = NULL;
static lisa_device_t *power_en_dev = NULL;
static lisa_device_t *usb_det_dev = NULL;

static void (*shutdown_cb)(void) = NULL;

static void power_gpio_init(void)
{
    power_key_dev = lisa_device_get(POWER_KEY_PAD);
    lisa_gpio_configure(power_key_dev, POWER_KEY_PIN, LISA_GPIO_CONFIG_INPUT_PULLUP);

    power_en_dev = lisa_device_get(POWER_EN_PAD);
    lisa_gpio_configure(power_en_dev, POWER_EN_PIN, LISA_GPIO_CONFIG_OUTPUT_LOW);

    usb_det_dev = lisa_device_get(USB_DET_PAD);
    lisa_gpio_configure(usb_det_dev, USB_DET_PIN, LISA_GPIO_CONFIG_INPUT_PULLUP);

    LISA_LOGI(TAG, "Power GPIO initialized - Button: %s(%d), Latch: %s(%d), USB Detect: %s(%d)", POWER_KEY_PAD,
              POWER_KEY_PIN, POWER_EN_PAD, POWER_EN_PIN, USB_DET_PAD, USB_DET_PIN);
}

static bool power_button_pressed(void)
{
    return lisa_gpio_read_pin(power_key_dev, POWER_KEY_PIN) == LISA_GPIO_LOW;
}

static bool power_latch_set(bool state)
{
    return lisa_gpio_write_pin(power_en_dev, POWER_EN_PIN, state ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
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
    if (power_is_usb_plugged()) {
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

bool power_is_usb_plugged(void)
{
    return lisa_gpio_read_pin(usb_det_dev, USB_DET_PIN) == LISA_GPIO_HIGH;
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
