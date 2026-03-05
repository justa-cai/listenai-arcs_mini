/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_display_panel.h"
#include "lisa_device.h"
#include "lisa_display_bus.h"
#include "lisa_display.h"
#include "lisa_gpio.h"
#include "lisa_pwm.h"
#include <stddef.h>
#include <stdint.h>

#define LOG_TAG "lisa_display_panel"
#include <lisa_log.h>

static int panel_send_data_with_sram_rotate(lisa_display_panel_t *panel, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const void *bitmap);

static inline void reverse_buffer_bytes(uint8_t *buf, int start, int end)
{
    while (start < end) {
        uint8_t temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }
}

static void panle_prepare_cmd_buffer(const void *cmd, uint8_t cmd_bits)
{
    uint8_t *from = (uint8_t *)cmd;
    if (cmd_bits > 8) {
        int start = 0;
        int end = (cmd_bits / 8) - 1;
        reverse_buffer_bytes(from, start, end);
    }
}

int panel_write_cmd_data(lisa_display_panel_t *panel, int cmd, uint8_t cmd_bits, const void *data, size_t len)
{
    if (!panel || !panel->bus_dev) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    panle_prepare_cmd_buffer(&cmd, cmd_bits);

    if (panel->cmd_bus_dev) {
        lisa_display_cmd_bus_api_t *api = panel->cmd_bus_dev->api;
        int ret = api->write_cmd(cmd, cmd_bits, data, len);
        return ret;
    }
    
    lisa_display_bus_api_t *bus_api = (lisa_display_bus_api_t *)panel->bus_dev->api;
    bus_api->transfer_control(panel->bus_dev, true);
    int ret = bus_api->trans_cmd_data(panel->bus_dev, cmd, cmd_bits, data, len);
    bus_api->transfer_control(panel->bus_dev, false);

    return ret;
}

int panel_draw_pixels(lisa_display_panel_t *panel, uint32_t cmd, uint16_t cmd_bits, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const void *pixels)
{
    int ret = 0;

    if (!panel || !panel->bus_dev) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (cmd_bits > 0) {
        panle_prepare_cmd_buffer(&cmd, cmd_bits);
    }

    lisa_display_bus_api_t *bus_api = (lisa_display_bus_api_t *)panel->bus_dev->api;
#if CONFIG_DCACHE_ENABLE
	dcache_clean_range((size_t)pixels, (size_t)pixels + ((size_t)w * h * 2));
#endif
    bus_api->transfer_control(panel->bus_dev, true);
    ret = bus_api->trans_cmd_data(panel->bus_dev, cmd, cmd_bits, NULL, 0);
    if (ret != LISA_DEVICE_OK) {
        bus_api->transfer_control(panel->bus_dev, false);
        return ret;
    }

    if (panel->caps.orientation == LISA_DISPLAY_ORIENTATION_90 ||
        panel->caps.orientation == LISA_DISPLAY_ORIENTATION_270) {
        ret = panel_send_data_with_sram_rotate(panel, x, y, w, h, pixels);
    }
    else {
        ret = bus_api->write_pixels(panel->bus_dev, pixels, (size_t)w * h * 2);
        if (ret != LISA_DEVICE_OK) {
            bus_api->transfer_control(panel->bus_dev, false);
            return ret;
        }
        ret  = bus_api->wait_for_completion(panel->bus_dev, 1000);
        if (ret != LISA_DEVICE_OK) {
            bus_api->transfer_control(panel->bus_dev, false);
            return ret;
        }
    }
    bus_api->transfer_control(panel->bus_dev, false);

    return ret;
}

