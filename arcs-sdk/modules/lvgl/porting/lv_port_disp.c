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
#include "log_print.h"
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
const struct display_device *lv_display_device = NULL;

/* Display buffers */
static lv_disp_buf_t disp_buf;
#if CONFIG_LV_DOUBLE_VDB
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

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
            
            // Signal that flush is complete
            xSemaphoreGive(flush_sem);
        }
    }
}
#endif

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#if CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE > 1
static void disp_rounder(struct _disp_drv_t *disp_drv, lv_area_t *area)
{
	area->y1 &= ~(CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
	area->y2 |= (CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
	area->x1 &= ~(CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
	area->x2 |= (CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
	
	/* Clamp to screen boundaries to prevent overflow */
	lv_coord_t max_x, max_y;
	if(disp_drv->rotated) {
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

#if CONFIG_LV_CUSTOM_MONITOR
#include "csk_monitor_lv.h"
volatile uint64_t g_dma2d_fill_total_time;
volatile uint64_t g_dma2d_fill_dma_time;
volatile uint32_t g_dma2d_fill_call_cnt;

volatile uint64_t g_dma2d_copy_total_time;
volatile uint64_t g_dma2d_copy_dma_time;
volatile uint32_t g_dma2d_copy_call_cnt;

volatile uint64_t g_draw_letter_time;
volatile uint32_t g_draw_letter_call_cnt;

volatile uint64_t g_draw_image_time;
volatile uint32_t g_draw_image_call_cnt;

volatile uint64_t g_draw_rect_time;
volatile uint32_t g_draw_rect_call_cnt;

#ifdef CONFIG_LV_USE_RPC
volatile uint64_t g_rpc_fill_total_time;
volatile uint64_t g_rpc_fill_proxy_time;
volatile uint32_t g_rpc_fill_call_cnt;

volatile uint64_t g_rpc_map_total_time;
volatile uint64_t g_rpc_map_proxy_time;
volatile uint32_t g_rpc_map_call_cnt;
#endif

static void monitor_cb(lv_disp_drv_t * drv, uint32_t time, uint32_t px)
{
	CLOGI("[%s] time = %dms, px = %d", __FUNCTION__, time, px);

	CLOGI("dam2d fill: cnt = %d, time = %dus, dma_time = %dus", g_dma2d_fill_call_cnt, clk_2_us(g_dma2d_fill_total_time), clk_2_us(g_dma2d_fill_dma_time));
	g_dma2d_fill_total_time = 0;
	g_dma2d_fill_dma_time = 0;
	g_dma2d_fill_call_cnt = 0;

	CLOGI("dam2d copy: cnt = %d, time = %dus, dma_time = %dus", g_dma2d_copy_call_cnt, clk_2_us(g_dma2d_copy_total_time), clk_2_us(g_dma2d_copy_dma_time));
	g_dma2d_copy_total_time = 0;
	g_dma2d_copy_dma_time = 0;
	g_dma2d_copy_call_cnt = 0;

	CLOGI("draw letter: cnt = %d, time = %dus", g_draw_letter_call_cnt, clk_2_us(g_draw_letter_time));
	g_draw_letter_time = 0;
	g_draw_letter_call_cnt = 0;

	CLOGI("draw image: cnt = %d, time = %dus", g_draw_image_call_cnt, clk_2_us(g_draw_image_time));
	g_draw_image_time = 0;
	g_draw_image_call_cnt = 0;

	CLOGI("draw rect: cnt = %d, time = %dus", g_draw_rect_call_cnt, clk_2_us(g_draw_rect_time));
	g_draw_rect_time = 0;
	g_draw_rect_call_cnt = 0;
	
#ifdef CONFIG_LV_USE_RPC
	CLOGI("rpc fill: cnt = %d, time = %dus, proxy_time = %dus", g_rpc_fill_call_cnt, clk_2_us(g_rpc_fill_total_time), clk_2_us(g_rpc_fill_proxy_time));
	g_rpc_fill_total_time = 0;
	g_rpc_fill_proxy_time = 0;
	g_rpc_fill_call_cnt = 0;

	CLOGI("rpc map: cnt = %d, time = %dus, proxy_time = %dus", g_rpc_map_call_cnt, clk_2_us(g_rpc_map_total_time), clk_2_us(g_rpc_map_proxy_time));
	g_rpc_map_total_time = 0;
	g_rpc_map_proxy_time = 0;
	g_rpc_map_call_cnt = 0;
#endif

}
#endif

void lv_port_disp_init(display_hw_config_t *config)
{
	struct display_capabilities caps = {0};

	/*-------------------------
	 * Initialize your display
	 * -----------------------*/
	lv_display_device = lisa_display_create(config);
	if (lv_display_device == NULL) {
		LV_LOG_ERROR("[%s] display device create faild", __FUNCTION__);
		return;
	}

	lisa_display_get_capabilities(lv_display_device, &caps);

	lisa_display_blanking_off(lv_display_device);
	lisa_display_set_brightness(lv_display_device, 50);

	/*-----------------------------
	 * Create a buffer for drawing
	 *----------------------------*/
	uint32_t size = caps.x_resolution * caps.y_resolution * sizeof(lv_color_t);
	lv_color_t *buf_1 = lv_mem_alloc(size);
	if(buf_1 == NULL) {
		LV_LOG_ERROR("[%s] buf_1 malloc faild", __FUNCTION__);
		return;
	}
#if CONFIG_LV_DOUBLE_VDB
	lv_color_t *buf_2 = lv_mem_alloc(size);
	if(buf_2 == NULL) {
		LV_LOG_ERROR("[%s] buf_2 malloc faild", __FUNCTION__);
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
	
	lv_disp_buf_init(&disp_buf, buf_1, buf_2, size / sizeof(lv_color_t));
#else
	lv_disp_buf_init(&disp_buf, buf_1, NULL, size / sizeof(lv_color_t));
#endif

	/*-----------------------------------
	 * Register the display in LVGL
	 *----------------------------------*/
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);

	/*Set up the functions to access to your display*/
	disp_drv.flush_cb = disp_flush;

#if CONFIG_LV_DRIVER_FULL_REFRESH
	disp_drv.full_refresh = 1;
#endif

#if CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE > 1
	disp_drv.rounder_cb = disp_rounder;
#endif

#if CONFIG_LV_DRIVER_ROTATE_NORMAL
	disp_drv.rotated = 0;
	lisa_display_set_orientation(lv_display_device, DISPLAY_ORIENTATION_NORMAL);
#elif CONFIG_LV_DRIVER_ROTATE_90
	disp_drv.rotated = 1;
	lisa_display_set_orientation(lv_display_device, DISPLAY_ORIENTATION_ROTATED_90);
#elif CONFIG_LV_DRIVER_ROTATE_270
	disp_drv.rotated = 1;
	lisa_display_set_orientation(lv_display_device, DISPLAY_ORIENTATION_ROTATED_270);
#endif

#if CONFIG_LV_CUSTOM_MONITOR
	disp_drv.monitor_cb = monitor_cb;
#endif
	
	/*Set the resolution of the display*/
	disp_drv.hor_res = caps.x_resolution;
	disp_drv.ver_res = caps.y_resolution;

	/*Used to copy the buffer's content to the display*/
	disp_drv.buffer = &disp_buf;

	/*Finally register the driver*/
	lv_disp_drv_register(&disp_drv);
}

/* Flush the content of the internal buffer the specific area on the display */
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

	lv_disp_flush_ready(disp_drv);
#else
    struct display_buffer_descriptor desc = {0};
    /* Update descriptor for the current area */
    desc.width = area->x2 - area->x1 + 1;
    desc.height = area->y2 - area->y1 + 1;
    desc.pitch = desc.width * sizeof(lv_color_t);
    desc.buf_size = desc.height * desc.width * sizeof(lv_color_t);

    /* Write data to display */
    lisa_display_write(lv_display_device, area->x1, area->y1, &desc, color_p);

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
#endif
}

#endif
