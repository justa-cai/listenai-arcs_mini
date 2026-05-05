/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "IOMuxManager.h"
#include "board.h"
#include "lisa_device.h"
#include "lisa_display.h"
#include "lv_port_disp.h"
#include "lvgl.h"

#define LOG_TAG "lvgl8_sample_rlottie"
#include <lisa_log.h>

#define RLOTTIE_MARGIN      16
#define MIN_RLOTTIE_SIZE    96
#define UI_TASK_STACK_DEPTH (8 * 1024)

/*
    为满足不同板型示例场景，重定向设备的 pinmux 配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB

/* arcs_evb: LCD 与 arcs_mini 引脚不同，需要重定义并覆盖 weak pinmux 函数 */
#define LCD_CS_PIN       5
#define LCD_SPI_CLK_PIN  3
#define LCD_SPI_DATA_PIN 1

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, CSK_IOMUX_FUNC_ALTER12);
}

#else /* arcs_mini 及其他板型 */

/*
 * arcs_mini: LCD_CS/CD/SPI/RST/PWM 引脚已由 boards/arcs_mini/pinmux.h 定义，
 * pinmux.c 提供 weak 实现，无需在此覆盖。
 */

#endif

extern const uint8_t lv_example_rlottie_approve[];

static lisa_device_t *display_device;

static lv_coord_t get_lottie_size(void)
{
    lv_coord_t min_res = LV_MIN(lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_coord_t max_size = min_res - RLOTTIE_MARGIN;

    if (max_size < MIN_RLOTTIE_SIZE) {
        return MIN_RLOTTIE_SIZE;
    }

    return max_size;
}

static void create_rlottie_demo_ui(void)
{
    lv_coord_t lottie_size = get_lottie_size();
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *lottie;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lottie = lv_rlottie_create_from_raw(screen,
                                        lottie_size,
                                        lottie_size,
                                        (const char *)lv_example_rlottie_approve);
    lv_obj_center(lottie);
}

static void task_ui(void *pvParameters)
{
    LV_UNUSED(pvParameters);

    create_rlottie_demo_ui();

    LISA_LOGI(LOG_TAG, "LVGL8 rlottie start");

    lv_task_handler();
    lisa_display_blanking_off(display_device);
    lisa_display_set_brightness(display_device, 100);

    while (1) {
        uint32_t wait_time = lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(wait_time));
    }
}

int main(int argc, char **argv)
{
    lisa_device_t *gpioa_dev;
    lisa_device_t *gpiob_dev;
    lisa_display_config_t display_config;
    int ret;

    LV_UNUSED(argc);
    LV_UNUSED(argv);

    lv_init();

    gpioa_dev = lisa_device_get("gpioa");
    gpiob_dev = lisa_device_get("gpiob");

#ifdef CONFIG_BOARD_ARCS_EVB
    display_config = (lisa_display_config_t){
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi1"),
                               .cs_gpio = gpiob_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpiob_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,
                           }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
                      .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };
#else
    display_config = (lisa_display_config_t){
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi0"),
                               .cs_gpio = gpioa_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpioa_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,
                           }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
                      .config.pwm = {.channel = 1, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpiob_dev,
        .rst_pin = LCD_RST_PIN,
    };
#endif

    display_device = lisa_device_get("display");
    if (display_device == NULL) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    ret = lisa_display_attach_bus(display_device, &display_config);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Display bus attach failed: %d", ret);
        return ret;
    }

    lv_port_disp_init(display_device);

    /* arcs_mini 无触摸屏，此 sample 仅初始化显示，不初始化 indev。 */

    xTaskCreate(task_ui, "task_ui", UI_TASK_STACK_DEPTH, NULL, configMAX_PRIORITIES - 2, NULL);

    return 0;
}