int panel_set_backlight_brightness(lisa_display_backlight_t *backlight, uint8_t brightness)
{
    if (!backlight) {
        return LISA_DEVICE_ERR_INVALID;
    }

    switch (backlight->type) {
    case LISA_DISPLAY_BACKLIGHT_TYPE_PWM: {
        lisa_display_backlight_pwm_config_t *pwm = &backlight->config.pwm;
        if (pwm->dev) {
            if (brightness == 0) {
                lisa_pwm_disable(pwm->dev, pwm->channel);
                return LISA_DEVICE_OK;
            }

            if (brightness >= 100) {
                brightness = 99;
            }
            lisa_pwm_config_t config = { .polarity = LISA_PWM_POLARITY_INVERTED };
            if (backlight->blacklight_polarity == LISA_DISPLAY_BLACKLIGHT_POLARITY_HIGH) {
                config.polarity = LISA_PWM_POLARITY_NORMAL;
            }
            else if (backlight->blacklight_polarity == LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW) {
                config.polarity = LISA_PWM_POLARITY_INVERTED;
            }
            lisa_pwm_configure(pwm->dev, pwm->channel, &config);

            lisa_pwm_set(pwm->dev, pwm->channel, pwm->freq, brightness);
            lisa_pwm_enable(pwm->dev, pwm->channel);
            return LISA_DEVICE_OK;
        }
        break;
    }
    case LISA_DISPLAY_BACKLIGHT_TYPE_SINGLE_WIRE: {
        lisa_display_backlight_single_wire_config_t *sw = &backlight->config.sw;
        if (sw->dev) {
            int level = brightness * sw->steps / 100;
            if (level == sw->current_level) {
                return LISA_DEVICE_OK;
            }

            if (level == 0) {
                lisa_gpio_write_pin(sw->dev, sw->pin, 0);
                lisa_thread_mdelay(3);
            } else {
                if (sw->current_level == 0) {
                    sw->current_level = sw->steps;
                    lisa_gpio_write_pin(sw->dev, sw->pin, 1);
                    lisa_thread_mdelay(30);
                }

                int pulses_start = sw->steps - sw->current_level;
                int pulses_end = sw->steps - level;
                int pulses = (sw->steps + pulses_end - pulses_start) % sw->steps;

                for (int i = 0; i < pulses; i++) {
                    lisa_gpio_write_pin(sw->dev, sw->pin, 0);
                    lisa_thread_mdelay(1);
                    lisa_gpio_write_pin(sw->dev, sw->pin, 1);
                    lisa_thread_mdelay(1);
                }
            }
            sw->current_level = level;
            return LISA_DEVICE_OK;
        }
        break;
    }
    default:
        break;
    }
    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

void panel_reset_pin_set(lisa_display_panel_t *panel, uint8_t level)
{
    if (!panel) {
        return;
    }

    lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, level);
}

void panel_set_mem_area(lisa_display_panel_t *panel, lisa_display_panel_mem_area_t *area, lisa_mem_coord_t x, lisa_mem_coord_t y)
{
    uint16_t new_x, new_y, new_w, new_h;

    if (!panel) {
        return;
    }

    if (panel->caps.orientation  == LISA_DISPLAY_ORIENTATION_90) {
        new_x = area->panel_w - (area->y + area->h);
        new_y = area->x;
        new_w = area->h;
        new_h = area->w;
    }
    else if (panel->caps.orientation == LISA_DISPLAY_ORIENTATION_270) {
        new_x = area->y;
        new_y = area->panel_h - area->x - area->w;
        new_w = area->h;
        new_h = area->w;
    }
    else {
        new_x = area->x;
        new_y = area->y;
        new_w = area->w;
        new_h = area->h;
    }

    new_x += area->x_offset;
    new_y += area->y_offset;

    x[0] = (new_x >> 8);
    x[1] = (new_x & 0xff);
    x[2] = ((new_x + new_w - 1) >> 8);
    x[3] = ((new_x + new_w - 1) & 0xff);

    y[0] = (new_y >> 8);
    y[1] = (new_y & 0xff);
    y[2] = ((new_y + new_h - 1) >> 8);
    y[3] = ((new_y + new_h - 1) & 0xff);
}

