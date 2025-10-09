/**
 * @file lv_gpu_csk_dma2d.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_gpu_csk_dma2d.h"
#include "lv_core/lv_refr.h"

#if LV_USE_GPU_CSK_DMA2D
#include "csk_dma2d_wrapper.h"
#include "cache.h"
#include "dma.h"
#include "log_print.h"
#include "FreeRTOS.h"
#include "semphr.h"
#if CONFIG_LV_CUSTOM_MONITOR
#include "csk_monitor_lv.h"
#endif
#if CONFIG_LV_USE_RPC
#include "lv_func_proxy.h"
#endif
/*********************
 *      DEFINES
 *********************/

#define GPU_DMA_CH CONFIG_LV_CSK_DMA2D_CH


#define IF_USE_NEW_API  0

#define CONFIG_USE_IRQ_PROXY  0
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void dma2d_wait(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static uint8_t real_ch = DMA_CHANNEL_ANY;
static SemaphoreHandle_t dma_sem = NULL;
/**********************
 *      MACROS
 **********************/


/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#if CONFIG_USE_IRQ_PROXY
static int32_t dma_proxy_interruptHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#else
static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif

void lv_gpu_csk_dma2d_init(void)
{
#if IF_USE_NEW_API
    rgb565_dma2d_init();
#else
    dma_initialize();

#if CONFIG_USE_IRQ_PROXY
    #include "dma_irq_proxy.h"
    real_ch = dma_channel_reserve(CONFIG_LVGL_GPU_CSK_CPDMA_DMA2D_CH, NULL, 0, DMA_CACHE_SYNC_NOP);
    if (real_ch == DMA_CHANNEL_ANY) {
        CLOGE("[FAILED] NO free CP DMA channel for lvgl blend!!");
        return;
    }
    dma_irq_proxy_register_callback(IRQ_PROXY_CHAN_LVGL_BLEND_DMA, dma_proxy_interruptHandler);
#else
    real_ch = dma_channel_reserve(CONFIG_LVGL_GPU_CSK_CPDMA_DMA2D_CH, DMA_DrvEvent, 0, DMA_CACHE_SYNC_NOP);
    if (real_ch == DMA_CHANNEL_ANY) {
        CLOGE("[FAILED] NO free CP DMA channel for lvgl blend!!");
        return;
    }
#endif

	dma_sem = xSemaphoreCreateBinary();
	if (dma_sem == NULL) {
		CLOGE("[%s] Failed to create DMA semaphore", __FUNCTION__);
		return;
	}

#if CONFIG_LV_USE_RPC
    lv_rpc_init();
#endif

#endif
}

/**
 * Fill an area in the buffer with a color
 * @param buf a buffer which should be filled
 * @param buf_w width of the buffer in pixels
 * @param color fill color
 * @param fill_w width to fill in pixels (<= buf_w)
 * @param fill_h height to fill in pixels
 * 
 * @return 0 on success, -1 on failure
 * 
 * @note `buf_w - fill_w` is offset to the next line after fill
 */
int lv_gpu_csk_dma2d_fill(lv_color_t * buf, lv_coord_t buf_w, lv_color_t color, lv_coord_t fill_w, lv_coord_t fill_h)
{

    #if IF_USE_NEW_API
    lv_gpu_dma2d_fill(buf, buf_w, color.full, fill_w, fill_h);
    #else

    if(((uint32_t)buf % 4 != 0) || ((uint32_t)fill_w % 2 != 0)) {
        return -1;
    }

    #if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
    #endif
    
    //Use sram as much as possible
    static __attribute__((aligned(4), section(".dtcm.bss"))) uint32_t fill_color;

    fill_color = color.full + color.full * 65536;
    uint32_t control, config_low, config_high;
    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t dst_gath = (((buf_w - fill_w) / 2 ) << SG_INTERVAL_POS) | ((fill_w / 2) << SG_COUNT_POS); 

    //CLOGD("[%s] fill_w = %d, fill_h = %d, buf(0x%x)_w = %d color = 0x%x\r\n", __func__, fill_w, fill_h, (uint32_t)buf, buf_w, color);

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);  

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

    dcache_flush_range((uint32_t)buf, (((uint32_t)buf + buf_w * fill_h * 2)));

    uint8_t stat = 0;
    if (buf_w == fill_w) {

        stat = dma_channel_configure (real_ch, (uint32_t)&fill_color, (uint32_t)buf, fill_w * fill_h / 2,
                                    control, config_low, config_high, 0, 0);
    } else {
        control |= DMA_CH_CTLL_D_SCAT_EN; 
        stat = dma_channel_configure (real_ch, (uint32_t)&fill_color, (uint32_t)buf, fill_w * fill_h / 2,
                                    control, config_low, config_high, 0, dst_gath);
    }

    #if CONFIG_LV_CUSTOM_MONITOR
    uint64_t dma2d_ready = get_time_cycle();
    #endif

    dma2d_wait();

    #if CONFIG_LV_CUSTOM_MONITOR
    g_dma2d_fill_total_time += calc_time_elapsed_cycle(start_time);
    g_dma2d_fill_dma_time += calc_time_elapsed_cycle(dma2d_ready);
    g_dma2d_fill_call_cnt++;
    #endif

    #endif

    return 0;
}

