#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lvgl.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"

#include "lisa_log.h"
#include "lisa_display.h"
#include "lisa_touch.h"

#include "IOMuxManager.h"
#include "Driver_I2C.h"

#include "board.h"

#include "lisa_ui_invoke.h"

#include "lisa_kv.h"
#include "kv.h"

#ifdef CONFIG_LV_CUSTOM_TASK_CPU_PER
extern void lvgl_task_cpu_percent_peroid500ms(void);
#endif
// #include "app_display.h"

#define TAG "view_main"

#define TOUCH_DEVICE "touch_cst328"
#define I2C_DEVICE   "i2c0"

/*
    为满足不同板型示例场景，重定向设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD_CS_PIN       5
#define LCD_SPI_CLK_PIN  3
#define LCD_SPI_DATA_PIN 1

/* Touch I2C 引脚定义 */
#define LISA_TOUCH_I2C_SDA_PORT CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SDA_PIN  22
#define LISA_TOUCH_I2C_SDA_FUNC 8

#define LISA_TOUCH_I2C_SCL_PORT CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SCL_PIN  23
#define LISA_TOUCH_I2C_SCL_FUNC 8

/* Touch GPIO 引脚定义 */
#define LISA_TOUCH_I2C_RST_PORT CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_RST_PIN  25
#define LISA_TOUCH_I2C_RST_FUNC 0

#define LISA_TOUCH_I2C_INT_PORT CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_INT_PIN  24
#define LISA_TOUCH_I2C_INT_FUNC 0

#endif

#define LISA_KV_KEY_LANGUAGE "user.locale"

int lisa_ui_lvgl_init()
{
    lv_init();

    char *locale = NULL;

    int r = lisa_kv_get_string(LISA_KV_KEY_LANGUAGE, &locale);
    if (r || locale == NULL) {
        lv_i18n_set_locale("zh-CN");
        LOGW("locale not found, use zn-CN");
    } else {
        LOGI("locale %s found", locale);
        lv_i18n_set_locale(locale);
        lisa_kv_free(locale);
    }

#ifdef CONFIG_BOARD_ARCS_EVB
    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");
#endif

#if 0 // dead code
    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config =
            {
                .spi_4wire =
                    {
                        .spi_dev = lisa_device_get("spi1"),
                        .cs_gpio = gpiob_dev,
                        .cs_pin = LCD_CS_PIN,
                        .dc_gpio = gpiob_dev,
                        .dc_pin = LCD_CD_PIN,
                        .spi_freq = 50 * 1000 * 1000,
                    },
            },
        .backlight =
            {
                .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                .config.pwm =
                    {
                        .channel = 0,
                        .dev = lisa_device_get("pwm0"),
                        .freq = 2000,
                    },
            },
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };
#endif

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lv_port_disp_init(display_device);

#ifdef CONFIG_BOARD_ARCS_EVB
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
        .config =
            {
                .i2c =
                    {
                        .i2c_dev = i2c_dev,
                        .int_gpio = gpioa_dev,
                        .int_pin = LISA_TOUCH_I2C_INT_PIN,
                        .rst_gpio = gpioa_dev,
                        .rst_pin = LISA_TOUCH_I2C_RST_PIN,
                    },
            },
    };

    int ret = lisa_touch_attach_bus(touch_dev, &bus_config);
    if (ret != 0) {
        printf("Error: Touch bus attach failed (code: %d)\n", ret);
        return -1;
    }
    lv_port_indev_init(touch_dev);
#endif

    lisa_display_blanking_on(display_device);

    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_scr_load(scr);
    lv_task_handler();
    lisa_display_blanking_off(display_device);

    return 0;
}

static void lisa_ui_invoke_worker(void *arg, uint32_t len)
{
    uint32_t sleep = lv_task_handler();

    LISA_UI_INVOKE_UI_DELAYED(lisa_ui_invoke_worker, arg, len, sleep);
}

int lisa_ui_lvgl_run(void)
{
    LISA_UI_INVOKE_UI_DELAYED(lisa_ui_invoke_worker, NULL, 0, 0);

    return 0;
}
