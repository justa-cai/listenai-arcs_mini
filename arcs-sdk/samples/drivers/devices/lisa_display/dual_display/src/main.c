/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lisa_device.h>
#include <lisa_display.h>
#include <lisa_mem.h>
#include <stddef.h>
#include <stdint.h>
#include "IOMuxManager.h"
// #include "lisa_gpio.h"
// #include "board.h"

#define LOG_TAG "dual_display"
#include <lisa_log.h>

/*
    为满足不同板型示例场景，重定向设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB

/* Display0 (ST7789P3) - SPI 4-Wire */
#define LCD0_RST_PIN 1
#define LCD0_CS_PIN 5
#define LCD0_DC_PIN 0
#define LCD0_SPI_CLK_PIN 3
#define LCD0_SPI_DATA_PIN 1
#define LCD0_BL_PWM_CHANNEL 0

/* Display1 (ST7789P3) - SPI 4-Wire */
#define LCD1_SPI_CLK_PIN 15
#define LCD1_DC_PIN 13
#define LCD1_CS_PIN 12
#define LCD1_SPI_DATA_PIN 14
#define LCD1_BL_PWM_CHANNEL 3

void lisa_gpioa_pinmux()
{
    /* Display0 RST */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD0_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    /* Display1 RST */
    // IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    /* Display1 CS */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD1_DC_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_gpiob_pinmux()
{
    /* Display0 DC and CS */
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
    /* Display0 SPI CLK and DATA */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD0_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD0_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 0, 12);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 12);
}

#endif

/* Display context structure */
typedef struct {
    lisa_device_t *device;
    uint16_t width;
    uint16_t height;
    uint16_t *framebuffer;
} display_context_t;

static display_context_t display0_ctx;
static display_context_t display1_ctx;

/* Fill buffer with solid color */
static void fill_buffer_with_color(uint16_t color, uint16_t *buf, size_t pixel_count)
{
    for (size_t i = 0; i < pixel_count; i++) {
        buf[i] = color;
    }
}

/* Initialize Display0 (ST7789P3 + SPI 4-Wire) */
static int init_display0(void)
{
    LISA_LOGI(LOG_TAG, "Initializing Display0 (ST7789P3)...");

    lisa_device_t *display0 = lisa_device_get("display");
    if (!display0) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_device_t *gpiob = lisa_device_get("gpiob");
    lisa_device_t *spi1 = lisa_device_get("spi1");
    lisa_device_t *pwm0 = lisa_device_get("pwm0");

    if (!gpioa || !gpiob || !spi1 || !pwm0) {
        LISA_LOGE(LOG_TAG, "Failed to get devices for Display0");
        return -1;
    }

    lisa_display_config_t config0 = {
        .panel_name = "st7789p3",
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {
            .spi_4wire = {
                .spi_dev = spi1,
                .cs_gpio = gpiob,
                .cs_pin = LCD0_CS_PIN,
                .dc_gpio = gpiob,
                .dc_pin = LCD0_DC_PIN,
                .spi_freq = 50 * 1000 * 1000,  // 50MHz
            }
        },
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .dev = pwm0,
                .channel = LCD0_BL_PWM_CHANNEL,
                .freq = 2000,
            }
        },
        .rst_gpio = gpioa,
        .rst_pin = LCD0_RST_PIN,
    };

    int ret = lisa_display_attach_bus(display0, &config0);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to attach bus for Display0: %d", ret);
        return ret;
    }

    lisa_display_capabilities_t caps;
    ret = lisa_display_get_capabilities(display0, &caps);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to get capabilities for Display0");
        return ret;
    }

    size_t buffer_size = (size_t)caps.width * caps.height * sizeof(uint16_t);
    uint16_t *framebuffer = lisa_mem_alloc(buffer_size);
    if (!framebuffer) {
        LISA_LOGE(LOG_TAG, "Failed to allocate framebuffer for Display0");
        return -1;
    }

    display0_ctx.device = display0;
    display0_ctx.width = caps.width;
    display0_ctx.height = caps.height;
    display0_ctx.framebuffer = framebuffer;

    lisa_display_blanking_off(display0);
    lisa_display_set_brightness(display0, 80);

    LISA_LOGI(LOG_TAG, "Display0 initialized: %dx%d", caps.width, caps.height);
    return 0;
}

