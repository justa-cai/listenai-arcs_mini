/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LOG_TAG
#define LOG_TAG "av_v_render"
#endif

#include "av_render_internal.h"

#include "color_convert/rotate.h"

#include <errno.h>

#include <lisa_log.h>
#include <lisa_time.h>
#include <lisa_display.h>
#include <lisa_device.h>
#include <lisa_mem.h>

static int lisa_video_ops_init(av_render_t *render, void *user)
{
    (void)user;
    return video_render_init(render);
}

static int lisa_video_ops_write_rgb565(av_render_t *render,
                                      const uint8_t *rgb565,
                                      uint32_t size,
                                      uint16_t width,
                                      uint16_t height,
                                      uint32_t pts_ms,
                                      void *user)
{
    (void)user;
    if (!render || !rgb565 || size == 0 || width == 0 || height == 0) {
        return -EINVAL;
    }

    av_render_video_frame_t frame = {
        .data = (uint8_t *)rgb565,
        .size = size,
        .width = width,
        .height = height,
        .pts_ms = pts_ms,
        .free_fn = NULL,
        .free_user = NULL,
    };
    return video_render_write_rgb565(render, &frame);
}

const av_render_video_renderer_ops_t av_render_lisa_video_renderer_ops = {
    .init = lisa_video_ops_init,
    .write_rgb565 = lisa_video_ops_write_rgb565,
    .deinit = NULL,
};

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)

static uint64_t s_prof_last_ms;
static uint64_t s_prof_rot_ms_sum;
static uint32_t s_prof_rot_ms_max;
static uint64_t s_prof_lcd_ms_sum;
static uint32_t s_prof_lcd_ms_max;
static uint32_t s_prof_cnt;

static inline bool prof_should_log(uint64_t *last_ms, uint32_t period_ms)
{
    uint64_t now = lisa_os_get_tick_ms();
    if (*last_ms == 0 || (now - *last_ms) >= period_ms) {
        *last_ms = now;
        return true;
    }
    return false;
}
#endif

