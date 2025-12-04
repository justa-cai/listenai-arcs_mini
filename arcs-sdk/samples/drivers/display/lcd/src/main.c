#include <stdio.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "lisa_display.h"
#include "FreeRTOS.h"
#include "task.h"
#include "heap_private.h"
#include "sysheap.h"
#include "IOMuxManager.h"
#include "Driver_SPI.h"
#include "Driver_GPT_PWM.h"
#include "board.h"

enum color {
	COLOR_RED,
	COLOR_GREEN,
	COLOR_BLUE,
	COLOR_CUSTOM
};

static uint16_t get_rgb565_color(enum color rgb, uint8_t grey)
{
	uint16_t color = 0;
	uint16_t grey_5bit;

	switch (rgb) {
	case COLOR_RED:
		color = 0xF800u;
		break;
	case COLOR_GREEN:
		color = 0x07E0u;
		break;
	case COLOR_BLUE:
		color = 0x001Fu;
		break;
	case COLOR_CUSTOM:
		grey_5bit = grey & 0x1Fu;
		/* shift the green an extra bit, it has 6 bits */
		color = grey_5bit << 11 | grey_5bit << (5 + 1) | grey_5bit;
		break;
	}
	return color;
}

static void fill_buffer_rgb565(enum color rgb, uint8_t grey, uint8_t *buf, size_t buf_size)
{
	uint16_t color = get_rgb565_color(rgb, grey);

	//小端模式
	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(buf + idx + 1) = (color >> 8) & 0xFFu;
		*(buf + idx + 0) = (color >> 0) & 0xFFu;
	}
}

int main(int argc, char **argv)
{
	void *display_device = NULL;
	struct display_capabilities caps;
	struct display_buffer_descriptor desc;
	uint8_t *image_buf = NULL;
	uint8_t cnt = 0;
	uint8_t pwm = 0;

	LOGI("display qspi axs15231b \r\n");

	display_hw_config_t config = {
		.reset = {
			.pad = LISA_DISPLAY_RESET_PORT,
			.pin = LISA_DISPLAY_RESET_PIN,
			.func = LISA_DISPLAY_RESET_FUNC,
		},
		.te = {
			.pad = LISA_DISPLAY_TE_PORT,
			.pin = LISA_DISPLAY_TE_PIN,
			.func = LISA_DISPLAY_TE_FUNC,
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
	display_device = lisa_display_create(&config);
	if(display_device == NULL){
		LOGE("lisa_display_create fail \r\n");
		return -1;
	}

	lisa_display_get_capabilities(display_device, &caps);
	if(caps.current_pixel_format != PIXEL_FORMAT_RGB_565){
		LOGE("display format is not rgb565 \r\n");
		return -1;
	}

	LOGI("display capabilities:width = %d, height = %d format = 0x%x \r\n", caps.x_resolution, caps.y_resolution, caps.current_pixel_format);

	desc.width = caps.x_resolution;
	desc.height = caps.y_resolution;
	desc.pitch = desc.width * 2;
	desc.buf_size = desc.height * desc.pitch;

	lisa_display_blanking_off(display_device);

	image_buf = (uint8_t *)exram_malloc(32, desc.buf_size);
    if(image_buf == NULL){
        LOGE("[%s] malloc fail \r\n", __func__);
    }

    fill_buffer_rgb565(COLOR_RED, 0x00, image_buf, desc.buf_size);
	while (1) {
		switch(cnt) {
			case COLOR_RED:
				fill_buffer_rgb565(COLOR_RED, 0x00, image_buf, desc.buf_size);
				break;
			case COLOR_GREEN:
				fill_buffer_rgb565(COLOR_GREEN, 0x00, image_buf, desc.buf_size);
				break;
			case COLOR_BLUE:
				fill_buffer_rgb565(COLOR_BLUE, 0x00, image_buf, desc.buf_size);
				break;
			default:
				break;
		}

		cnt = (cnt + 1) % 3;
		pwm = (pwm + 1) % 100;
		lisa_display_set_brightness(display_device, pwm);
		
		lisa_display_write(display_device, 0, 0, &desc, image_buf);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	return 0;
}
