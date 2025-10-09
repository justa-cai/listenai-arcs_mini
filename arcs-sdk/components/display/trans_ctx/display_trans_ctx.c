#include <stdint.h>
#include <string.h>

#include "Driver_GPIO.h"
#include "dma.h"
#include "lisa_log.h"
#include "log_print.h"
#include "display_trans_ctx.h"

#ifndef CONFIG_ROTATE_BUF_MAX_HEIGHT
#error "CONFIG_ROTATE_BUF_MAX_HEIGHT is not defined"
#else
#define ROTATE_BUF_MAX_HEIGHT CONFIG_ROTATE_BUF_MAX_HEIGHT
#endif

// Buffer width for image rotation
#ifndef CONFIG_ROATE_BUF_WIDTH
#define ROTATE_BUF_WIDTH      16
#else
#define ROTATE_BUF_WIDTH      CONFIG_ROTATE_BUF_WIDTH
#endif

struct display_trans_ctx {
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    SemaphoreHandle_t cpdma_done_sem;
#endif

    uint16_t *data;
    uint32_t data_offset;       // per pixel
    uint32_t data_total;        // per pixel
    uint16_t h;                 // per pixel
    uint16_t w;                 // per pixel
    uint32_t bits_per_pixel;
    int      lcd_cmd_bits;
    uint8_t  dc_cmd_level;
    enum display_trans_orient orientation;
    // transaction_cb_t pre_cb;    //Callback to be called before transmission
    // transaction_cb_t post_cb;    //Callback to be called after transmission
    uint16_t rotate_buf_ping[ROTATE_BUF_MAX_HEIGHT * ROTATE_BUF_WIDTH];
    uint16_t rotate_buf_pong[ROTATE_BUF_MAX_HEIGHT * ROTATE_BUF_WIDTH];

    struct display_trans_ops *ops;
};

static struct display_trans_ctx g_disp_trans_context;
static uint8_t rotate_cpdma_ch = DMA_CHANNEL_ANY;

#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
#include "sysutils.h"

__dtcm_bss__ DMA_LLI dma_llp_lists[ROTATE_BUF_MAX_HEIGHT];