int video_render_write_rgb565(av_render_t *render, const av_render_video_frame_t *frame)
{
    if (!render || !frame || !frame->data || frame->size == 0) {
        return -EINVAL;
    }
    if (!render->cfg.display_dev || !render->video_stream_set) {
        return -ENODEV;
    }

    lisa_display_buffer_desc_t desc = {0};

    bool rotate_ccw = false;
    const bool do_rotate = av_render_rotate_select(render->cfg.rotate_mode, frame->width, frame->height,
                                                   render->display_width, render->display_height, &rotate_ccw);
    // LISA_LOGI(LOG_TAG, "frame %ux%u, display %ux%u, rotate_mode=%d, do_rotate=%d, rotate_ccw=%d",
    //           (unsigned)frame->width,
    //           (unsigned)frame->height,
    //           (unsigned)render->display_width,
    //           (unsigned)render->display_height,
    //           (int)render->cfg.rotate_mode,
    //           (int)do_rotate,
    //           (int)rotate_ccw);
    if (do_rotate) {
        const size_t out_size = (size_t)render->display_width * (size_t)render->display_height * 2;
        if (render->rotate_buf_size != out_size) {
            if (render->rotate_buf) {
                lisa_mem_free(render->rotate_buf);
                render->rotate_buf = NULL;
            }
            render->rotate_buf = (uint8_t *)lisa_mem_alloc(out_size);
            render->rotate_buf_size = render->rotate_buf ? out_size : 0;
        }

        if (!render->rotate_buf) {
            return -ENOMEM;
        }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
        uint64_t t0 = lisa_os_get_tick_ms();
#endif
        av_render_rotate_rgb565_90((uint16_t *)render->rotate_buf, (const uint16_t *)frame->data, frame->width,
                                   frame->height, rotate_ccw);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
        uint64_t t1 = lisa_os_get_tick_ms();
        uint32_t rms = (t1 >= t0) ? (uint32_t)(t1 - t0) : 0;
        s_prof_rot_ms_sum += rms;
        if (rms > s_prof_rot_ms_max) {
            s_prof_rot_ms_max = rms;
        }
#endif

        desc.width = render->display_width;
        desc.height = render->display_height;
        desc.buf_size = out_size;

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
        uint64_t t2 = lisa_os_get_tick_ms();
#endif
        int ret = lisa_display_write((lisa_device_t *)render->cfg.display_dev, 0, 0, &desc, render->rotate_buf);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
        uint64_t t3 = lisa_os_get_tick_ms();
        uint32_t lms = (t3 >= t2) ? (uint32_t)(t3 - t2) : 0;
        s_prof_lcd_ms_sum += lms;
        if (lms > s_prof_lcd_ms_max) {
            s_prof_lcd_ms_max = lms;
        }
        s_prof_cnt++;
        if (prof_should_log(&s_prof_last_ms, 1000)) {
            uint32_t avg_r = (s_prof_cnt > 0) ? (uint32_t)(s_prof_rot_ms_sum / s_prof_cnt) : 0;
            uint32_t avg_l = (s_prof_cnt > 0) ? (uint32_t)(s_prof_lcd_ms_sum / s_prof_cnt) : 0;
            LISA_LOGI(LOG_TAG, "prof: rotate avg=%ums max=%ums; lcd avg=%ums max=%ums; cnt=%u",
                      (unsigned)avg_r,
                      (unsigned)s_prof_rot_ms_max,
                      (unsigned)avg_l,
                      (unsigned)s_prof_lcd_ms_max,
                      (unsigned)s_prof_cnt);
            s_prof_rot_ms_sum = 0;
            s_prof_rot_ms_max = 0;
            s_prof_lcd_ms_sum = 0;
            s_prof_lcd_ms_max = 0;
            s_prof_cnt = 0;
        }
#endif
        return ret;
    }

    desc.width = frame->width;
    desc.height = frame->height;
    desc.buf_size = frame->size;

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    uint64_t t2 = lisa_os_get_tick_ms();
#endif
    int ret = lisa_display_write((lisa_device_t *)render->cfg.display_dev, 0, 0, &desc, frame->data);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    uint64_t t3 = lisa_os_get_tick_ms();
    uint32_t lms = (t3 >= t2) ? (uint32_t)(t3 - t2) : 0;
    s_prof_lcd_ms_sum += lms;
    if (lms > s_prof_lcd_ms_max) {
        s_prof_lcd_ms_max = lms;
    }
    s_prof_cnt++;
    if (prof_should_log(&s_prof_last_ms, 1000)) {
        uint32_t avg_l = (s_prof_cnt > 0) ? (uint32_t)(s_prof_lcd_ms_sum / s_prof_cnt) : 0;
        LISA_LOGI(LOG_TAG, "prof: lcd avg=%ums max=%ums; cnt=%u",
                  (unsigned)avg_l,
                  (unsigned)s_prof_lcd_ms_max,
                  (unsigned)s_prof_cnt);
        s_prof_lcd_ms_sum = 0;
        s_prof_lcd_ms_max = 0;
        s_prof_cnt = 0;
    }
#endif
    return ret;
}

int video_render_init(av_render_t *render)
{
    if (!render) {
        return -EINVAL;
    }

    if (render->cfg.display_dev && (render->display_width == 0 || render->display_height == 0)) {
        lisa_display_capabilities_t caps;
        if (lisa_display_get_capabilities((lisa_device_t *)render->cfg.display_dev, &caps) == LISA_DEVICE_OK) {
            render->display_width = (uint16_t)caps.width;
            render->display_height = (uint16_t)caps.height;
        }
    }

    return 0;
}

int av_render_default_display_write_rgb565(av_render_t *render,
                                          uint16_t x,
                                          uint16_t y,
                                          const uint16_t *rgb565,
                                          uint16_t width,
                                          uint16_t height)
{
    if (!render || !render->cfg.display_dev || !rgb565 || width == 0 || height == 0) {
        return -EINVAL;
    }

    lisa_display_buffer_desc_t desc = {
        .width = width,
        .height = height,
        .buf_size = (size_t)width * (size_t)height * sizeof(uint16_t),
    };

    return lisa_display_write((lisa_device_t *)render->cfg.display_dev, x, y, &desc, rgb565);
}
