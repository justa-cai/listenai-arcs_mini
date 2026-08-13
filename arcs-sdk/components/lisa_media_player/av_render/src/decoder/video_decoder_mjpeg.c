/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "av_render_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_time.h>

#if defined(__has_include)
#if __has_include(<jpeglib.h>)
#include <jpeglib.h>
#define AV_RENDER_HAVE_JPEGLIB 1
#else
#define AV_RENDER_HAVE_JPEGLIB 0
#endif
#else
#include <jpeglib.h>
#define AV_RENDER_HAVE_JPEGLIB 1
#endif

#ifndef LOG_TAG
#define LOG_TAG "av_vdec"
#endif

typedef struct {
    uint8_t *jpeg;
    uint32_t size;
    uint32_t pts_ms;

    av_render_free_fn_t free_fn;
    void *free_user;
} mjpeg_pkt_t;

static void rgb565_pool_release(void *data, void *user)
{
    av_render_t *r = (av_render_t *)user;
    if (!data) {
        return;
    }
    if (!r || !r->video_rgb_pool_q || r->should_stop) {
        /* 池化内存：如果缓存池已释放（closing），直接丢弃 */
        return;
    }

    void *ptr = data;
    if (lisa_queue_push(r->video_rgb_pool_q, &ptr, sizeof(ptr), 0) != LISA_OK) {
        /* 缓存池溢出（重复释放或竞态）：静默丢弃 */
    }
}

