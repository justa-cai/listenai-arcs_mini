#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lisa_log.h"
#include "lisa_display.h"
#include "FreeRTOS.h"
#include "task.h"
#include "heap_private.h"
#include "sysheap.h"
#include "IOMuxManager.h"
#include "Driver_SPI.h"
#include "Driver_GPT_PWM.h"

#include "lv_port_disp.h"
// #include "lv_port_indev.h"
#include "lv_demos.h"
#include "pearl_girl_image.h"
// #include "pearl_girl_atkinson.h"

#define CONFIG_USE_LVGL 0
static void *display_device = NULL;

#if CONFIG_USE_LVGL
lv_obj_t *label = NULL;
uint32_t cnt = 0;
static void user_btn_label_cb(lv_timer_t * timer)
{
	LV_UNUSED(timer);
	if (label) {
		char buf[64];
		snprintf(buf, sizeof(buf), "Hello LVGL %d", cnt++);
		lv_label_set_text(label, buf);
		LOGI("btn label set text: %s", buf);
	}
}

void lv_btn_test(void)
{
	lv_obj_t *btn = lv_btn_create(lv_scr_act());
	lv_obj_set_size(btn, 200, 160);
	lv_obj_center(btn);
	label = lv_label_create(btn);
	lv_label_set_text(label, "Hello World!");
	lv_obj_center(label);
	lv_timer_create(user_btn_label_cb, 1000, NULL);

	/* 在当前屏幕上创建一个黑色方块 */
	lv_obj_t *rect = lv_obj_create(lv_scr_act());   /* 父对象为屏幕 */
	lv_obj_set_size(rect, 100, 100);                /* 宽 100 高 100 */
	lv_obj_set_pos(rect, 20, 20);                   /* x=20,y=20 */

	/* 设置样式为黑色背景、不透明、无边框 */
	lv_obj_set_style_bg_color(rect, lv_color_hex(0x000000), 0);
	lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(rect, 0, 0);

}

static void task_ui(void *pvParameters)
{
	LOGI("LVGL widgets start\r\n");
	// lv_demo_widgets();
	lv_btn_test();

	while (1) {
		uint32_t wait_time = lv_task_handler();
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

#endif

// EPD 测试图案
enum epd_pattern {
	EPD_PATTERN_CLEAR,      // 全白
	EPD_PATTERN_FILL,       // 全黑
	EPD_PATTERN_CHESS,      // 棋盘格
	EPD_PATTERN_STRIPE_H,   // 水平条纹
	EPD_PATTERN_STRIPE_V,   // 垂直条纹
	EPD_PATTERN_FRAME,      // 边框
	EPD_PATTERN_TEXT,       // 文本测试
	EPD_PATTERN_IMAGE       // 图片显示
};

// 填充单色缓冲区 (1bpp - 每个像素1位，8个像素占1字节)
static void fill_buffer_mono(enum epd_pattern pattern, uint8_t *buf, uint16_t width, uint16_t height)
{
	uint32_t buf_size = (width * height) / 8;  // 1bpp
	uint32_t i, x, y;
	uint8_t byte_val;

	// 清空缓冲区
	memset(buf, 0x00, buf_size);
	LOGI("Filling buffer: pattern=%d, width=%d, height=%d, buf_size=%d\r\n",
			pattern, width, height, buf_size);
	switch (pattern) {
	case EPD_PATTERN_CLEAR:
		// 全白 (墨水屏：0xFF = 白色)
		memset(buf, 0xFF, buf_size);
		break;

	case EPD_PATTERN_FILL:
		// 全黑 (墨水屏：0x00 = 黑色)
		memset(buf, 0x00, buf_size);
		break;

	case EPD_PATTERN_CHESS:
		// 棋盘格 8x8
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x += 8) {
				uint32_t byte_idx = (y * width + x) / 8;
				byte_val = 0;

				for (int bit = 0; bit < 8 && (x + bit) < width; bit++) {
					int px = x + bit;
					// 8x8棋盘格
					if (((px / 8) + (y / 8)) % 2 == 0) {
						byte_val |= (1 << (7 - bit));  // 白色
					}
				}
				buf[byte_idx] = byte_val;
			}
		}
		break;

	case EPD_PATTERN_STRIPE_H:
		// 水平条纹
		for (y = 0; y < height; y++) {
			byte_val = (y / 4) % 2 ? 0xFF : 0x00;  // 4像素宽的条纹
			for (x = 0; x < width; x += 8) {
				uint32_t byte_idx = (y * width + x) / 8;
				buf[byte_idx] = byte_val;
			}
		}
		break;

	case EPD_PATTERN_STRIPE_V:
		// 垂直条纹
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x += 8) {
				uint32_t byte_idx = (y * width + x) / 8;
				byte_val = 0;

				for (int bit = 0; bit < 8 && (x + bit) < width; bit++) {
					int px = x + bit;
					if ((px / 4) % 2 == 0) {  // 4像素宽的条纹
						byte_val |= (1 << (7 - bit));
					}
				}
				buf[byte_idx] = byte_val;
			}
		}
		break;

	case EPD_PATTERN_FRAME:
		// 边框
		memset(buf, 0xFF, buf_size);  // 先填白色
		// 画边框 (黑色)
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x += 8) {
				uint32_t byte_idx = (y * width + x) / 8;
				byte_val = 0xFF;

				for (int bit = 0; bit < 8 && (x + bit) < width; bit++) {
					int px = x + bit;
					// 边框区域
					if (y < 10 || y >= height - 10 || px < 10 || px >= width - 10) {
						byte_val &= ~(1 << (7 - bit));  // 黑色
					}
				}
				buf[byte_idx] = byte_val;
			}
		}
		break;

	case EPD_PATTERN_TEXT:
		// 文本测试图案
		// generate_text_image(buf, "UC8253c");
		break;

	case EPD_PATTERN_IMAGE:
		memcpy(buf, epd_image, buf_size);
		// LOGI("Using embedded image data\r\n");
		break;

	default:
		memset(buf, 0xFF, buf_size);  // 默认白色
		break;
	}
}

