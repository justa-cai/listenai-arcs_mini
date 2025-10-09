#include <string.h>
#include "lisa_display.h"
// #include "lisa_log.h"
#include "log_print.h"
#include "cache.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "display_common.h"
#include "display_trans_ctx.h"
#if 0
#include "dma_irq_proxy.h"
#endif

static struct display_obj g_display_obj;	

static int _axs15231b_trans_cmd_prepare(int lcd_cmd)
{
    lcd_cmd &= 0xFF;
    lcd_cmd <<= 8;
    lcd_cmd |= (DISPLAY_COMN_OPCODE_WRITE_CMD << 24); 

    return lcd_cmd;
}

static int _axs15231b_trans_img_prepare(int lcd_cmd)
{
    lcd_cmd &= 0xFF;
    lcd_cmd <<= 8;
    lcd_cmd |= (DISPLAY_COMM_OPCODE_WRITE_IMG << 24); 

    return lcd_cmd;
}

static void lcd_axs15231b_init(void)
{
	// reset
	disp_comm_rst_set();
	delay_ms(10);
	disp_comm_rst_clr();
	delay_ms(10);
	disp_comm_rst_set();

	delay_ms(10);

	/**
	 * 屏幕模组内置FLASH上电已自动完成LCD初始化
	 */

	// SLEEP OUT
	display_trans_cmd_data(_axs15231b_trans_cmd_prepare(DISPLAY_COMM_CMD_SLEEP_OUT), NULL, 0);
	delay_ms(10);
}

int axs15231b_display_init(display_hw_config_t *config)
{
	if(g_display_obj.initialized){
		return 0;
	}

	g_display_obj.mutex = xSemaphoreCreateMutex();
	if (g_display_obj.mutex == NULL) {
		CLOGE("[%s] Failed to create g_mutex", __FUNCTION__);
		return -1;
	}

	// init rst
	disp_comm_rst_init(&config->reset);

	// init pwm
	disp_comm_brightness_init(&config->blacklight);

#if CONFIG_LISA_DISPLAY_TE_SYNC
	g_display_obj.te_sem = xSemaphoreCreateBinary();
	if (g_display_obj.te_sem == NULL) {
		CLOGE("[%s] Failed to create TE semaphore", __FUNCTION__);
		return -1;
	}

	disp_comm_te_init(g_display_obj.te_sem, &config->te);
#endif

	display_trans_ctx_init(16, 32, 0, &config->trans_config);

	lcd_axs15231b_init();

	g_display_obj.orientation = DISPLAY_ORIENTATION_NORMAL;
	g_display_obj.initialized = true;

	return 0;
}

int axs15231b_display_blanking_off(void)
{
	xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
	display_trans_cmd_data(_axs15231b_trans_cmd_prepare(DISPLAY_COMM_CMD_DISP_ON), NULL, 0);
	xSemaphoreGive(g_display_obj.mutex);
	return 0;
}

int axs15231b_display_blanking_on(void)
{
	xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
	display_trans_cmd_data(_axs15231b_trans_cmd_prepare(DISPLAY_COMM_CMD_DISP_OFF), NULL, 0);
	xSemaphoreGive(g_display_obj.mutex);
	return 0;
}

void axs15231b_display_get_capabilities(struct display_capabilities *capabilities)
{
	xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
	memset(capabilities, 0, sizeof(*capabilities));

	capabilities->x_resolution = CONFIG_DISPLAY_AXS15231B_WIDTH;
	capabilities->y_resolution = CONFIG_DISPLAY_AXS15231B_HEIGHT;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
	capabilities->current_orientation = g_display_obj.orientation;
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	xSemaphoreGive(g_display_obj.mutex);
}

int axs15231b_display_set_brightness(const uint8_t brightness)
{
	disp_comm_brightness_set(brightness);

	return 0;
}

