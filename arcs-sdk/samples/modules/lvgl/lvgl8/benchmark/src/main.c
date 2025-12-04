#include <stdio.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "lv_demos.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"
#include "board.h"

void *display_device = NULL;

static void task_ui(void *pvParameters)
{
	lv_demo_benchmark();

	LOGI("LVGL benchmark start\r\n");

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
			.pad = LISA_DISPLAY_RESET_PORT,
			.pin = LISA_DISPLAY_RESET_PIN,
			.func = LISA_DISPLAY_RESET_FUNC,
		},
		.blacklight = {
			.pin = {
				.pad = LISA_DISPLAY_BL_PWM_PORT,
				.pin = LISA_DISPLAY_BL_PWM_PIN,
				.func = LISA_DISPLAY_BL_PWM_FUNC,
			},
			.dev = GPT0_PWM(),
			.channel = LISA_DISPLAY_BL_CHANNEL,
			.freq = 1000,
		},
		.trans_config = {
			.spi_4line = {
				.spi_dev = LISA_DISPLAY_SPI_DEV,
				.spi_tx_dma_ch = 3,
				.spi_sck_freq = 25000000,
				.spi_pins = {
					.cs = {
						.pad = LISA_DISPLAY_SPI_CS_PORT,
						.pin = LISA_DISPLAY_SPI_CS_PIN,
						.func = LISA_DISPLAY_SPI_CS_FUNC,
					},
					.clk = {
						.pad = LISA_DISPLAY_SPI_CLK_PORT,
						.pin = LISA_DISPLAY_SPI_CLK_PIN,
						.func = LISA_DISPLAY_SPI_CLK_FUNC,
					},
					.sda = {
						.pad = LISA_DISPLAY_SPI_SDA_PORT,
						.pin = LISA_DISPLAY_SPI_SDA_PIN,
						.func = LISA_DISPLAY_SPI_SDA_FUNC,
					},
					.dc = {
						.pad = LISA_DISPLAY_SPI_DC_PORT,
						.pin = LISA_DISPLAY_SPI_DC_PIN,
						.func = LISA_DISPLAY_SPI_DC_FUNC,
					},
				}
			}
		}
	};
	lv_port_disp_init(&disp_config);
	touch_hw_config_t touch_config = {
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
	lv_port_indev_init(&touch_config);
	lisa_display_blanking_off(lisa_display_get());

	xTaskCreate(task_ui, "task_ui", 4*1024, NULL, configMAX_PRIORITIES - 2, NULL);

	return 0;
}

