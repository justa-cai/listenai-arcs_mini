/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_touch.h"
#include "benchmark_dual.h"
#include "lv_port_disp_multi.h"
#include "lv_port_indev_multi.h"
#include "IOMuxManager.h"

#define LOG_TAG "lvgl8_benchmark_dual"
#include <lisa_log.h>

#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD0_RST_PIN 1
#define LCD0_CS_PIN 5
#define LCD0_DC_PIN 0
#define LCD0_SPI_CLK_PIN 3
#define LCD0_SPI_DATA_PIN 1
#define LCD0_BL_PWM_CHANNEL 0

#define LCD1_SPI_CLK_PIN 15
#define LCD1_DC_PIN 13
#define LCD1_CS_PIN 12
#define LCD1_SPI_DATA_PIN 14
#define LCD1_BL_PWM_CHANNEL 3

#define TOUCH0_DEVICE    "touch_cst328"
#define TOUCH0_I2C_DEVICE "i2c0"
#define TOUCH1_DEVICE    "touch_cst328_1"
#define TOUCH1_I2C_DEVICE "i2c1"

#define LISA_TOUCH_I2C_SDA_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SDA_PIN   22
#define LISA_TOUCH_I2C_SDA_FUNC  8

#define LISA_TOUCH_I2C_SCL_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SCL_PIN   23
#define LISA_TOUCH_I2C_SCL_FUNC  8

#define LISA_TOUCH_I2C_RST_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_RST_PIN   25
#define LISA_TOUCH_I2C_RST_FUNC  0

#define LISA_TOUCH_I2C_INT_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_INT_PIN   24
#define LISA_TOUCH_I2C_INT_FUNC  0

/*
 * 第二块触摸屏的 RST/INT 引脚需要按实际硬件调整。
 * 默认值仅用于占位，只有 TOUCH1_DEVICE 存在时才会走初始化。
 */
#define LISA_TOUCH1_I2C_RST_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH1_I2C_RST_PIN   26
#define LISA_TOUCH1_I2C_RST_FUNC  0

#define LISA_TOUCH1_I2C_INT_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH1_I2C_INT_PIN   27
#define LISA_TOUCH1_I2C_INT_FUNC  0



void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD0_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_DC_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_RST_PORT, LISA_TOUCH_I2C_RST_PIN, LISA_TOUCH_I2C_RST_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_INT_PORT, LISA_TOUCH_I2C_INT_PIN, LISA_TOUCH_I2C_INT_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH1_I2C_RST_PORT, LISA_TOUCH1_I2C_RST_PIN, LISA_TOUCH1_I2C_RST_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH1_I2C_INT_PORT, LISA_TOUCH1_I2C_INT_PIN, LISA_TOUCH1_I2C_INT_FUNC);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD0_DC_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD0_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER5);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD0_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD0_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SDA_PORT, LISA_TOUCH_I2C_SDA_PIN, LISA_TOUCH_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SCL_PORT, LISA_TOUCH_I2C_SCL_PIN, LISA_TOUCH_I2C_SCL_FUNC);
}

void lisa_i2c1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, 8);

}

void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 0, 12);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 12);
}

#endif

static int init_display0(void)
{
    lisa_device_t *display0 = lisa_device_get("display");
    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_device_t *gpiob = lisa_device_get("gpiob");
    lisa_device_t *spi1 = lisa_device_get("spi1");
    lisa_device_t *pwm0 = lisa_device_get("pwm0");

    lisa_display_config_t config0 = {
        .panel_name = "st7789p3",
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config.spi_4wire = {
            .spi_dev = spi1,
            .cs_gpio = gpiob,
            .cs_pin = LCD0_CS_PIN,
            .dc_gpio = gpiob,
            .dc_pin = LCD0_DC_PIN,
            .spi_freq = 50 * 1000 * 1000,
        },
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .dev = pwm0,
                .channel = LCD0_BL_PWM_CHANNEL,
                .freq = 2000,
            },
        },
        .rst_gpio = gpioa,
        .rst_pin = LCD0_RST_PIN,
    };

    if (!display0 || !gpioa || !gpiob || !spi1 || !pwm0) {
        return -1;
    }

    return lisa_display_attach_bus(display0, &config0);
}