#ifndef CONFIG_ROTATE_BUF_MAX_HEIGHT
#define ROTATE_BUF_MAX_HEIGHT 16
#else
#define ROTATE_BUF_MAX_HEIGHT CONFIG_ROTATE_BUF_MAX_HEIGHT
#endif

#ifndef CONFIG_ROATE_BUF_WIDTH
#define ROTATE_BUF_WIDTH      16
#else
#define ROTATE_BUF_WIDTH      CONFIG_ROTATE_BUF_WIDTH
#endif

#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
#include <dma.h>
#endif

typedef struct {
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    lisa_semaphore_t *cpdma_done_sem;
#endif
    uint16_t rotate_buf_ping[ROTATE_BUF_MAX_HEIGHT * ROTATE_BUF_WIDTH];
    uint16_t rotate_buf_pong[ROTATE_BUF_MAX_HEIGHT * ROTATE_BUF_WIDTH];
} panel_rotate_ctx_t;

static panel_rotate_ctx_t g_rotate_ctx;


#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
#include "sysutils.h"
__dtcm_bss__ DMA_LLI dma_llp_lists[ROTATE_BUF_MAX_HEIGHT];

static void rotate_dma_drv_event(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    panel_rotate_ctx_t *ctx = (panel_rotate_ctx_t *)usr_param;
    lisa_semaphore_give(ctx->cpdma_done_sem);
}
#endif


int panel_rotate_init(void)
{
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    g_rotate_ctx.cpdma_done_sem = lisa_semaphore_create(1);
    if (!g_rotate_ctx.cpdma_done_sem) {
        return LISA_DEVICE_ERR_NO_MEM;
    }
    dma_initialize();
    dma_channel_reserve(CONFIG_LISA_DISPLAY_CPDMA_CH, rotate_dma_drv_event, (uint32_t)(&g_rotate_ctx), DMA_CACHE_SYNC_AUTO);
#endif
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
static void _panel_dma_rotate(const uint16_t *src_buf, uint16_t src_buf_w,
                              uint16_t *dst_buf, uint16_t area_w, uint16_t area_h,
                              lisa_display_orientation_t orientation)
{
    uint16_t link_size = area_h;
    uint32_t control, config_low, config_high;

    if (orientation == LISA_DISPLAY_ORIENTATION_90) {
        control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) |
                  DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC |
                  DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_TTFC_M2M |
                  DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    } else { /* 270 */
        control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) |
                  DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_INC |
                  DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_16) | DMA_CH_CTLL_TTFC_M2M |
                  DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
    }
    control |= DMA_CH_CTLL_D_SCAT_EN;
    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;

    for (uint32_t i = 0; i < link_size; i++) {
        dma_llp_lists[i].SAR = (uint32_t)(src_buf + src_buf_w * i);
        if (orientation == LISA_DISPLAY_ORIENTATION_90) {
            dma_llp_lists[i].DAR = (uint32_t)(dst_buf + (link_size - 1 - i));
        } else { /* 270 */
            dma_llp_lists[i].DAR = (uint32_t)(dst_buf + (area_w - 1) * link_size + i);
        }
        dma_llp_lists[i].LLP = (uint32_t)(&dma_llp_lists[(i + 1) % link_size]);
        dma_llp_lists[i].CTL_LO = control | DMA_CH_CTLL_LLP_EN_MASK;
        dma_llp_lists[i].u.SIZE = area_w;
    }
    dma_llp_lists[link_size - 1].LLP = 0;
    dma_llp_lists[link_size - 1].CTL_LO &= ~DMA_CH_CTLL_LLP_EN_MASK;

    uint32_t dst_scat = ((link_size - 1) << SG_INTERVAL_POS) | (1 << SG_COUNT_POS);

    dma_channel_configure_LLP_with_size(CONFIG_LISA_DISPLAY_CPDMA_CH, dma_llp_lists, config_low, config_high, 0,
                                        dst_scat, link_size * area_w);

    if (lisa_semaphore_take(g_rotate_ctx.cpdma_done_sem, 100) != 0) {
        LISA_LOGE(LOG_TAG, "CPDMA rotate timeout");
    }
}
#endif

