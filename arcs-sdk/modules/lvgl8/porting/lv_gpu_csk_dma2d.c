/**
 * @file lv_gpu_csk_dma2d.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_gpu_csk_dma2d.h"
#include "../src/core/lv_refr.h"
#include "lisa_log.h"
#include "cache.h"
#include "dma.h"
#include "FreeRTOS.h"
#include "semphr.h"
#if CONFIG_LV_USE_RPC
#include "lv_func_proxy.h"
#endif
/*********************
 *      DEFINES
 *********************/
static SemaphoreHandle_t dma_sem = NULL;

static void lv_draw_csk_dma2d_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc);
static void lv_draw_csk_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,  const lv_point_t * pos_p,
                       uint32_t letter);
static int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h);
static int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h);

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void lv_draw_csk_dma2d_init(void)
{
#if CONFIG_LV_USE_RPC
    lv_rpc_init();
#endif

    dma_initialize();

    uint8_t ch = dma_channel_reserve(CONFIG_LVGL_GPU_CSK_CPDMA_DMA2D_CH, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);
    if (ch == DMA_CHANNEL_ANY) {
        LOGE("[FAILED] NO free CP DMA channel for lvgl blend!!");
        return;
    }

	// Create binary semaphore for DMA synchronization
	dma_sem = xSemaphoreCreateBinary();
	if (dma_sem == NULL) {
		LOGE("[%s] Failed to create DMA semaphore", __FUNCTION__);
		return;
	}
}

#if CONFIG_LV_CUSTOM_MONITOR
#include "csk_monitor_lv.h"
volatile uint64_t g_dma2d_fill_total_time;
volatile uint64_t g_dma2d_fill_dma_time;
volatile uint32_t g_dma2d_fill_call_cnt;

volatile uint64_t g_dma2d_copy_total_time;
volatile uint64_t g_dma2d_copy_dma_time;
volatile uint32_t g_dma2d_copy_call_cnt;

static void lv_draw_csk_dma2d_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    uint64_t start_time = get_time_cycle();
    lv_draw_sw_rect(draw_ctx, dsc, coords);
    g_draw_rect_time += calc_time_elapsed_cycle(start_time);
    g_draw_rect_call_cnt++;    
}

static void lv_draw_csk_dma2d_img_decoded(lv_draw_ctx_t * draw_ctx,
                                                  const lv_draw_img_dsc_t * draw_dsc,
                                                  const lv_area_t * coords, const uint8_t * src_buf,
                                                  lv_img_cf_t cf)
{
    uint64_t start_time = get_time_cycle();
    lv_draw_sw_img_decoded(draw_ctx, draw_dsc, coords, src_buf, cf);
    g_draw_image_time += calc_time_elapsed_cycle(start_time);
    g_draw_image_call_cnt++;    
}

#endif

void lv_draw_csk_dma2d_ctx_init(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    lv_draw_sw_init_ctx(drv, draw_ctx);

    lv_draw_csk_dma2d_ctx_t * dma2d_draw_ctx = (lv_draw_sw_ctx_t *)draw_ctx;

#if CONFIG_LV_CUSTOM_MONITOR
    dma2d_draw_ctx->base_draw.draw_rect = lv_draw_csk_dma2d_rect;
    dma2d_draw_ctx->base_draw.draw_img_decoded = lv_draw_csk_dma2d_img_decoded;
#endif

    dma2d_draw_ctx->base_draw.draw_letter = lv_draw_csk_letter;
    dma2d_draw_ctx->blend = lv_draw_csk_dma2d_blend;
}

static LV_ATTRIBUTE_FAST_MEM void lv_draw_csk_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,  const lv_point_t * pos_p,
                       uint32_t letter)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif
#if CONFIG_LV_CSK_SW_LETTER
    extern void lv_draw_sw_csk_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,  const lv_point_t * pos_p,
                        uint32_t letter);
                       
    lv_draw_sw_csk_letter(draw_ctx, dsc, pos_p, letter);
