/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "av_render_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(CONFIG_AVI_PLAYER_LISA_JPEG_SOFT_FALLBACK)
#if defined(__has_include)
#if __has_include(<jpeglib.h>)
#include <jpeglib.h>
#define AV_RENDER_HW_FALLBACK_HAVE_JPEGLIB 1
#else
#define AV_RENDER_HW_FALLBACK_HAVE_JPEGLIB 0
#endif
#else
#include <jpeglib.h>
#define AV_RENDER_HW_FALLBACK_HAVE_JPEGLIB 1
#endif
#endif

#if defined(CONFIG_AVI_PLAYER_VIDEO_DECODER_LISA_JPEG)
#include <lisa_jpeg.h>
#endif
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_time.h>

#ifndef LOG_TAG
#define LOG_TAG "av_vdec_hw"
#endif

#if defined(CONFIG_AVI_PLAYER_VIDEO_DECODER_LISA_JPEG)

typedef struct {
    uint8_t *jpeg;
    uint32_t size;
    uint32_t pts_ms;

    av_render_free_fn_t free_fn;
    void *free_user;
} lisa_jpeg_pkt_t;

static inline void free_lisa_jpeg_pkt(lisa_jpeg_pkt_t *pkt)
{
    if (!pkt || !pkt->jpeg) {
        return;
    }
    if (pkt->free_fn) {
        pkt->free_fn(pkt->jpeg, pkt->free_user);
    } else {
        lisa_mem_free(pkt->jpeg);
    }
    pkt->jpeg = NULL;
}

static bool should_log_every(uint64_t *last_ms, uint32_t period_ms)
{
    uint64_t now = lisa_os_get_tick_ms();
    if (*last_ms == 0 || (now - *last_ms) >= period_ms) {
        *last_ms = now;
        return true;
    }
    return false;
}

static void rgb565_pool_release(void *data, void *user)
{
    av_render_t *r = (av_render_t *)user;
    if (!data) {
        return;
    }
    if (!r || !r->video_rgb_pool_q || r->should_stop) {
        return;
    }

    void *ptr = data;
    (void)lisa_queue_push(r->video_rgb_pool_q, &ptr, sizeof(ptr), 0);
}

static int rgb565_pool_init_if_needed(av_render_t *r, uint16_t w, uint16_t h)
{
    if (!r || w == 0 || h == 0) {
        return -EINVAL;
    }

    if (r->video_rgb_pool_q && r->video_rgb_pool_mem) {
        uint32_t need = (uint32_t)w * (uint32_t)h * 2U;
        return (need == r->video_rgb_pool_slot_size) ? 0 : -ENOTSUP;
    }

    uint16_t slots = 3;
    if (r->cfg.video_queue_depth > 0) {
        uint32_t want = r->cfg.video_queue_depth + 2U;
        if (want > 8U) {
            want = 8U;
        }
        if (want > slots) {
            slots = (uint16_t)want;
        }
    }

    const uint32_t slot_size = (uint32_t)w * (uint32_t)h * 2U;
    const uint32_t pool_size = (uint32_t)slots * slot_size;

    uint8_t *mem = (uint8_t *)lisa_mem_alloc(pool_size);
    if (!mem) {
        return -ENOMEM;
    }

    lisa_queue_t *q = lisa_queue_create(slots, (uint8_t *)"av_vrgb", sizeof(void *));
    if (!q) {
        lisa_mem_free(mem);
        return -ENOMEM;
    }

    r->video_rgb_pool_mem = mem;
    r->video_rgb_pool_q = q;
    r->video_rgb_pool_slot_size = slot_size;
    r->video_rgb_pool_slots = slots;

    for (uint16_t i = 0; i < slots; i++) {
        void *ptr = (void *)(mem + (uint32_t)i * slot_size);
        (void)lisa_queue_push(q, &ptr, sizeof(ptr), 0);
    }

    return 0;
}

static void *rgb565_pool_take(av_render_t *r)
{
    if (!r || !r->video_rgb_pool_q) {
        return NULL;
    }
    void *ptr = NULL;
    if (lisa_queue_pop(r->video_rgb_pool_q, &ptr, sizeof(ptr), 0) != LISA_OK) {
        return NULL;
    }
    return ptr;
}