static int rgb565_pool_init_if_needed(av_render_t *r, uint16_t w, uint16_t h)
{
    if (!r || w == 0 || h == 0) {
        return -EINVAL;
    }

    if (r->video_rgb_pool_q && r->video_rgb_pool_mem) {
        /* 已初始化：要求单块大小一致 */
        uint32_t need = (uint32_t)w * (uint32_t)h * 2U;
        return (need == r->video_rgb_pool_slot_size) ? 0 : -ENOTSUP;
    }

    /*
    * 缓存池的块数量建议覆盖：
     * - video 线程在等待 PTS 时暂存的 pending frame
     * - video_q 中排队的帧
     * - 少量安全余量
     *
    * 如果块数太小，当 video 线程在同步等待阶段暂时“占住”buffer 时，
    * 解码器可能因为拿不到空闲块而丢帧（返回 -EAGAIN）。
     */
    uint16_t slots = 3; /* 最小值 */
    if (r && r->cfg.video_queue_depth > 0) {
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

#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
static uint64_t s_qstat_last_ms;
static uint32_t s_qstat_push_ok;
static uint32_t s_qstat_evict_ok;
static uint32_t s_qstat_drop;

static void qstat_maybe_log(av_render_t *render)
{
    if (!render) {
        return;
    }
    if (should_log_every(&s_qstat_last_ms, 2000)) {
        LISA_LOGI(LOG_TAG,
                  "mjpeg qstat: push_ok=%u evict=%u drop=%u q_wait=%u",
                  (unsigned)s_qstat_push_ok,
                  (unsigned)s_qstat_evict_ok,
                  (unsigned)s_qstat_drop,
                  (unsigned)(render->video_codec_q ? lisa_queue_waiting(render->video_codec_q) : 0));
        s_qstat_push_ok = 0;
        s_qstat_evict_ok = 0;
        s_qstat_drop = 0;
    }
}
#endif

static bool should_log_every(uint64_t *last_ms, uint32_t period_ms)
{
    uint64_t now = lisa_os_get_tick_ms();
    if (*last_ms == 0 || (now - *last_ms) >= period_ms) {
        *last_ms = now;
        return true;
    }
    return false;
}

static int decode_mjpeg_to_rgb565(av_render_t *r,
                                 const uint8_t *jpeg_data,
                                 uint32_t jpeg_size,
                                 uint8_t **out_rgb,
                                 uint16_t *out_w,
                                 uint16_t *out_h,
                                 bool *out_from_pool)
{
#if !AV_RENDER_HAVE_JPEGLIB
    (void)jpeg_data;
    (void)jpeg_size;
    (void)r;
    (void)out_rgb;
    (void)out_w;
    (void)out_h;
    (void)out_from_pool;
    return -ENOTSUP;
#else
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    if (out_from_pool) {
        *out_from_pool = false;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -EINVAL;
    }

    /* 快速解码参数（按常见调参方式关闭部分高质量但耗时的选项） */
    cinfo.out_color_space = JCS_RGB565;
    cinfo.dither_mode = JDITHER_NONE;
    cinfo.do_fancy_upsampling = FALSE;
    cinfo.do_block_smoothing = FALSE;
    cinfo.two_pass_quantize = FALSE;
    cinfo.dct_method = JDCT_IFAST;

    /* 在开始解压前先计算输出尺寸 */
    jpeg_calc_output_dimensions(&cinfo);

    uint16_t w = (uint16_t)cinfo.output_width;
    uint16_t h = (uint16_t)cinfo.output_height;
    size_t out_size = (size_t)w * (size_t)h * 2;

    uint8_t *rgb = NULL;
    bool from_pool = false;

    /* 优先使用缓存池 buffer，避免每帧分配/释放 */
    if (r && r->cfg.display_dev && r->video_stream_set && r->video_info.width == w && r->video_info.height == h) {
        if (rgb565_pool_init_if_needed(r, w, h) == 0 && r->video_rgb_pool_q) {
            rgb = (uint8_t *)rgb565_pool_take(r);
            if (rgb) {
                from_pool = true;
            }
        }
    }

    if (!rgb) {
        rgb = (uint8_t *)lisa_mem_alloc(out_size);
    }

    if (!rgb) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return -ENOMEM;
    }

    jpeg_start_decompress(&cinfo);

    int row_stride = (int)w * 2;
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &rgb[cinfo.output_scanline * row_stride];
        (void)jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    *out_rgb = rgb;
    *out_w = w;
    *out_h = h;
    if (out_from_pool) {
        *out_from_pool = from_pool;
    }
    return 0;
#endif
}

static void vdec_thread_entry(void *arg)
{
    av_render_t *r = (av_render_t *)arg;

    uint64_t stat_last_ms = 0;
    uint32_t decoded = 0;
    uint32_t dropped = 0;
    uint32_t drop_decode_err = 0;
    uint32_t drop_vq_full = 0;

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    uint64_t prof_last_ms = 0;
    uint64_t prof_decode_ms_sum = 0;
    uint32_t prof_decode_ms_max = 0;
    uint32_t prof_decode_cnt = 0;
#endif

    while (!r->should_stop) {
        mjpeg_pkt_t pkt = {0};
        lisa_err_t err = lisa_queue_pop(r->video_codec_q, &pkt, sizeof(pkt), 50);
        if (err != LISA_OK) {
            continue;
        }

        if (!pkt.jpeg || pkt.size == 0) {
            if (pkt.jpeg) {
                if (pkt.free_fn) {
                    pkt.free_fn(pkt.jpeg, pkt.free_user);
                } else {
                    lisa_mem_free(pkt.jpeg);
                }
            }
            continue;
        }

        uint8_t *rgb565 = NULL;
        uint16_t w = 0, h = 0;
        bool from_pool = false;
    #if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
        uint64_t t0 = lisa_os_get_tick_ms();
    #endif
        int ret = decode_mjpeg_to_rgb565(r, pkt.jpeg, pkt.size, &rgb565, &w, &h, &from_pool);
    #if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
        uint64_t t1 = lisa_os_get_tick_ms();
        uint32_t dms = (t1 >= t0) ? (uint32_t)(t1 - t0) : 0;
        prof_decode_ms_sum += dms;
        if (dms > prof_decode_ms_max) {
            prof_decode_ms_max = dms;
        }
        prof_decode_cnt++;
    #endif
        if (pkt.free_fn) {
            pkt.free_fn(pkt.jpeg, pkt.free_user);
        } else {
            lisa_mem_free(pkt.jpeg);
        }
        pkt.jpeg = NULL;

        if (ret != 0) {
            if (rgb565) {
                if (from_pool) {
                    rgb565_pool_release(rgb565, r);
                } else {
                    lisa_mem_free(rgb565);
                }
            }
            dropped++;
            drop_decode_err++;
            continue;
        }

        /* 入队解码后的帧（如果渲染太慢，可能会丢帧） */
        av_render_video_frame_t frame = {
            .data = rgb565,
            .size = (uint32_t)((size_t)w * (size_t)h * 2),
            .width = w,
            .height = h,
            .pts_ms = pkt.pts_ms,
            .free_fn = from_pool ? rgb565_pool_release : NULL,
            .free_user = from_pool ? (void *)r : NULL,
        };
        /*
         * 当 A/V 同步暂存 pending frame 时，video_q 可能会在一段时间内保持满。
         * 用一个有界等待替代“立即丢弃”，以避免浪费已完成的解码工作。
         */
        lisa_err_t qerr = lisa_queue_push(r->video_q, &frame, sizeof(frame), 30);
        if (qerr != LISA_OK) {
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
                LISA_LOGI(LOG_TAG, "mjpeg stat: decoded=%u drop=%u (decode=%u vq=%u) q_wait=%u",
                      (unsigned)decoded,
                      (unsigned)dropped,
                          (unsigned)drop_decode_err,
                          (unsigned)drop_vq_full,
                      (unsigned)lisa_queue_waiting(r->video_codec_q));
#endif

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            uint64_t nowp = lisa_os_get_tick_ms();
            if (prof_last_ms == 0) {
                prof_last_ms = nowp;
            }
            if ((nowp - prof_last_ms) >= 1000) {
                uint32_t avg = (prof_decode_cnt > 0) ? (uint32_t)(prof_decode_ms_sum / prof_decode_cnt) : 0;
                LISA_LOGI(LOG_TAG,
                          "prof: decode avg=%ums max=%ums cnt=%u enq=%u drop=%u vq_wait=%u pool_wait=%u",
                          (unsigned)avg,
                          (unsigned)prof_decode_ms_max,
                          (unsigned)prof_decode_cnt,
                          (unsigned)decoded,
                          (unsigned)dropped,
                          (unsigned)(r->video_q ? lisa_queue_waiting(r->video_q) : 0),
                          (unsigned)(r->video_rgb_pool_q ? lisa_queue_waiting(r->video_rgb_pool_q) : 0));
                prof_last_ms = nowp;
                prof_decode_ms_sum = 0;
                prof_decode_ms_max = 0;
                prof_decode_cnt = 0;
            }
#endif
            decoded = 0;
            dropped = 0;
            drop_decode_err = 0;
            drop_vq_full = 0;
        }
    }

    while (r->video_codec_q && lisa_queue_waiting(r->video_codec_q) > 0) {
        mjpeg_pkt_t pkt = {0};
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

static int mjpeg_push(av_render_t *render,
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
        /* 尚未初始化 */
        return -EIO;
    }

    mjpeg_pkt_t pkt = {
        .jpeg = jpeg,
        .size = (uint32_t)size,
        .pts_ms = pts_ms,
        .free_fn = free_fn,
        .free_user = free_user,
    };

    /* 最新包策略：队列满则淘汰一个旧包后重试 */
    if (lisa_queue_push(render->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
        mjpeg_pkt_t old = {0};
        if (lisa_queue_pop(render->video_codec_q, &old, sizeof(old), 0) == LISA_OK) {
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
            s_qstat_evict_ok++;
#endif
            if (old.jpeg) {
                if (old.free_fn) {
                    old.free_fn(old.jpeg, old.free_user);
                } else {
                    lisa_mem_free(old.jpeg);
                }
            }
        }

        if (lisa_queue_push(render->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
            s_qstat_drop++;
            qstat_maybe_log(render);
#endif
            return -EAGAIN;
        }
    }

#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
    s_qstat_push_ok++;
    qstat_maybe_log(render);
#endif

    return 0;
}

static int mjpeg_open(av_render_t *r)
{
#if !AV_RENDER_HAVE_JPEGLIB
    (void)r;
    return -ENOTSUP;
#else
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

    r->video_codec_q = lisa_queue_create(q_depth, (uint8_t *)"av_mjpeg", sizeof(mjpeg_pkt_t));
    if (!r->video_codec_q) {
        return -ENOMEM;
    }

    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"av_vdec",
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
#endif
}

void av_render_mjpeg_close(av_render_t *r)
{
    if (!r) {
        return;
    }
    /* 线程通过 av_render_close() 设置 r->should_stop 的方式停止 */
    if (r->video_codec_thread) {
        lisa_thread_delete(r->video_codec_thread);
        r->video_codec_thread = NULL;
    }
    if (r->video_codec_q) {
        while (lisa_queue_waiting(r->video_codec_q) > 0) {
            mjpeg_pkt_t pkt = {0};
            if (lisa_queue_pop(r->video_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
                break;
            }
            if (pkt.jpeg) {
                lisa_mem_free(pkt.jpeg);
            }
        }
        lisa_queue_delete(r->video_codec_q);
        r->video_codec_q = NULL;
    }
}

int av_render_mjpeg_push(av_render_t *render,
                         uint8_t *jpeg,
                         size_t size,
                         uint32_t pts_ms,
                         av_render_free_fn_t free_fn,
                         void *free_user)
{
#if !AV_RENDER_HAVE_JPEGLIB
    (void)render;
    (void)jpeg;
    (void)size;
    (void)pts_ms;
    (void)free_fn;
    (void)free_user;
    return -ENOTSUP;
#else
    if (!render || !jpeg || size == 0) {
        return -EINVAL;
    }

    if (!render->video_codec_q || !render->video_codec_thread) {
        int ret = mjpeg_open(render);
        if (ret != 0) {
            return ret;
        }
    }

    return mjpeg_push(render, jpeg, size, pts_ms, free_fn, free_user);
#endif
}
