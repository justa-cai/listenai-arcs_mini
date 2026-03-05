/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_touch.h"
#include "lv_demos.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "FreeRTOSConfig.h"
#include "IOMuxManager.h"
#include "board.h"

#define LOG_TAG "lvgl8_demo_widgets"
#include <lisa_log.h>

#define TOUCH_DEVICE     "touch_cst328"
#define I2C_DEVICE       "i2c0"

/*
    为满足不同板型示例场景，重定向设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD_CS_PIN 5
#define LCD_SPI_CLK_PIN 3
#define LCD_SPI_DATA_PIN 1

/* Touch I2C 引脚定义 */
#define LISA_TOUCH_I2C_SDA_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SDA_PIN   22
#define LISA_TOUCH_I2C_SDA_FUNC  8

#define LISA_TOUCH_I2C_SCL_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SCL_PIN   23
#define LISA_TOUCH_I2C_SCL_FUNC  8

/* Touch GPIO 引脚定义 */
#define LISA_TOUCH_I2C_RST_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_RST_PIN   25
#define LISA_TOUCH_I2C_RST_FUNC  0

#define LISA_TOUCH_I2C_INT_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_INT_PIN   24
#define LISA_TOUCH_I2C_INT_FUNC  0

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_RST_PORT, LISA_TOUCH_I2C_RST_PIN, LISA_TOUCH_I2C_RST_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_INT_PORT, LISA_TOUCH_I2C_INT_PIN, LISA_TOUCH_I2C_INT_FUNC);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SDA_PORT, LISA_TOUCH_I2C_SDA_PIN, LISA_TOUCH_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SCL_PORT, LISA_TOUCH_I2C_SCL_PIN, LISA_TOUCH_I2C_SCL_FUNC);
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
#endif

static void task_ui(void *pvParameters)
{
    extern void lv_demo_widgets(void);
    lv_demo_widgets();

    LISA_LOGI(LOG_TAG, "LVGL8 widgets start");

    while (1) {
        uint32_t wait_time = lv_task_handler();
        lisa_thread_mdelay(wait_time);
    }
}

int main(int argc, char **argv)
{
    lv_init();

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
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
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM, .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
                      .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lisa_display_attach_bus(display_device, &display_config);

    lv_port_disp_init(display_device);

    lisa_device_t *touch_dev = lisa_device_get(TOUCH_DEVICE);
    if (!lisa_device_ready(touch_dev)) {
        printf("Error: %s device not ready\n", TOUCH_DEVICE);
        return -1;
    }

    /* 获取I2C设备 */
    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        printf("Error: %s device not ready\n", I2C_DEVICE);
        return -1;
    }

    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config = {
            .i2c = {
                .i2c_dev = i2c_dev,
                .int_gpio = gpioa_dev,
                .int_pin = LISA_TOUCH_I2C_INT_PIN,
                .rst_gpio = gpioa_dev,
                .rst_pin = LISA_TOUCH_I2C_RST_PIN,
            }
        }
    };
    int ret = lisa_touch_attach_bus(touch_dev, &bus_config);
    if (ret != 0) {
        printf("Error: Touch bus attach failed (code: %d)\n", ret);
        return -1;
    }
    lv_port_indev_init(touch_dev);

    xTaskCreate(task_ui, "task_ui", 4 * 1024, NULL, configMAX_PRIORITIES - 2, NULL);

    return 0;
}