static void panel_buf_rotate_90(const uint16_t *src_buf, uint16_t src_buf_w,
                                    uint16_t *dst_buf, uint16_t area_w, uint16_t area_h)
{
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    _panel_dma_rotate(src_buf, src_buf_w, dst_buf, area_w, area_h, LISA_DISPLAY_ORIENTATION_90);
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

static void panel_buf_rotate_270(const uint16_t *src_buf, uint16_t src_buf_w, uint16_t *dst_buf, uint16_t area_w,
     uint16_t area_h)
{
#if CONFIG_LISA_DISPLAY_CPDMA_ROTATE
    _panel_dma_rotate(src_buf, src_buf_w, dst_buf, area_w, area_h, LISA_DISPLAY_ORIENTATION_270);
#else
    for (uint16_t y = 0; y < area_h; y++) {
        for (uint16_t x = 0; x < area_w; x++) {
            uint32_t dst_idx = ((area_w - 1 - x) * area_h) + y;
            dst_buf[dst_idx] = *(src_buf++);
        }
        src_buf += src_buf_w - area_w;
    }
#endif
}

static int panel_send_data_with_sram_rotate(lisa_display_panel_t *panel, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const void *bitmap)
{
    uint32_t data_offset = 0;
    uint16_t *curr_buf = g_rotate_ctx.rotate_buf_ping;
    uint16_t *next_buf = g_rotate_ctx.rotate_buf_pong;
    const uint16_t *src_buf = (const uint16_t *)bitmap;

    if (h > ROTATE_BUF_MAX_HEIGHT) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (panel->caps.orientation == LISA_DISPLAY_ORIENTATION_90) {
        data_offset = 0;
        panel_buf_rotate_90(src_buf + data_offset, w, curr_buf, ROTATE_BUF_WIDTH, h);
    } else if (panel->caps.orientation == LISA_DISPLAY_ORIENTATION_270) {
        data_offset = w - ROTATE_BUF_WIDTH;
        panel_buf_rotate_270(src_buf + data_offset, w, curr_buf, ROTATE_BUF_WIDTH, h);
    }
    else {
        LISA_LOGE(LOG_TAG, "Invalid orientation");
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_display_bus_api_t *bus_api = (lisa_display_bus_api_t *)panel->bus_dev->api;

    for (uint16_t i = 0; i < w; i += ROTATE_BUF_WIDTH) {
        size_t chunk_size = (size_t)ROTATE_BUF_WIDTH * h * sizeof(uint16_t);

        // 使用异步传输，实现 CPU/DMA 并行操作
        int ret = bus_api->write_pixels(panel->bus_dev,curr_buf, chunk_size);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Failed to start async transfer: %d", ret);
            return ret;
        }

        // 在 DMA 传输的同时，准备下一块数据
        if (panel->caps.orientation == LISA_DISPLAY_ORIENTATION_90) {
            data_offset += ROTATE_BUF_WIDTH;
            panel_buf_rotate_90(src_buf + data_offset, w, next_buf, ROTATE_BUF_WIDTH, h);
        } else if (panel->caps.orientation == LISA_DISPLAY_ORIENTATION_270) {
            data_offset -= ROTATE_BUF_WIDTH;
            panel_buf_rotate_270(src_buf + data_offset, w, next_buf, ROTATE_BUF_WIDTH, h);
        }

        // 等待当前 DMA 传输完成
        ret = bus_api->wait_for_completion(panel->bus_dev, 1000);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Async transfer timeout or error: %d", ret);
            return ret;
        }

        uint16_t *temp = curr_buf;
        curr_buf = next_buf;
        next_buf = temp;
    }
}