int main(int argc, char **argv)
{
	struct display_capabilities caps;
	struct display_buffer_descriptor desc;
	uint8_t *image_buf = NULL;
	enum epd_pattern pattern = EPD_PATTERN_CLEAR;
	int pattern_count = 0;

	LOGI("UC8253c E-Paper Display Sample\r\n");


	display_hw_config_t config = {
		.reset = {
			.pad = CSK_IOMUX_PAD_A,
			.pin = 1,
			.func = CSK_IOMUX_FUNC_ALTER1,
		},
		// .blacklight = {
		// 	.pin = {
		// 		.pad = CSK_IOMUX_PAD_A,
		// 		.pin = 0,
		// 		.func = CSK_IOMUX_FUNC_ALTER12,
		// 	},
		// 	.dev = GPT0_PWM(),
		// 	.channel = GPT_CHANNEL0,
		// 	.freq = 1000,
		// },
		.busy = {
			.pad = CSK_IOMUX_PAD_B,
			.pin = 4,
			.func = CSK_IOMUX_FUNC_DEFAULT,
		},
		.trans_config = {
			.spi_4line = {
				.spi_sck_freq = 20*1000*1000, // 20MHz
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

#if CONFIG_USE_LVGL
	lv_init();
	lv_port_disp_init(&config);
	display_device = lv_port_get_display_device();
#else
	display_device = lisa_display_create(&config);
#endif

	if(display_device == NULL){
		LOGE("lisa_display_create fail\r\n");
		return -1;
	}

	lisa_display_get_capabilities(display_device, &caps);
	if(caps.current_pixel_format != PIXEL_FORMAT_MONO_1){
		LOGE("display format is not mono 1bpp, got format = 0x%x\r\n", caps.current_pixel_format);
	}

	LOGI("display capabilities: width = %d, height = %d, format = 0x%x\r\n",
		 caps.x_resolution, caps.y_resolution, caps.current_pixel_format);

	desc.width = caps.x_resolution;
	desc.height = caps.y_resolution;
	desc.pitch = (desc.width + 7) / 8;  // 1bpp: 8个像素占1字节，向上取整
	desc.buf_size = desc.height * desc.pitch;

	LOGI("buffer: width=%d, height=%d, pitch=%d, buf_size=%d\r\n",
		 desc.width, desc.height, desc.pitch, desc.buf_size);

	lisa_display_blanking_off(display_device);

	image_buf = (uint8_t *)exram_malloc(32, desc.buf_size);
	if(image_buf == NULL){
		LOGE("[%s] malloc fail\r\n", __func__);
		return -1;
	}

#if CONFIG_USE_LVGL
	LOGI("Starting pattern cycle...");
	xTaskCreate(task_ui, "task_ui", 4*1024, NULL, configMAX_PRIORITIES - 2, NULL);

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(3000));
		LOGI("tick...");
	}
#endif


	while (1) {
		// 循环显示不同图案
		switch(pattern_count % 8) {
			case 0:
				pattern = EPD_PATTERN_CLEAR;
				LOGI("Displaying: CLEAR (All White)\r\n");
				break;
			case 1:
				pattern = EPD_PATTERN_FILL;
				LOGI("Displaying: FILL (All Black)\r\n");
				break;
			case 2:
				pattern = EPD_PATTERN_CHESS;
				LOGI("Displaying: CHESS (Checkerboard)\r\n");
				break;
			case 3:
				pattern = EPD_PATTERN_STRIPE_H;
				LOGI("Displaying: STRIPE_H (Horizontal Stripes)\r\n");
				break;
			case 4:
				pattern = EPD_PATTERN_STRIPE_V;
				LOGI("Displaying: STRIPE_V (Vertical Stripes)\r\n");
				break;
			case 5:
				pattern = EPD_PATTERN_FRAME;
				LOGI("Displaying: FRAME (Border Frame)\r\n");
				break;
			case 6:
				pattern = EPD_PATTERN_TEXT;
				LOGI("Displaying: TEXT (Text Pattern)\r\n");
				break;
			case 7:
				pattern = EPD_PATTERN_IMAGE;
				LOGI("Displaying: IMAGE (Image Pattern)\r\n");
				break;
			default:
				pattern = EPD_PATTERN_CLEAR;
				LOGW("Displaying: default CLEAR (All White)\r\n");
				break;
		}

		fill_buffer_mono(pattern, image_buf, desc.width, desc.height);
		lisa_display_write(display_device, 0, 0, &desc, image_buf);

		pattern_count++;

		// 墨水屏刷新较慢，间隔时间长一些
		LOGD("Waiting 1 seconds for next pattern...\r\n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	return 0;
}