#else
    lv_draw_sw_letter(draw_ctx, dsc, pos_p, letter);
#endif
#if CONFIG_LV_CUSTOM_MONITOR
    g_draw_letter_time += calc_time_elapsed_cycle(start_time);
    g_draw_letter_call_cnt++;
#endif
}

static int _lv_draw_csk_dma2d_blend_fill(const lv_color_t * dest_buf, lv_coord_t dest_stride,
                                                           const lv_area_t * draw_area, lv_color_t color, lv_opa_t opa)
{
    lv_color_t *dest_bufc =  (lv_color_t *)dest_buf;
    dest_bufc += dest_stride * draw_area->y1;
    dest_bufc += draw_area->x1;

    if(((uint32_t)dest_bufc % 4 == 0) && (lv_area_get_width(draw_area) % 2 == 0) && (uint32_t)dest_stride % 2 == 0){
        dcache_flush_range((uint32_t)dest_bufc, ((uint32_t)dest_bufc + dest_stride * lv_area_get_height(draw_area) * sizeof(lv_color_t)));
        return lv_gpu_dma2d_fill(dest_bufc, dest_stride, (uint32_t)(color.full), lv_area_get_width(draw_area), lv_area_get_height(draw_area));
    }

    return -1;
}

static int _lv_draw_csk_dma2d_blend(const lv_color_t * dest_buf, lv_coord_t dest_stride,
                                                          const lv_area_t * draw_area, const void * src_buf, lv_coord_t src_stride, const lv_point_t * src_offset, lv_opa_t opa)
{
    lv_color_t *dest_bufc =  (lv_color_t *)dest_buf;
    lv_color_t *src_bufc =  (lv_color_t *)src_buf;

    dest_bufc += dest_stride * draw_area->y1;
    dest_bufc += draw_area->x1;

    src_bufc += src_stride * src_offset->y;
    src_bufc += src_offset->x;   

    if(((uint32_t)dest_bufc % 4 == 0) && (lv_area_get_width(draw_area) % 2 == 0) && (uint32_t)src_bufc % 4 == 0
        && (uint32_t)dest_stride % 2 == 0 && (uint32_t)src_stride % 2 == 0){
        dcache_flush_range((uint32_t)dest_bufc, ((uint32_t)dest_bufc + dest_stride * lv_area_get_height(draw_area) * sizeof(lv_color_t)));
        dcache_flush_range((uint32_t)src_bufc, ((uint32_t)src_bufc + src_stride * lv_area_get_height(draw_area) * sizeof(lv_color_t)));
        return lv_gpu_dma2d_blend(dest_bufc, dest_stride, src_bufc, opa, src_stride, lv_area_get_width(draw_area), lv_area_get_height(draw_area));
    }

    return -1;
}

static lv_point_t lv_area_get_offset(const lv_area_t * area1, const lv_area_t * area2)
{
    lv_point_t offset = {x: area2->x1 - area1->x1, y: area2->y1 - area1->y1};
    return offset;
}

static LV_ATTRIBUTE_FAST_MEM void lv_draw_csk_dma2d_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    int done = -1;

    lv_area_t draw_area;
    /*Let's get the blend area which is the intersection of the area to draw and the clip area*/
    if(!_lv_area_intersect(&draw_area, dsc->blend_area, draw_ctx->clip_area)) 
        return;

    const lv_opa_t * mask = dsc->mask_buf;

    lv_coord_t dest_stride = lv_area_get_width(draw_ctx->buf_area);

    if(lv_area_get_size(&draw_area) >= 4096){
        if(mask != NULL) {
        }else{
            if(dsc->src_buf == NULL) { 
                lv_area_move(&draw_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1); 
                if (dsc->opa == LV_OPA_COVER) {
                    done = _lv_draw_csk_dma2d_blend_fill(draw_ctx->buf, dest_stride, &draw_area, dsc->color, dsc->opa);
                }
            }else{
                lv_coord_t src_stride = lv_area_get_width(dsc->blend_area);
                lv_point_t src_offset = lv_area_get_offset(dsc->blend_area, &draw_area);
                lv_area_move(&draw_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

                if (dsc->opa == LV_OPA_COVER) {
                    done = _lv_draw_csk_dma2d_blend(draw_ctx->buf, dest_stride, &draw_area, dsc->src_buf, src_stride, &src_offset, dsc->opa);
                }
            }
        }       
    }else{
    }

    if(done != 0){
        lv_draw_sw_blend_basic(draw_ctx, dsc);
    }
}