static int decode_mjpeg_to_rgb565_hw(av_render_t *r,
                                     const uint8_t *jpeg_data,
                                     uint32_t jpeg_size,
                                     uint8_t **out_rgb,
                                     uint16_t *out_w,
                                     uint16_t *out_h,
                                     bool *out_from_pool,
                                     bool allow_sw_fallback)
{
    if (!r || !jpeg_data || jpeg_size == 0 || !out_rgb || !out_w || !out_h || !out_from_pool) {
        return -EINVAL;
    }

    if (!r->video_stream_set || r->video_info.width == 0 || r->video_info.height == 0) {
        return -EINVAL;
    }

    uint16_t exp_w = r->video_info.width;
    uint16_t exp_h = r->video_info.height;
    uint32_t out_size = (uint32_t)exp_w * (uint32_t)exp_h * 2U;

    uint8_t *rgb = NULL;
    bool from_pool = false;

    if (rgb565_pool_init_if_needed(r, exp_w, exp_h) == 0) {
        rgb = (uint8_t *)rgb565_pool_take(r);
        if (rgb) {
            from_pool = true;
        }
    }

    if (!rgb) {
        rgb = (uint8_t *)lisa_mem_alloc(out_size);
        if (!rgb) {
            return -ENOMEM;
        }
    }

    uint16_t dec_w = 0;
    uint16_t dec_h = 0;
    lisa_jpeg_decoder_cfg_t dec_cfg = {
        .output_format = LISA_JPEG_PIXEL_FORMAT_RGB565,
    };

#if defined(CONFIG_AVI_PLAYER_LISA_JPEG_SOFT_FALLBACK) && AV_RENDER_HW_FALLBACK_HAVE_JPEGLIB
    int sw_fallback_ok = 0;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
#endif

    uint32_t ret = 0;
    dec_w = 0;
    dec_h = 0;
    ret = lisa_jpeg_decoder(jpeg_data, jpeg_size, rgb, &dec_w, &dec_h, dec_cfg);
    if (ret != 0 || dec_w == 0 || dec_h == 0) {
#if defined(CONFIG_AVI_PLAYER_LISA_JPEG_SOFT_FALLBACK) && AV_RENDER_HW_FALLBACK_HAVE_JPEGLIB
        if (allow_sw_fallback) {
            cinfo.err = jpeg_std_error(&jerr);
            jpeg_create_decompress(&cinfo);
            jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);

            if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
                cinfo.out_color_space = JCS_RGB565;
                cinfo.dither_mode = JDITHER_NONE;
                cinfo.do_fancy_upsampling = FALSE;
                cinfo.do_block_smoothing = FALSE;
                cinfo.two_pass_quantize = FALSE;
                cinfo.dct_method = JDCT_IFAST;

                jpeg_calc_output_dimensions(&cinfo);
                if ((uint16_t)cinfo.output_width == exp_w && (uint16_t)cinfo.output_height == exp_h) {
                    jpeg_start_decompress(&cinfo);
                    while (cinfo.output_scanline < cinfo.output_height) {
                        row_pointer[0] = &rgb[cinfo.output_scanline * ((uint32_t)exp_w * 2U)];
                        (void)jpeg_read_scanlines(&cinfo, row_pointer, 1);
                    }
                    jpeg_finish_decompress(&cinfo);
                    sw_fallback_ok = 1;
                    dec_w = exp_w;
                    dec_h = exp_h;
                }
            }
            jpeg_destroy_decompress(&cinfo);

            if (sw_fallback_ok) {
                LISA_LOGW(LOG_TAG, "hw jpeg decode failed(ret=%u), fallback to sw decoder", (unsigned)ret);
                *out_rgb = rgb;
                *out_w = dec_w;
                *out_h = dec_h;
                *out_from_pool = from_pool;
                return 0;
            }
        }
#endif

        LISA_LOGW(LOG_TAG, "hw jpeg decode failed(ret=%u), drop frame", (unsigned)ret);
        if (from_pool) {
            rgb565_pool_release(rgb, r);
        } else {
            lisa_mem_free(rgb);
        }
        return -EIO;
    }

    *out_rgb = rgb;
    *out_w = dec_w;
    *out_h = dec_h;
    *out_from_pool = from_pool;
    return 0;
}

