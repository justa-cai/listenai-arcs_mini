#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "brightness"

#include "lisa_log.h"
#include "lisa_kv.h"
#include "lisa_display.h"

#include "kv.h"
#include "IOMuxManager.h"
#include "Driver_I2C.h"
#include "board.h"

#define DEFAULT_BRIGHTNESS 70

void service_brightness_set(int brightness);
static lisa_device_t *s_display_device = NULL;

void service_brightness_init(void)
{
    s_display_device = lisa_device_get("display");
    if (!lisa_device_ready(s_display_device)) {
        LOGE("Display device not ready");
        s_display_device = NULL;
        return;
    }

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    board_display_composite_init();
#endif

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config =
            {
                .spi_4wire =
                    {
                        .spi_dev = lisa_device_get("spi0"),
#ifndef CONFIG_LISA_DISPLAY_COMPOSITE
                        .cs_gpio = gpioa_dev,
                        .cs_pin = LCD_CS_PIN,
#endif
                        .dc_gpio = gpioa_dev,
                        .dc_pin = LCD_CD_PIN,
                        .spi_freq = 50 * 1000 * 1000,
                    },
            },
        .backlight =
            {
                .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                .config.pwm =
                    {
                        .channel = 1,
                        .dev = lisa_device_get("pwm0"),
                        .freq = 2000,
                    },
            },
        .rst_gpio = gpiob_dev,
        .rst_pin = LCD_RST_PIN,
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
        .composite_activate = board_display_composite_activate,
        .composite_deactivate = board_display_composite_deactivate,
#endif
    };

    lisa_display_attach_bus(s_display_device, &display_config);

    int brightness = DEFAULT_BRIGHTNESS;
    if (lisa_kv_get_int(KV_KEY_USER_BRIGHTNESS, &brightness) != 0) {
        brightness = DEFAULT_BRIGHTNESS;
    }
    lisa_display_blanking_on(s_display_device);
    service_brightness_set(brightness);

    LOGI("Brightness service initialized");
}

void service_brightness_set(int brightness)
{
    if (brightness < 0) {
        brightness = 0;
    }
    if (brightness > 100) {
        brightness = 100;
    }

    lisa_kv_set_int(KV_KEY_USER_BRIGHTNESS, brightness);

    if (s_display_device) {
        int hw_brightness = brightness;
        lisa_display_set_brightness(s_display_device, hw_brightness);
        LOGI("Brightness set to %d (hw: %d)", brightness, hw_brightness);
    } else {
        LOGW("Display device not available");
    }
}

int service_brightness_get(void)
{
    int brightness = DEFAULT_BRIGHTNESS;

    if (lisa_kv_get_int(KV_KEY_USER_BRIGHTNESS, &brightness) != 0) {
        brightness = DEFAULT_BRIGHTNESS;
    }

    return brightness;
}