static void rotate_dma_drv_event(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    struct display_trans_ctx *ctx = (struct display_trans_ctx *)usr_param;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->cpdma_done_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif

__attribute__((weak)) struct display_trans_ops *display_trans_ops_get(void)
{
    // 返回空指针
    return NULL;
}

static void display_buf_rotate_90(struct display_trans_ctx *ctx, uint16_t *src_buf, uint16_t src_buf_w,
                                    uint16_t *dst_buf, uint16_t area_w, uint16_t area_h)
{
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    uint16_t link_size = area_h;
    uint32_t control, config_low, config_high;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) |
    DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC |
    DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_TTFC_M2M |
    DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    control |= DMA_CH_CTLL_D_SCAT_EN;
    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

    for (uint32_t i = 0; i < link_size; i++) {
        dma_llp_lists[i].SAR = (uint32_t)(src_buf + src_buf_w * i);
        dma_llp_lists[i].DAR = (uint32_t)(dst_buf + (link_size - 1 - i));
        dma_llp_lists[i].LLP = (uint32_t)(&dma_llp_lists[(i + 1) % link_size]);
        dma_llp_lists[i].CTL_LO = control | DMA_CH_CTLL_LLP_EN_MASK;
        dma_llp_lists[i].u.SIZE = area_w;
    }
    dma_llp_lists[link_size - 1].LLP = 0;
    dma_llp_lists[link_size - 1].CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

    uint32_t dst_scat = ((link_size - 1) << SG_INTERVAL_POS) | (1 << SG_COUNT_POS);

    dma_channel_configure_LLP_with_size(CONFIG_LISA_DISPLAY_CPDMA_CH, dma_llp_lists, config_low, config_high, 0,
                                        dst_scat, link_size * area_w);

    if (xSemaphoreTake(ctx->cpdma_done_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("[%s] Failed to take DMA semaphore", __FUNCTION__);
    }
#else
    uint32_t invert = (area_w * area_h) - 1;
    uint32_t initial_i = ((area_w - 1) * area_h);
    for (uint16_t y = 0; y < area_h; y++) {
        uint32_t i = initial_i + y;
        i = invert - i;
        for (uint16_t x = 0; x < area_w; x++) {
            dst_buf[i] = *(src_buf++);
            i += area_h;
        }
        src_buf += src_buf_w - area_w;
    }
#endif
}

static void display_buf_rotate_270(struct display_trans_ctx *ctx, uint16_t *src_buf, uint16_t src_buf_w, uint16_t *dst_buf, uint16_t area_w,
     uint16_t area_h)
{
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    uint16_t link_size = area_h;
    uint32_t control, config_low, config_high;

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) |
    DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_INC |
    DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_TTFC_M2M |
    DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    control |= DMA_CH_CTLL_D_SCAT_EN;
    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

    for (uint32_t i = 0; i < link_size; i++) {
        dma_llp_lists[i].SAR = (uint32_t)(src_buf + src_buf_w * i);
        dma_llp_lists[i].DAR = (uint32_t)(dst_buf + (area_w - 1) * link_size + i);
        dma_llp_lists[i].LLP = (uint32_t)(&dma_llp_lists[(i + 1) % link_size]);
        dma_llp_lists[i].CTL_LO = control | DMA_CH_CTLL_LLP_EN_MASK;
        dma_llp_lists[i].u.SIZE = area_w;
    }
    dma_llp_lists[link_size - 1].LLP = 0;
    dma_llp_lists[link_size - 1].CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

    uint32_t dst_scat = ((link_size - 1) << SG_INTERVAL_POS) | (1 << SG_COUNT_POS);

    dma_channel_configure_LLP_with_size(CONFIG_LISA_DISPLAY_CPDMA_CH, dma_llp_lists, config_low, config_high, 0,
                                        dst_scat, link_size * area_w);

    if (xSemaphoreTake(ctx->cpdma_done_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOGE("[%s] Failed to take DMA semaphore", __FUNCTION__);
    }
#else
    for (uint16_t y = 0; y < area_h; y++) {
        for (uint16_t x = 0; x < area_w; x++) {
            uint32_t dst_idx = (area_w - 1 - x) * area_h + y;
            dst_buf[dst_idx] = *(src_buf++);
        }
        src_buf += src_buf_w - area_w;
    }
#endif
}

int display_trans_ctx_init(uint32_t bpp, uint32_t cmd_bits, uint8_t dc_cmd_level, void *trans_config)
{
    g_disp_trans_context.orientation    = DISPLAY_TRANS_ORIENT_NORMAL;
    g_disp_trans_context.lcd_cmd_bits   = cmd_bits;
    g_disp_trans_context.bits_per_pixel = bpp;
    g_disp_trans_context.dc_cmd_level   = dc_cmd_level;

#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    g_disp_trans_context.cpdma_done_sem = xSemaphoreCreateBinary();
    if (g_disp_trans_context.cpdma_done_sem == NULL) {
        LOGE("[%s] Failed to create CP DMA semaphore", __func__);
        return -1;
    }

    dma_initialize();
    rotate_cpdma_ch = dma_channel_reserve(CONFIG_LISA_DISPLAY_CPDMA_CH, rotate_dma_drv_event, (uint32_t)(&g_disp_trans_context), DMA_CACHE_SYNC_AUTO);
#endif

    g_disp_trans_context.ops = display_trans_ops_get();
    assert(g_disp_trans_context.ops);

    g_disp_trans_context.ops->trans_init(trans_config);
    return 0;
}

static void display_send_data_with_sram_rotate(struct display_trans_ctx *ctx)
{
    uint16_t *curr_buf = ctx->rotate_buf_ping;
    uint16_t *next_buf = ctx->rotate_buf_pong;

    if (!(ctx->orientation == DISPLAY_TRANS_ORIENT_ROTATED_90 || ctx->orientation == DISPLAY_TRANS_ORIENT_ROTATED_270)) {
        LOGE("[%s] not support", __FUNCTION__);
        return;
    }

    if (ctx->h > ROTATE_BUF_MAX_HEIGHT) {
        LOGE("[%s] height(%d) limits overflow", __FUNCTION__, ctx->h);
        return;
    }

    LOGD("[%s] w:%d h:%d", __FUNCTION__, ctx->w, ctx->h);

    if (ctx->orientation == DISPLAY_TRANS_ORIENT_ROTATED_90) {
        display_buf_rotate_90(ctx, ctx->data + ctx->data_offset, ctx->w, curr_buf, ROTATE_BUF_WIDTH, ctx->h);
    } else if (ctx->orientation == DISPLAY_TRANS_ORIENT_ROTATED_270) {
        display_buf_rotate_270(ctx, ctx->data + ctx->data_offset, ctx->w, curr_buf, ROTATE_BUF_WIDTH, ctx->h);
    }

    for (uint16_t i = 0; i < ctx->w; i += ROTATE_BUF_WIDTH) {
        if (ctx->ops->trans_dc_trig)
            ctx->ops->trans_dc_trig(!ctx->dc_cmd_level);

        ctx->ops->trans_image(curr_buf, ROTATE_BUF_WIDTH * ctx->h * ctx->bits_per_pixel / 8);

        if (ctx->orientation == DISPLAY_TRANS_ORIENT_ROTATED_90) {
            ctx->data_offset += ROTATE_BUF_WIDTH;
            display_buf_rotate_90(ctx, ctx->data + ctx->data_offset, ctx->w, next_buf, ROTATE_BUF_WIDTH, ctx->h);
        } else if (ctx->orientation == DISPLAY_TRANS_ORIENT_ROTATED_270) {
            ctx->data_offset -= ROTATE_BUF_WIDTH;
            display_buf_rotate_270(ctx, ctx->data + ctx->data_offset, ctx->w, next_buf, ROTATE_BUF_WIDTH, ctx->h);
        }

        if (ctx->ops->trans_image_wait(pdMS_TO_TICKS(1000)) != 0) {
            CLOGE("[%s] Failed to wait for image transfer", __FUNCTION__);
        }

        uint16_t *temp = curr_buf;
        curr_buf = next_buf;
        next_buf = temp;
    }
}

static inline void _reverse_buffer_bytes(uint8_t *buf, int start, int end)
{
    uint8_t temp = 0;
    while (start < end) {
        temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }
}

static void _display_trans_prepare_cmd_buffer(const void *cmd)
{
    uint8_t *from = (uint8_t *)cmd;
    if (g_disp_trans_context.lcd_cmd_bits > 8) {
        int start = 0;
        int end = g_disp_trans_context.lcd_cmd_bits / 8 - 1;
        _reverse_buffer_bytes(from, start, end);
    }
}

int display_trans_cmd_data(int cmd, const void *data, size_t data_len)
{
    g_disp_trans_context.ops->trans_cs_trig(0);

    if (cmd >= 0) {
        if (g_disp_trans_context.ops->trans_dc_trig)
            g_disp_trans_context.ops->trans_dc_trig(g_disp_trans_context.dc_cmd_level);

        _display_trans_prepare_cmd_buffer(&cmd);
        g_disp_trans_context.ops->trans_cmd((void *)&cmd, g_disp_trans_context.lcd_cmd_bits / 8);
    }

    if (data && data_len) {
        if (g_disp_trans_context.ops->trans_dc_trig)
            g_disp_trans_context.ops->trans_dc_trig(!g_disp_trans_context.dc_cmd_level);

        g_disp_trans_context.ops->trans_cmd((uint8_t *)data, data_len);
    }

    g_disp_trans_context.ops->trans_cs_trig(1);
    return 0;
}

int display_trans_image(int cmd, void *buf, uint32_t w, uint32_t h, enum display_trans_orient orient)
{
    if ((orient != DISPLAY_TRANS_ORIENT_NORMAL) && (w % ROTATE_BUF_WIDTH != 0)) {
        LOGE("[%s] Width alignment is required in rotation mode", __FUNCTION__);
        return -1;
    }

    g_disp_trans_context.data = buf;
    g_disp_trans_context.h = h;
    g_disp_trans_context.w = w;
    g_disp_trans_context.orientation = orient;
    g_disp_trans_context.data_total = w * h * g_disp_trans_context.bits_per_pixel / 8;

    if (orient == DISPLAY_TRANS_ORIENT_ROTATED_90) {
        g_disp_trans_context.data_offset = 0;
    }
    else if (orient == DISPLAY_TRANS_ORIENT_ROTATED_270) {
        g_disp_trans_context.data_offset = w - ROTATE_BUF_WIDTH;
    }
    g_disp_trans_context.ops->trans_cs_trig(0);

    if (cmd >= 0) {
        if (g_disp_trans_context.ops->trans_dc_trig)
            g_disp_trans_context.ops->trans_dc_trig(g_disp_trans_context.dc_cmd_level);

        _display_trans_prepare_cmd_buffer(&cmd);
        g_disp_trans_context.ops->trans_cmd((void *)&cmd, g_disp_trans_context.lcd_cmd_bits / 8);
    }
    

    if (orient == DISPLAY_TRANS_ORIENT_NORMAL) {
        if (g_disp_trans_context.ops->trans_dc_trig)
            g_disp_trans_context.ops->trans_dc_trig(!g_disp_trans_context.dc_cmd_level);

        g_disp_trans_context.ops->trans_image(buf, g_disp_trans_context.data_total);
        if (0 != g_disp_trans_context.ops->trans_image_wait(1000)) {
            LOGE("[%s] Failed to wait trans image done.", __func__);
            g_disp_trans_context.ops->trans_cs_trig(1);
            return -1;
        }
    }
    else {
        display_send_data_with_sram_rotate(&g_disp_trans_context);
    }
    g_disp_trans_context.ops->trans_cs_trig(1);
    return 0;
}