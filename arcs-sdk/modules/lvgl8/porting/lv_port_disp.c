/**
 * @file lv_port_disp.c
 *
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "lisa_display.h"
#include <stdlib.h>
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static struct display_device *lv_display_device = NULL;

/* Display buffers */
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf_1;

#if CONFIG_LV_DOUBLE_VDB
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

static lv_color_t *buf_2;
static QueueHandle_t flush_queue;
static TaskHandle_t flush_task_handle;
static SemaphoreHandle_t flush_sem;  // Add semaphore handle

typedef struct {
    lv_area_t area;
    lv_color_t *color_p;
	lv_disp_drv_t *disp_drv;
} flush_msg_t;

static void flush_thread(void *arg)
{
    flush_msg_t msg;
    struct display_buffer_descriptor desc;

    while (1) {
        if (xQueueReceive(flush_queue, &msg, portMAX_DELAY) == pdTRUE) {
            desc.width = msg.area.x2 - msg.area.x1 + 1;
            desc.height = msg.area.y2 - msg.area.y1 + 1;
            desc.pitch = desc.width * sizeof(lv_color_t);
            desc.buf_size = desc.height * desc.width * sizeof(lv_color_t);

            lisa_display_write(lv_display_device, msg.area.x1, msg.area.y1, &desc, msg.color_p);

            xSemaphoreGive(flush_sem);

        }
    }
}
#endif

#if CONFIG_LV_CUSTOM_MONITOR
#include "lisa_log.h"
#include "csk_monitor_lv.h"

volatile uint64_t g_draw_letter_time;
volatile uint32_t g_draw_letter_call_cnt;

volatile uint64_t g_draw_image_time;
volatile uint32_t g_draw_image_call_cnt;

volatile uint64_t g_draw_rect_time;
volatile uint32_t g_draw_rect_call_cnt;

static void monitor_cb(lv_disp_drv_t * drv, uint32_t time, uint32_t px)
{
    LOGI("[%s] time:%dms px:%d", __func__, time, px);

    LOGI("draw letter: cnt = %d, time = %dus", g_draw_letter_call_cnt, clk_2_us(g_draw_letter_time));
	g_draw_letter_time = 0;
	g_draw_letter_call_cnt = 0;

	LOGI("draw image: cnt = %d, time = %dus", g_draw_image_call_cnt, clk_2_us(g_draw_image_time));
	g_draw_image_time = 0;
	g_draw_image_call_cnt = 0;

	LOGI("draw rect: cnt = %d, time = %dus", g_draw_rect_call_cnt, clk_2_us(g_draw_rect_time));
	g_draw_rect_time = 0;
	g_draw_rect_call_cnt = 0;

#ifdef CONFIG_LV_USE_GPU_CSK_DMA2D
    LOGI("dam2d fill: cnt = %d, time = %dus, dma_time = %dus", g_dma2d_fill_call_cnt, clk_2_us(g_dma2d_fill_total_time), clk_2_us(g_dma2d_fill_dma_time));
    g_dma2d_fill_total_time = 0;
    g_dma2d_fill_dma_time = 0;
    g_dma2d_fill_call_cnt = 0;

    LOGI("dam2d copy: cnt = %d, time = %dus, dma_time = %dus", g_dma2d_copy_call_cnt, clk_2_us(g_dma2d_copy_total_time), clk_2_us(g_dma2d_copy_dma_time));
    g_dma2d_copy_total_time = 0;
    g_dma2d_copy_dma_time = 0;
    g_dma2d_copy_call_cnt = 0;
#endif

#ifdef CONFIG_LV_USE_RPC
	LOGI("rpc fill normal  : cnt = %d, time = %dus, proxy_time = %dus", g_rpc_fill_normal_call_cnt, clk_2_us(g_rpc_fill_normal_total_time), clk_2_us(g_rpc_fill_normal_proxy_time));
	g_rpc_fill_normal_total_time = 0;
	g_rpc_fill_normal_proxy_time = 0;
	g_rpc_fill_normal_call_cnt = 0;

	LOGI("rpc map normal: cnt = %d, time = %dus, proxy_time = %dus", g_rpc_map_normal_call_cnt, clk_2_us(g_rpc_map_normal_total_time), clk_2_us(g_rpc_map_normal_proxy_time));
	g_rpc_map_normal_total_time = 0;
	g_rpc_map_normal_proxy_time = 0;
	g_rpc_map_normal_call_cnt = 0;

	LOGI("rpc fill blended: cnt = %d, time = %dus, proxy_time = %dus", g_rpc_fill_blended_call_cnt, clk_2_us(g_rpc_fill_blended_total_time), clk_2_us(g_rpc_fill_blended_proxy_time));
	g_rpc_fill_blended_total_time = 0;
	g_rpc_fill_blended_proxy_time = 0;
	g_rpc_fill_blended_call_cnt = 0;

	LOGI("rpc map blended: cnt = %d, time = %dus, proxy_time = %dus", g_rpc_map_blended_call_cnt, clk_2_us(g_rpc_map_blended_total_time), clk_2_us(g_rpc_map_blended_proxy_time));
	g_rpc_map_blended_total_time = 0;
	g_rpc_map_blended_proxy_time = 0;
	g_rpc_map_blended_call_cnt = 0;
#endif
}
#endif

