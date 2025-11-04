#include <stdio.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "lv_demos.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"

void *display_device = NULL;

static void task_ui(void *pvParameters)
{
	lv_demo_widgets();

	LOGI("LVGL widgets start\r\n");

	while (1) {
		uint32_t wait_time = lv_task_handler();
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

int main(int argc, char **argv)
{
	lv_init();
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
				.spi_sck_freq = 25000000,
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
	lv_port_disp_init(&disp_config);
	touch_hw_config_t touch_config = {
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
	lv_port_indev_init(&touch_config);
	lisa_display_blanking_off(lisa_display_get());
	
	xTaskCreate(task_ui, "task_ui", 4*1024, NULL, configMAX_PRIORITIES - 2, NULL);

	return 0;
}