/**
 * Fill an area in the buffer with a color but take into account a mask which describes the opacity of each pixel
 * @param buf a buffer which should be filled using a mask
 * @param buf_w width of the buffer in pixels
 * @param color fill color
 * @param mask 0..255 values describing the opacity of the corresponding pixel. It's width is `fill_w`
 * @param opa overall opacity. 255 in `mask` should mean this opacity.
 * @param fill_w width to fill in pixels (<= buf_w)
 * @param fill_h height to fill in pixels
 * @note `buf_w - fill_w` is offset to the next line after fill
 */
void lv_gpu_csk_dma2d_fill_mask(lv_color_t * buf, lv_coord_t buf_w, lv_color_t color, const lv_opa_t * mask,
                                  lv_opa_t opa, lv_coord_t fill_w, lv_coord_t fill_h)
{
    lv_gpu_dma2d_fill_mask(buf, buf_w, color.full, (void *)mask, opa, fill_w, fill_h);
}

/**
 * Copy a map (typically RGB image) to a buffer
 * @param buf a buffer where map should be copied
 * @param buf_w width of the buffer in pixels
 * @param map an "image" to copy
 * @param map_w width of the map in pixels
 * @param copy_w width of the area to copy in pixels (<= buf_w)
 * @param copy_h height of the area to copy in pixels
 * 
 * @return 0 on success, -1 on failure
 * 
 * @note `map_w - fill_w` is offset to the next line after copy
 */
int lv_gpu_csk_dma2d_copy(lv_color_t * buf, lv_coord_t buf_w, const lv_color_t * map, lv_coord_t map_w,
                             lv_coord_t copy_w, lv_coord_t copy_h)
{
#if IF_USE_NEW_API
    lv_gpu_dma2d_copy(buf, buf_w, map, map_w, copy_w, copy_h);
#else

    if(((uint32_t)buf % 4 != 0) || ((uint32_t)copy_w % 2 != 0)) {
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    uint32_t control, config_low, config_high;
    uint32_t src_width = DMA_WIDTH_WORD;
    uint32_t src_bsize = DMA_BSIZE_16;
    uint32_t dst_width = DMA_WIDTH_WORD;
    uint32_t dst_bsize = DMA_BSIZE_16;
    uint32_t src_gath = (((map_w - copy_w) / 2) << SG_INTERVAL_POS) | ((copy_w / 2) << SG_COUNT_POS); 
    uint32_t dst_gath = (((buf_w - copy_w) / 2) << SG_INTERVAL_POS) | ((copy_w / 2) << SG_COUNT_POS); 

    //CLOGD("[%s] copy_w = %d, copy_h = %d, map(0x%x)_w = %d, buf(0x%x)_w = %d\r\n", __func__, copy_w, copy_h, (uint32_t)map, (uint32_t)map_w, buf_w);

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
            DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
            DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    control |= DMA_CH_CTLL_S_GATH_EN;    
    control |= DMA_CH_CTLL_D_SCAT_EN;    

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

    dcache_flush_range((uint32_t)map, (((uint32_t)map + map_w * copy_h * 2)));
    dcache_flush_range((uint32_t)buf, (((uint32_t)buf + buf_w * copy_h * 2)));

    uint8_t stat = dma_channel_configure (real_ch, (uint32_t)map, (uint32_t)buf, copy_w * copy_h / 2,
                                control, config_low, config_high, src_gath, dst_gath);

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t dma2d_ready = get_time_cycle();
#endif

    dma2d_wait();

#if CONFIG_LV_CUSTOM_MONITOR
    g_dma2d_copy_total_time += calc_time_elapsed_cycle(start_time);
    g_dma2d_copy_dma_time += calc_time_elapsed_cycle(dma2d_ready);
    g_dma2d_copy_call_cnt++;
#endif

#endif

    return 0;
}

/**
 * Blend a map (e.g. ARGB image or RGB image with opacity) to a buffer
 * @param buf a buffer where `map` should be copied
 * @param buf_w width of the buffer in pixels
 * @param map an "image" to copy
 * @param opa opacity of `map`
 * @param map_w width of the map in pixels
 * @param copy_w width of the area to copy in pixels (<= buf_w)
 * @param copy_h height of the area to copy in pixels
 * @note `map_w - fill_w` is offset to the next line after copy
 */
void lv_gpu_csk_dma2d_blend(lv_color_t * buf, lv_coord_t buf_w, const lv_color_t * map, lv_opa_t opa,
                              lv_coord_t map_w, lv_coord_t copy_w, lv_coord_t copy_h)
{
    lv_gpu_dma2d_blend(buf, buf_w, (void *)map, opa, map_w, copy_w, copy_h);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void invalidate_cache(void)
{

}

static void dma2d_wait(void)
{
    if (xSemaphoreTake(dma_sem, pdMS_TO_TICKS(20)) != pdTRUE) {
        CLOGE("dma2d wait timeout");
    }
}

#endif
