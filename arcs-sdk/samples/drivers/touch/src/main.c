#include <stdio.h>
#include <stdlib.h>
#include "IOMuxManager.h"
#include "lisa_display.h"
#include "lisa_touch.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"
#include "board.h"

void *touch_device = NULL;
static volatile bool touch_int_trigger = false;

/**
        该函数在中断上下文
 */
static void touch_int_callback(void)
{
    touch_int_trigger = true;
}

int main(int argc, char **argv)
{
    uint16_t x, y;

    LOGD("touch sample start.\r\n");

    /* 对于 ST7789P3 方案，触摸初始化无需先点亮 LCD，可直接初始化触摸 */
    touch_hw_config_t config = {
        .i2c_dev = LISA_TOUCH_I2C_DEV,
        .i2c_pins = {
            .sda = {
                .sda_pad = LISA_TOUCH_I2C_SDA_PORT,
                .sda_pin = LISA_TOUCH_I2C_SDA_PIN,
                .sda_func = LISA_TOUCH_I2C_SDA_FUNC,
            },
            .scl = {
                .scl_pad = LISA_TOUCH_I2C_SCL_PORT,
                .scl_pin = LISA_TOUCH_I2C_SCL_PIN,
                .scl_func = LISA_TOUCH_I2C_SCL_FUNC,
            },
        },
        .gpio_pins.reset.reset_pad = LISA_TOUCH_I2C_RST_PORT,
        .gpio_pins.reset.reset_pin = LISA_TOUCH_I2C_RST_PIN,
        .gpio_pins.reset.reset_func = LISA_TOUCH_I2C_RST_FUNC,
        .gpio_pins.intr.int_pad = LISA_TOUCH_I2C_INT_PORT,
        .gpio_pins.intr.int_pin = LISA_TOUCH_I2C_INT_PIN,
        .gpio_pins.intr.int_func = LISA_TOUCH_I2C_INT_FUNC,
    };
    touch_device = lisa_touch_create(&config);
    if (touch_device == NULL) {
        LOGE("lisa_touchy_create fail.\r\n");
        return -1;
    }

    lisa_touch_set_int_callback(touch_device, touch_int_callback);

    while (1) {
        if (touch_int_trigger == true) {
            touch_int_trigger = false;
            bool pressed = false;
            lisa_touch_read_coordinates(touch_device, &x, &y, &pressed);
            LOGI("touch: x=%d, y=%d, pressed=%d.", x, y, pressed);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return 0;
}
