#include <stdio.h>
#include <stdlib.h>
#include "IOMuxManager.h"
#include "lisa_display.h"
#include "lisa_touch.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"

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

    display_hw_config_t disp_config = {
        .reset = {
            .pad = CSK_IOMUX_PAD_A,
            .pin = 1,
            .func = CSK_IOMUX_FUNC_ALTER1,
        },
        .blacklight = {
            .pin = {
                .pad = CSK_IOMUX_PAD_A,
                .pin = 0,
                .func = CSK_IOMUX_FUNC_ALTER12,
            },
            .dev = GPT0_PWM(),
            .channel = 0,
            .freq = 1000,
        },
        .trans_config = {
            .spi_4line = {
                .spi_dev = SPI1(),
                .spi_tx_dma_ch = 3,
                .spi_pins = {
                    .cs = {
                        .pad = CSK_IOMUX_PAD_B,
                        .pin = 5,
                        .func = CSK_IOMUX_FUNC_ALTER6,
                    },
                    .clk = {
                        .pad = CSK_IOMUX_PAD_B,
                        .pin = 3,
                        .func = CSK_IOMUX_FUNC_ALTER6,
                    },
                    .sda = {
                        .pad = CSK_IOMUX_PAD_B,
                        .pin = 1,
                        .func = CSK_IOMUX_FUNC_ALTER6,
                    },
                    .dc = {
                        .pad = CSK_IOMUX_PAD_B,
                        .pin = 0,
                        .func = CSK_IOMUX_FUNC_DEFAULT,
                    },
                }
            }
        }
    };
    if (lisa_display_create(&disp_config) == NULL) {
        CLOGE("lisa_display_create fail.\r\n");
        return -1;
    }

    /**
     * AXS15231b模组，需要LCD初始化后，touch才能正常通信
     */
    // touch_device = lisa_touch_create();
    touch_hw_config_t config = {
        .i2c_dev = I2C0(),
        .i2c_pins = {
            .sda = {
                .sda_pad = CSK_IOMUX_PAD_A,
                .sda_pin = 22,
                .sda_func = CSK_IOMUX_FUNC_ALTER8,
            },
            .scl = {
                .scl_pad = CSK_IOMUX_PAD_A,
                .scl_pin = 23,
                .scl_func = CSK_IOMUX_FUNC_ALTER8,
            },
        },
        .gpio_pins.reset.reset_pad = CSK_IOMUX_PAD_A,
        .gpio_pins.reset.reset_pin = 25,
        .gpio_pins.reset.reset_func = CSK_IOMUX_FUNC_DEFAULT,
        .gpio_pins.intr.int_pad = CSK_IOMUX_PAD_A,
        .gpio_pins.intr.int_pin = 24,
        .gpio_pins.intr.int_func = CSK_IOMUX_FUNC_DEFAULT,
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