#if CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE > 1
static void disp_rounder(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    area->y1 &= ~(CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
    area->y2 |= (CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
    area->x1 &= ~(CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
    area->x2 |= (CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);

	/* Clamp to screen boundaries to prevent overflow */
	lv_coord_t max_x, max_y;
	if(disp_drv->rotated == LV_DISP_ROT_90 || disp_drv->rotated == LV_DISP_ROT_270) {
		max_x = disp_drv->ver_res - 1;
		max_y = disp_drv->hor_res - 1;
	} else {
		max_x = disp_drv->hor_res - 1;
		max_y = disp_drv->ver_res - 1;
	}

	if(area->x2 > max_x) area->x2 = max_x;
	if(area->y2 > max_y) area->y2 = max_y;
}
#endif
struct display_device *lv_port_get_display_device(void)
{
    return lv_display_device;
}

void lv_port_disp_init(display_hw_config_t *config)
{
    struct display_capabilities caps = {0};

    lvgl_port_mem_init();
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    lv_display_device = lisa_display_create(config);
    if (lv_display_device == NULL) {
        LV_LOG_ERROR("[%s] display device create faild", __FUNCTION__);
        return;
    }

    lisa_display_get_capabilities(lv_display_device, &caps);

    lisa_display_blanking_on(lv_display_device);

    uint32_t size = caps.x_resolution * caps.y_resolution * sizeof(lv_color_t);
    buf_1 = (lv_color_t *)lv_mem_alloc(size);
    if (buf_1 == NULL) {
        LV_LOG_ERROR("[%s] no mem !!! (%d)", __FUNCTION__, size);
        return;
    }

#if CONFIG_LV_DOUBLE_VDB
	buf_2 = lv_mem_alloc(size);
	if(buf_2 == NULL) {
		LV_LOG_ERROR("[%s] lv_mem malloc faild", __FUNCTION__);
		lv_mem_free(buf_1);
		return;
	}

	/* Create message queue for flush commands */
	flush_queue = xQueueCreate(2, sizeof(flush_msg_t));
	if (flush_queue == NULL) {
		LV_LOG_ERROR("[%s] Failed to create flush queue", __FUNCTION__);
		lv_mem_free(buf_1);
		lv_mem_free(buf_2);
		return;
	}

	/* Create semaphore for flush synchronization */
	flush_sem = xSemaphoreCreateBinary();
	if (flush_sem == NULL) {
		LV_LOG_ERROR("[%s] Failed to create flush semaphore", __FUNCTION__);
		vQueueDelete(flush_queue);
		lv_mem_free(buf_1);
		lv_mem_free(buf_2);
		return;
	}
	/* Initialize semaphore as available */
	xSemaphoreGive(flush_sem);

	/* Create flush thread */
	BaseType_t ret = xTaskCreate(flush_thread, "lvgl_flush", 2048, NULL,
                                configMAX_PRIORITIES - 1, &flush_task_handle);
	if (ret != pdPASS) {
		LV_LOG_ERROR("[%s] Failed to create flush thread", __FUNCTION__);
		vQueueDelete(flush_queue);
		vSemaphoreDelete(flush_sem);
		lv_mem_free(buf_1);
		lv_mem_free(buf_2);
		return;
	}

	lv_disp_draw_buf_init(&disp_buf, buf_1, buf_2, size / sizeof(lv_color_t));
#else
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, size / sizeof(lv_color_t));
#endif

#if 1
    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /*Set up the functions to access to your display*/
    disp_drv.flush_cb = disp_flush;

#if CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE > 1
    disp_drv.rounder_cb = disp_rounder;
#endif

#if CONFIG_LV_DRIVER_ROTATE_NORMAL
    disp_drv.rotated = LV_DISP_ROT_NONE;
    lisa_display_set_orientation(lv_display_device, DISPLAY_ORIENTATION_NORMAL);
#elif CONFIG_LV_DRIVER_ROTATE_90
    disp_drv.rotated = LV_DISP_ROT_90;
    lisa_display_set_orientation(lv_display_device, DISPLAY_ORIENTATION_ROTATED_90);
#elif CONFIG_LV_DRIVER_ROTATE_270
    disp_drv.rotated = LV_DISP_ROT_270;
    lisa_display_set_orientation(lv_display_device, DISPLAY_ORIENTATION_ROTATED_270);
#endif

#if CONFIG_LV_DRIVER_FULL_REFRESH
    disp_drv.full_refresh = 1;
#endif

#if CONFIG_LV_CUSTOM_MONITOR
    disp_drv.monitor_cb = monitor_cb;
#endif

    /*Set the resolution of the display*/
    disp_drv.hor_res = caps.x_resolution;
    disp_drv.ver_res = caps.y_resolution;

    /*Used to copy the buffer's content to the display*/
    disp_drv.draw_buf = &disp_buf;
#if CONFIG_LV_COLOR_DEPTH_1
    uint8_t *fb_buffer_mono = lv_mem_alloc(caps.x_resolution * caps.y_resolution / 8);
    if (fb_buffer_mono == NULL) {
        LV_LOG_ERROR("[%s] lv_mem malloc faild", __FUNCTION__);
    } else {
        disp_drv.user_data = fb_buffer_mono;
    }
#endif

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);

    lisa_display_set_brightness(lv_display_device, 50);
#endif
}

#if CONFIG_LV_COLOR_DEPTH_1

int framebuffer_convert_to_mono(const void *src, void *dst, uint32_t width, uint32_t height)
{
    const uint8_t *src_buf = (const uint8_t *)src;
    uint8_t *dst_buf = (uint8_t *)dst;

    if (src == NULL || dst == NULL || width == 0 || height == 0) {
        return -1;
    }

    uint32_t src_pitch = width * sizeof(lv_color_t);
    uint32_t dst_pitch = (width + 7) / 8; // 1bpp: 8个像素占1字节，向上取整

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t src_idx = y * src_pitch + x * sizeof(lv_color_t);
            uint32_t dst_idx = y * dst_pitch + x / 8;
            uint8_t bit_pos = 7 - (x % 8);

            lv_color_t color = *((lv_color_t *)(src_buf + src_idx));
            if (lv_color_to1(color) == 0) {
                dst_buf[dst_idx] &= ~(1 << bit_pos);
            } else {
                dst_buf[dst_idx] |= (1 << bit_pos);
            }
        }
    }

    return 0;
}