static int init_display1(void)
{
    lisa_device_t *display1 = lisa_device_get("display1");
    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_device_t *spi0 = lisa_device_get("spi0");
    lisa_device_t *pwm0 = lisa_device_get("pwm0");

    lisa_display_config_t config1 = {
        .panel_name = "st7789p3",
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config.spi_4wire = {
            .spi_dev = spi0,
            .cs_gpio = gpioa,
            .cs_pin = LCD1_CS_PIN,
            .dc_gpio = gpioa,
            .dc_pin = LCD1_DC_PIN,
            .spi_freq = 50 * 1000 * 1000,
        },
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .dev = pwm0,
                .channel = LCD1_BL_PWM_CHANNEL,
                .freq = 2000,
            },
        },
    };

    if (!display1 || !gpioa || !spi0 || !pwm0) {
        return -1;
    }

    return lisa_display_attach_bus(display1, &config1);
}

static int init_touch0(void)
{
    lisa_device_t *touch_dev = lisa_device_get(TOUCH0_DEVICE);
    lisa_device_t *i2c_dev = lisa_device_get(TOUCH0_I2C_DEVICE);
    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config.i2c = {
            .i2c_dev = i2c_dev,
            .int_gpio = gpioa,
            .int_pin = LISA_TOUCH_I2C_INT_PIN,
            .rst_gpio = gpioa,
            .rst_pin = LISA_TOUCH_I2C_RST_PIN,
        },
    };

    if (!lisa_device_ready(touch_dev) || !lisa_device_ready(i2c_dev) || !gpioa) {
        return -1;
    }

    return lisa_touch_attach_bus(touch_dev, &bus_config);
}

static int init_touch1(void)
{
    lisa_device_t *touch_dev = lisa_device_get(TOUCH1_DEVICE);
    lisa_device_t *i2c_dev = lisa_device_get(TOUCH1_I2C_DEVICE);
    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config.i2c = {
            .i2c_dev = i2c_dev,
            .int_gpio = gpioa,
            .int_pin = LISA_TOUCH1_I2C_INT_PIN,
            .rst_gpio = gpioa,
            .rst_pin = LISA_TOUCH1_I2C_RST_PIN,
        },
    };

    if (!touch_dev) {
        LISA_LOGW(LOG_TAG, "Touch device %s not found, skip display1 touch", TOUCH1_DEVICE);
        return 1;
    }

    if (!lisa_device_ready(touch_dev) || !lisa_device_ready(i2c_dev) || !gpioa) {
        LISA_LOGW(LOG_TAG, "Touch device %s or I2C1 not ready, skip display1 touch", TOUCH1_DEVICE);
        return 1;
    }

    return lisa_touch_attach_bus(touch_dev, &bus_config);
}

int main(int argc, char **argv)
{
    lv_disp_t *disp0;
    lv_disp_t *disp1;
    lv_indev_t *indev0;
    lv_indev_t *indev1;
    lisa_device_t *display0 = lisa_device_get("display");
    lisa_device_t *display1 = lisa_device_get("display1");
    lisa_device_t *touch0_dev = lisa_device_get(TOUCH0_DEVICE);
    lisa_device_t *touch1_dev = lisa_device_get(TOUCH1_DEVICE);

    lv_init();

    if (init_display0() != 0 || init_display1() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize dual displays");
        return -1;
    }

    disp0 = lv_port_disp_register(display0);
    disp1 = lv_port_disp_register(display1);
    if (!disp0 || !disp1) {
        LISA_LOGE(LOG_TAG, "Failed to register LVGL displays");
        return -1;
    }

    if (init_touch0() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize touch for display0");
        return -1;
    }

    indev0 = lv_port_indev_register(touch0_dev, display0, disp0);
    if (!indev0) {
        LISA_LOGE(LOG_TAG, "Failed to register LVGL touch for display0");
        return -1;
    }

    if (init_touch1() == 0) {
        indev1 = lv_port_indev_register(touch1_dev, display1, disp1);
        if (!indev1) {
            LISA_LOGE(LOG_TAG, "Failed to register LVGL touch for display1");
            return -1;
        }
    }

    lisa_display_blanking_off(display0);
    lisa_display_blanking_off(display1);
    lisa_display_set_brightness(display0, 80);
    lisa_display_set_brightness(display1, 80);

    lv_demo_benchmark_disp0_bind(disp0);
    lv_demo_benchmark_disp1_bind(disp1);
    lv_demo_benchmark_disp0();
    lv_demo_benchmark_disp1();

    LISA_LOGI(LOG_TAG, "LVGL8 dual benchmark start");

    while (1) {
        uint32_t wait_time = lv_timer_handler();
        lisa_thread_mdelay(wait_time);
    }
}