/* Initialize Display1 (AXS15231B + QSPI) */
static int init_display1(void)
{
    LISA_LOGI(LOG_TAG, "Initializing Display1 (AXS15231B)...");

    lisa_device_t *display1 = lisa_device_get("display1");
    if (!display1) {
        LISA_LOGE(LOG_TAG, "Failed to get display1 device");
        return -1;
    }

    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_device_t *spi0 = lisa_device_get("spi0");
    lisa_device_t *pwm0 = lisa_device_get("pwm0");

    if (!gpioa || !spi0 || !pwm0) {
        LISA_LOGE(LOG_TAG, "Failed to get devices for Display1");
        return -1;
    }

    lisa_display_config_t config1 = {
        .panel_name = "st7789p3",
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {
            .spi_4wire = {
                .spi_dev = spi0,
                .cs_gpio = gpioa,
                .cs_pin = LCD1_CS_PIN,
                .dc_gpio = gpioa,
                .dc_pin = LCD1_DC_PIN,
                .spi_freq = 50 * 1000 * 1000,  // 50MHz
            }
        },
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .dev = pwm0,
                .channel = LCD1_BL_PWM_CHANNEL,
                .freq = 2000,
            }
        },
        // .rst_gpio = gpioa,
        // .rst_pin = LCD1_RST_PIN,
    };

    int ret = lisa_display_attach_bus(display1, &config1);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to attach bus for Display1: %d", ret);
        return ret;
    }

    lisa_display_capabilities_t caps;
    ret = lisa_display_get_capabilities(display1, &caps);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to get capabilities for Display1");
        return ret;
    }

    size_t buffer_size = (size_t)caps.width * caps.height * sizeof(uint16_t);
    uint16_t *framebuffer = lisa_mem_alloc(buffer_size);
    if (!framebuffer) {
        LISA_LOGE(LOG_TAG, "Failed to allocate framebuffer for Display1");
        return -1;
    }

    display1_ctx.device = display1;
    display1_ctx.width = caps.width;
    display1_ctx.height = caps.height;
    display1_ctx.framebuffer = framebuffer;

    lisa_display_blanking_off(display1);
    lisa_display_set_brightness(display1, 60);

    LISA_LOGI(LOG_TAG, "Display1 initialized: %dx%d", caps.width, caps.height);
    return 0;
}

/* Update Display0 with color */
static void display0_show_color(uint16_t color)
{
    fill_buffer_with_color(color, display0_ctx.framebuffer, 
                          (size_t)display0_ctx.width * display0_ctx.height);

    lisa_display_buffer_desc_t desc = {
        .width = display0_ctx.width,
        .height = display0_ctx.height,
        .buf_size = (size_t)display0_ctx.width * display0_ctx.height * sizeof(uint16_t),
    };

    lisa_display_write(display0_ctx.device, 0, 0, &desc, display0_ctx.framebuffer);
}

/* Update Display1 with color */
static void display1_show_color(uint16_t color)
{
    fill_buffer_with_color(color, display1_ctx.framebuffer,
                          (size_t)display1_ctx.width * display1_ctx.height);

    lisa_display_buffer_desc_t desc = {
        .width = display1_ctx.width,
        .height = display1_ctx.height,
        .buf_size = (size_t)display1_ctx.width * display1_ctx.height * sizeof(uint16_t),
    };

    lisa_display_write(display1_ctx.device, 0, 0, &desc, display1_ctx.framebuffer);
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "Dual Display sample started");

    /* Initialize both displays */
    if (init_display0() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize Display0");
        return -1;
    }

    if (init_display1() != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize Display1");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Both displays initialized successfully");

    /* Color sequence for demonstration */
    uint16_t colors[] = {
        LISA_DISPLAY_COLOR_RED,
        LISA_DISPLAY_COLOR_GREEN,
        LISA_DISPLAY_COLOR_BLUE,
        LISA_DISPLAY_COLOR_YELLOW,
        LISA_DISPLAY_COLOR_CYAN,
        LISA_DISPLAY_COLOR_MAGENTA,
        LISA_DISPLAY_COLOR_WHITE,
    };
    int num_colors = sizeof(colors) / sizeof(colors[0]);

    /* Display different colors on each screen */
    int idx0 = 0;  // Display0 starts with RED
    int idx1 = 3;  // Display1 starts with YELLOW
    int brightness = 20;
    int brightness_step = 10;

    while (1) {
        /* Update Display0 */
        display0_show_color(colors[idx0]);
        LISA_LOGI(LOG_TAG, "Display0: color index %d", idx0);

        /* Update Display1 with different color */
        display1_show_color(colors[idx1]);
        LISA_LOGI(LOG_TAG, "Display1: color index %d", idx1);

        lisa_display_set_brightness(display0_ctx.device, brightness);
        lisa_display_set_brightness(display1_ctx.device, brightness);
        LISA_LOGI(LOG_TAG, "Display brightness: %d", brightness);

        /* Wait 2 seconds */
        lisa_thread_mdelay(2000);

        /* Move to next colors */
        idx0 = (idx0 + 1) % num_colors;
        idx1 = (idx1 + 1) % num_colors;

        brightness += brightness_step;
        if (brightness >= 100) {
            brightness = 100;
            brightness_step = -brightness_step;
        }
        else if (brightness <= 10) {
            brightness = 10;
            brightness_step = -brightness_step;
        }
    }

    return 0;
}