static void vdec_thread_entry(void *arg)
{
    av_render_t *r = (av_render_t *)arg;

    uint64_t stat_last_ms = 0;
    uint32_t decoded = 0;
    uint32_t dropped = 0;
    uint32_t drop_decode_err = 0;
    uint32_t drop_vq_full = 0;
    uint32_t drop_older_pkt = 0;

    while (!r->should_stop) {
        lisa_jpeg_pkt_t pkt = {0};
        if (lisa_queue_pop(r->video_codec_q, &pkt, sizeof(pkt), 50) != LISA_OK) {
            continue;
        }

        while (r->video_codec_q && lisa_queue_waiting(r->video_codec_q) > 0) {
            lisa_jpeg_pkt_t newer = {0};
            if (lisa_queue_pop(r->video_codec_q, &newer, sizeof(newer), 0) != LISA_OK) {
                break;
            }
            free_lisa_jpeg_pkt(&pkt);
            pkt = newer;
            dropped++;
            drop_older_pkt++;
        }

        if (!pkt.jpeg || pkt.size == 0) {
            free_lisa_jpeg_pkt(&pkt);
            continue;
        }

        uint8_t *rgb565 = NULL;
        uint16_t w = 0;
        uint16_t h = 0;
        bool from_pool = false;

        bool allow_sw_fallback = true;
    #if defined(CONFIG_AVI_PLAYER_LISA_JPEG_SOFT_FALLBACK)
        uint32_t codec_q_wait = (r->video_codec_q ? lisa_queue_waiting(r->video_codec_q) : 0U);
        uint32_t video_q_wait = (r->video_q ? lisa_queue_waiting(r->video_q) : 0U);
        if (codec_q_wait > 0U || video_q_wait > 1U) {
            allow_sw_fallback = false;
        }
    #else
        allow_sw_fallback = false;
    #endif

        int ret = decode_mjpeg_to_rgb565_hw(r, pkt.jpeg, pkt.size, &rgb565, &w, &h, &from_pool, allow_sw_fallback);
        free_lisa_jpeg_pkt(&pkt);

        if (ret != 0) {
            dropped++;
            drop_decode_err++;
            continue;
        }

        av_render_video_frame_t frame = {
            .data = rgb565,
            .size = (uint32_t)((size_t)w * (size_t)h * 2),
            .width = w,
            .height = h,
            .pts_ms = pkt.pts_ms,
            .free_fn = from_pool ? rgb565_pool_release : NULL,
            .free_user = from_pool ? (void *)r : NULL,
        };

        if (lisa_queue_push(r->video_q, &frame, sizeof(frame), 30) != LISA_OK) {
            if (from_pool) {
                rgb565_pool_release(rgb565, r);
            } else {
                lisa_mem_free(rgb565);
            }
            dropped++;
            drop_vq_full++;
        } else {
            decoded++;
        }

        if (should_log_every(&stat_last_ms, 1000)) {
#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
            LISA_LOGI(LOG_TAG,
                      "hwjpeg stat: decoded=%u drop=%u (decode=%u vq=%u old=%u) q_wait=%u",
                      (unsigned)decoded,
                      (unsigned)dropped,
                      (unsigned)drop_decode_err,
                      (unsigned)drop_vq_full,
                      (unsigned)drop_older_pkt,
                      (unsigned)(r->video_codec_q ? lisa_queue_waiting(r->video_codec_q) : 0));
#endif
            decoded = 0;
            dropped = 0;
            drop_decode_err = 0;
            drop_vq_full = 0;
            drop_older_pkt = 0;
        }
    }

    while (r->video_codec_q && lisa_queue_waiting(r->video_codec_q) > 0) {
        lisa_jpeg_pkt_t pkt = {0};
        if (lisa_queue_pop(r->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
            break;
        }
        free_lisa_jpeg_pkt(&pkt);
    }

    if (r) {
        r->video_codec_thread_exited = true;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            r->video_codec_thread = NULL;
            lisa_mutex_unlock(r->state_lock);
        } else {
            r->video_codec_thread = NULL;
        }
    }
}