static int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    int ret = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t dst_gath = (((buf_w - fill_w) / 2 ) << SG_INTERVAL_POS) | ((fill_w / 2) << SG_COUNT_POS);

    //Use sram as much as possible
    static __attribute__((aligned(4), section(".dtcm.bss"))) uint32_t fill_color;

    fill_color = color + color * 65536;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);  

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

#if CONFIG_LV_CUSTOM_MONITOR
uint64_t dma2d_ready = get_time_cycle();
#endif

    uint8_t stat = 0;
    if (buf_w == fill_w) {

        stat = dma_channel_configure (CONFIG_LVGL_GPU_CSK_CPDMA_DMA2D_CH, (uint32_t)&fill_color, (uint32_t)buf, fill_w * fill_h / 2,
                                    control, config_low, config_high, 0, 0);
    } else {
        control |= DMA_CH_CTLL_D_SCAT_EN; 
        stat = dma_channel_configure (CONFIG_LVGL_GPU_CSK_CPDMA_DMA2D_CH, (uint32_t)&fill_color, (uint32_t)buf, fill_w * fill_h / 2,
                                    control, config_low, config_high, 0, dst_gath);
    }

    if(stat == -1) {
        LOGE("[FAILED] dma_channel_configure failed!!\r\n");
        ret = -2;
        goto error;
    }

    if (xSemaphoreTake(dma_sem, pdMS_TO_TICKS(500)) != pdTRUE) {
        ret = -3;
        goto error;
    }


error:
#if CONFIG_LV_CUSTOM_MONITOR
    g_dma2d_fill_total_time += calc_time_elapsed_cycle(start_time);
    g_dma2d_fill_dma_time += calc_time_elapsed_cycle(dma2d_ready);
    g_dma2d_fill_call_cnt++;
#endif
    return ret;
}

static int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int ret = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t src_gath = (((map_w - copy_w) / 2) << SG_INTERVAL_POS) | ((copy_w / 2) << SG_COUNT_POS); 
    uint32_t dst_gath = (((buf_w - copy_w) / 2) << SG_INTERVAL_POS) | ((copy_w / 2) << SG_COUNT_POS); 

#if CONFIG_LV_CUSTOM_MONITOR
uint64_t start_time = get_time_cycle();
#endif

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    control |= DMA_CH_CTLL_S_GATH_EN;    
    control |= DMA_CH_CTLL_D_SCAT_EN;    

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t dma2d_ready = get_time_cycle();
#endif
    uint8_t stat = dma_channel_configure (CONFIG_LVGL_GPU_CSK_CPDMA_DMA2D_CH, (uint32_t)map, (uint32_t)buf, copy_w * copy_h / 2,
                                control, config_low, config_high, src_gath, dst_gath);

    if(stat == -1) {
        ret = -2;
        goto error;
    }

    if (xSemaphoreTake(dma_sem, pdMS_TO_TICKS(500)) != pdTRUE) {
        ret = -3;
        goto error;
    }

error:
#if CONFIG_LV_CUSTOM_MONITOR
    g_dma2d_copy_total_time += calc_time_elapsed_cycle(start_time);
    g_dma2d_copy_dma_time += calc_time_elapsed_cycle(dma2d_ready);
    g_dma2d_copy_call_cnt++;
#endif
    return ret;
}