#endif

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
#if CONFIG_LV_DOUBLE_VDB
    flush_msg_t msg;
	msg.area.x1 = area->x1;
	msg.area.y1 = area->y1;
	msg.area.x2 = area->x2;
	msg.area.y2 = area->y2;
	msg.disp_drv = disp_drv;
    msg.color_p = color_p;

	if (xSemaphoreTake(flush_sem, portMAX_DELAY) != pdTRUE) {
        LV_LOG_ERROR("[%s] Failed to take flush semaphore", __FUNCTION__);
    }

    if (xQueueSend(flush_queue, &msg, portMAX_DELAY) != pdPASS) {
        LV_LOG_ERROR("[%s] Failed to send flush message", __FUNCTION__);
    }

    lv_disp_flush_ready(msg.disp_drv);
#else
    struct display_buffer_descriptor desc = {0};
    /* Update descriptor for the current area */
    desc.width = area->x2 - area->x1 + 1;
    desc.height = area->y2 - area->y1 + 1;
    desc.pitch = desc.width * sizeof(lv_color_t);
    desc.buf_size = desc.height * desc.width * sizeof(lv_color_t);

#if CONFIG_LV_COLOR_DEPTH_1
    uint8_t *fb_buffer_mono = (uint8_t *)disp_drv->user_data;
    if (fb_buffer_mono == NULL) {
        LV_LOG_ERROR("[%s] fb_buffer_mono is NULL", __FUNCTION__);
        lv_disp_flush_ready(disp_drv);
        return;
    }
    if (framebuffer_convert_to_mono(color_p, fb_buffer_mono, desc.width, desc.height) != 0) {
        LV_LOG_ERROR("[%s] framebuffer_convert_to_mono faild", __FUNCTION__);
        lv_disp_flush_ready(disp_drv);
        return;
    }
    color_p = (lv_color_t *)fb_buffer_mono;
#endif
    /* Write data to display */
    lisa_display_write(lv_display_device, area->x1, area->y1, &desc, color_p);

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
#endif
}
#endif