int axs15231b_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
			    const void *buf)
{
	int ret = 0;
	xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);

	// LOGI("[%s:%d] buf=%p x=%d, y=%d, w=%d, h=%d, image_size=%d", __func__, __LINE__, buf, x, y, desc->width, desc->height, image_size);

#if CONFIG_LISA_DISPLAY_TE_SYNC
	if (disp_comm_te_wait(g_display_obj.te_sem, 100) != 0) {
		CLOGE("[%s] Failed to take TE semaphore", __FUNCTION__);
		xSemaphoreGive(g_display_obj.mutex);
		return -1;
	}
#endif

#if CONFIG_DCACHE_ENABLE
	dcache_clean_range((uint32_t)buf, (uint32_t)buf + (uint32_t)desc->buf_size);
#endif

#ifndef CONFIG_DISPLAY_AXS51231B_X_OFFSET
	#define CONFIG_DISPLAY_AXS51231B_X_OFFSET 0
#endif

#ifndef CONFIG_DISPLAY_AXS51231B_Y_OFFSET
	#define CONFIG_DISPLAY_AXS51231B_Y_OFFSET 0
#endif

	struct disp_mem_area_input area_input = {
		.panel_w = CONFIG_DISPLAY_AXS15231B_WIDTH,
		.panel_h = CONFIG_DISPLAY_AXS15231B_HEIGHT,
		.x_offset = CONFIG_DISPLAY_AXS51231B_X_OFFSET,
		.y_offset = CONFIG_DISPLAY_AXS51231B_Y_OFFSET,
		.x = x,
		.y = y,
		.w = desc->width,
		.h = desc->height,
		.orient = g_display_obj.orientation,
	};
	disp_mem_coord coord_x;
	disp_mem_coord coord_y;
	disp_comm_set_mem_area(&area_input, coord_x, coord_y);

	display_trans_cmd_data(_axs15231b_trans_cmd_prepare(DISPLAY_COMM_CMD_CASET), coord_x, sizeof(coord_x));
	display_trans_cmd_data(_axs15231b_trans_cmd_prepare(DISPLAY_COMM_CMD_RASET), coord_y,sizeof(coord_y));

	enum display_trans_orient rotated = DISPLAY_TRANS_ORIENT_NORMAL;
	if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_90) {
		rotated = DISPLAY_TRANS_ORIENT_ROTATED_90;
	} else if (g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_270) {
		rotated = DISPLAY_TRANS_ORIENT_ROTATED_270;
	}

	ret = display_trans_image(_axs15231b_trans_img_prepare(DISPLAY_COMM_CMD_RAMWR), (void *)buf, desc->width, desc->height, rotated);
	
	xSemaphoreGive(g_display_obj.mutex);

	return ret;
}

int axs15231b_display_set_orientation(const enum display_orientation orientation)
{
	xSemaphoreTake(g_display_obj.mutex, portMAX_DELAY);
	g_display_obj.orientation = orientation;
	xSemaphoreGive(g_display_obj.mutex);
	return 0;
}

int axs15231b_display_sleep(const uint8_t onoff)
{
	if(onoff){
		display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_IN, NULL, 0);
	}else{
		display_trans_cmd_data(DISPLAY_COMM_CMD_SLEEP_OUT, NULL, 0);
	}
	return 0;
}

static const struct display_driver_api axs15231b_driver_api = {
	.display_blanking_on = axs15231b_display_blanking_on,
	.display_blanking_off = axs15231b_display_blanking_off,
	.display_get_capabilities = axs15231b_display_get_capabilities,
	.display_set_brightness = axs15231b_display_set_brightness,
	.display_write = axs15231b_display_write,
	.display_set_orientation = axs15231b_display_set_orientation,
	.display_sleep = axs15231b_display_sleep,
};

const struct display_device display_axs15231b = {
	.name = "axs15231b",
	.device_init = axs15231b_display_init,
	.api = &axs15231b_driver_api,
};