static int lisa_jpeg_open(av_render_t *r)
{
    uint32_t q_depth = 2;
#if defined(CONFIG_AVI_PLAYER_MJPEG_DECODE_QUEUE_DEPTH)
    q_depth = (uint32_t)CONFIG_AVI_PLAYER_MJPEG_DECODE_QUEUE_DEPTH;
    if (q_depth == 0) {
        q_depth = 2;
    }
#endif

    uint32_t stack_size = 24 * 1024;
#if defined(CONFIG_AVI_PLAYER_MJPEG_DECODE_THREAD_STACK_SIZE)
    stack_size = (uint32_t)CONFIG_AVI_PLAYER_MJPEG_DECODE_THREAD_STACK_SIZE;
    if (stack_size < 4096) {
        stack_size = 4096;
    }
#endif

    uint32_t priority = 2;
#if defined(CONFIG_AVI_PLAYER_MJPEG_DECODE_THREAD_PRIORITY)
    priority = (uint32_t)CONFIG_AVI_PLAYER_MJPEG_DECODE_THREAD_PRIORITY;
    if (priority == 0) {
        priority = 2;
    }
#endif

    if (!r) {
        return -EINVAL;
    }
    if (r->video_codec_q || r->video_codec_thread) {
        return 0;
    }

    r->video_codec_q = lisa_queue_create(q_depth, (uint8_t *)"av_ljpeg", sizeof(lisa_jpeg_pkt_t));
    if (!r->video_codec_q) {
        return -ENOMEM;
    }

    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"av_vdec_hw",
        .stack_size = stack_size,
        .priority = priority,
    };

    r->video_codec_thread = lisa_thread_create(&attr, vdec_thread_entry, r);
    if (!r->video_codec_thread) {
        lisa_queue_delete(r->video_codec_q);
        r->video_codec_q = NULL;
        return -ENOMEM;
    }

    return 0;
}

void av_render_lisa_jpeg_close(av_render_t *r)
{
    if (!r) {
        return;
    }

    if (r->video_codec_thread) {
        lisa_thread_delete(r->video_codec_thread);
        r->video_codec_thread = NULL;
    }

    if (r->video_codec_q) {
        while (lisa_queue_waiting(r->video_codec_q) > 0) {
            lisa_jpeg_pkt_t pkt = {0};
            if (lisa_queue_pop(r->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
                break;
            }
            if (pkt.jpeg) {
                if (pkt.free_fn) {
                    pkt.free_fn(pkt.jpeg, pkt.free_user);
                } else {
                    lisa_mem_free(pkt.jpeg);
                }
            }
        }
        lisa_queue_delete(r->video_codec_q);
        r->video_codec_q = NULL;
    }
}

int av_render_lisa_jpeg_push(av_render_t *render,
                             uint8_t *jpeg,
                             size_t size,
                             uint32_t pts_ms,
                             av_render_free_fn_t free_fn,
                             void *free_user)
{
    if (!render || !jpeg || size == 0) {
        return -EINVAL;
    }

    if (!render->video_codec_q || !render->video_codec_thread) {
        int ret = lisa_jpeg_open(render);
        if (ret != 0) {
            return ret;
        }
    }

    lisa_jpeg_pkt_t pkt = {
        .jpeg = jpeg,
        .size = (uint32_t)size,
        .pts_ms = pts_ms,
        .free_fn = free_fn,
        .free_user = free_user,
    };

    if (lisa_queue_push(render->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
        lisa_jpeg_pkt_t old = {0};
        if (lisa_queue_pop(render->video_codec_q, &old, sizeof(old), 0) == LISA_OK && old.jpeg) {
            if (old.free_fn) {
                old.free_fn(old.jpeg, old.free_user);
            } else {
                lisa_mem_free(old.jpeg);
            }
        }

        if (lisa_queue_push(render->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
            return -EAGAIN;
        }
    }

    return 0;
}

#else

int av_render_lisa_jpeg_push(av_render_t *render,
                             uint8_t *jpeg,
                             size_t size,
                             uint32_t pts_ms,
                             av_render_free_fn_t free_fn,
                             void *free_user)
{
    (void)render;
    (void)jpeg;
    (void)size;
    (void)pts_ms;
    (void)free_fn;
    (void)free_user;
    return -ENOTSUP;
}

void av_render_lisa_jpeg_close(av_render_t *r)
{
    (void)r;
}

#endif
