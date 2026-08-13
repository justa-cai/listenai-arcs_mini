/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui_playback.h"
#include "app_context.h"
#include "playback_mgr.h"
#include "lvgl.h"
#include "lisa_mem.h"

#include <stdio.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ui_playback"
#include <lisa_log.h>

static lv_obj_t *s_screen;
static lv_timer_t *s_poll_timer;
static lv_obj_t *s_progress_bar;
static lv_obj_t *s_time_label;

/* LVGL video renderer ops: write decoded frames to an LVGL image widget */
static int lvgl_video_init(av_render_t *render, void *user)
{
    (void)render;
    (void)user;
    return 0;
}

static int lvgl_video_write_rgb565(av_render_t *render,
                                    const uint8_t *rgb565,
                                    uint32_t size,
                                    uint16_t width,
                                    uint16_t height,
                                    uint32_t pts_ms,
                                    void *user)
{
    (void)render;
    (void)pts_ms;
    (void)user;

    if (!rgb565 || size == 0 || width == 0 || height == 0) {
        return -1;
    }

    app_context_t *ctx = app_ctx_get();
    avi_lvgl_render_ctx_t *rctx = &ctx->render_ctx;

    if (!rctx->lock) {
        return -1;
    }

    if (xSemaphoreTake(rctx->lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return -1;
    }

    if (rctx->frame_buf_size < size) {
        if (rctx->frame_buf) {
            lisa_mem_free(rctx->frame_buf);
            rctx->frame_buf = NULL;
            rctx->frame_buf_size = 0;
        }
        rctx->frame_buf = (uint8_t *)lisa_mem_alloc(size);
        if (!rctx->frame_buf) {
            xSemaphoreGive(rctx->lock);
            return -1;
        }
        rctx->frame_buf_size = size;
    }

    memcpy(rctx->frame_buf, rgb565, size);

    rctx->frame_w = width;
    rctx->frame_h = height;
    rctx->img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    rctx->img_dsc.header.w = width;
    rctx->img_dsc.header.h = height;
    rctx->img_dsc.data = rctx->frame_buf;
    rctx->img_dsc.data_size = size;
    rctx->first_frame_arrived = true;
    rctx->frame_dirty = true;

    xSemaphoreGive(rctx->lock);
    return 0;
}

static void lvgl_video_deinit(av_render_t *render, void *user)
{
    (void)render;
    (void)user;
}

static const av_render_video_renderer_ops_t s_lvgl_video_ops = {
    .init = lvgl_video_init,
    .write_rgb565 = lvgl_video_write_rgb565,
    .deinit = lvgl_video_deinit,
};

/* Button callback: stop playback */
static void btn_stop_cb(lv_event_t *e)
{
    (void)e;
    LISA_LOGI(LOG_TAG, "Stop button pressed");
    playback_stop();

    screen_id_t prev = app_ctx_get()->previous_screen;
    app_switch_screen(prev);
}

/* Timer to detect when playback finishes naturally */
static void playback_poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!playback_is_active()) {
        LISA_LOGI(LOG_TAG, "Playback finished, returning to list");
        if (s_poll_timer) {
            lv_timer_del(s_poll_timer);
            s_poll_timer = NULL;
        }
        screen_id_t prev = app_ctx_get()->previous_screen;
        app_switch_screen(prev);
        return;
    }

    /* Update progress bar and time label */
    uint32_t pos_ms = 0, dur_ms = 0;
    if (playback_get_progress(&pos_ms, &dur_ms) == 0 && dur_ms > 0) {
        uint32_t pct = (uint32_t)((uint64_t)pos_ms * 100 / dur_ms);
        if (pct > 100) {
            pct = 100;
        }
        if (s_progress_bar) {
            lv_bar_set_value(s_progress_bar, (int32_t)pct, LV_ANIM_OFF);
        }
        if (s_time_label) {
            uint32_t pos_sec = pos_ms / 1000;
            uint32_t dur_sec = dur_ms / 1000;
            char buf[32];
            snprintf(buf, sizeof(buf), "%u:%02u / %u:%02u",
                     pos_sec / 60, pos_sec % 60,
                     dur_sec / 60, dur_sec % 60);
            lv_label_set_text(s_time_label, buf);
        }
    }
}

/* Refresh the LVGL image from the render context (called from ui task) */
void ui_playback_refresh_frame(void)
{
    app_context_t *ctx = app_ctx_get();
    avi_lvgl_render_ctx_t *rctx = &ctx->render_ctx;

    if (!rctx->img || !rctx->img_dsc.data) {
        return;
    }

    if (rctx->first_frame_arrived) {
        lv_img_set_src(rctx->img, &rctx->img_dsc);
        lv_obj_center(rctx->img);
        lv_obj_invalidate(rctx->img);
    }
}

void ui_playback_create(const char *path_or_url, bool is_url)
{
    app_context_t *ctx = app_ctx_get();
    avi_lvgl_render_ctx_t *rctx = &ctx->render_ctx;

    /* Create render lock if needed */
    if (!rctx->lock) {
        rctx->lock = xSemaphoreCreateMutex();
    }
    rctx->first_frame_arrived = false;
    rctx->frame_dirty = false;

    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);

    /* Video image widget —— 不让它吃触摸事件，否则 stop 按钮点不到 */
    rctx->img = lv_img_create(s_screen);
    lv_obj_center(rctx->img);
    lv_obj_clear_flag(rctx->img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(rctx->img, LV_OBJ_FLAG_SCROLLABLE);
    rctx->screen = s_screen;

    /* Semi-transparent stop button in top-right
     * 放大按钮到 60x36 方便手指点；hit-area 再额外往外扩 8px */
    lv_obj_t *btn = lv_btn_create(s_screen);
    lv_obj_set_size(btn, 60, 36);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_bg_color(btn, lv_color_make(0xcc, 0x33, 0x33), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
    lv_obj_set_ext_click_area(btn, 8);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_STOP);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, btn_stop_cb, LV_EVENT_CLICKED, NULL);

    /* Semi-transparent progress bar near bottom */
    s_progress_bar = lv_bar_create(s_screen);
    lv_obj_set_size(s_progress_bar, lv_pct(88), 5);
    lv_obj_align(s_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_make(0x66, 0x66, 0x66), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_90, LV_PART_INDICATOR);

    /* Time label below progress bar */
    s_time_label = lv_label_create(s_screen);
    lv_label_set_text(s_time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_time_label, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* 确保 overlay 控件 z-order 在视频画面之上（视频帧会通过
     * lv_img_set_src 频繁重绘，如果 overlay 不在前景，可能被覆盖或
     * 无法接收触摸事件） */
    lv_obj_move_foreground(btn);
    lv_obj_move_foreground(s_progress_bar);
    lv_obj_move_foreground(s_time_label);

    lv_scr_load(s_screen);

    /* Start playback */
    playback_type_t type = is_url ? PLAYBACK_TYPE_HTTP_URL : PLAYBACK_TYPE_LOCAL_FILE;
    int ret = playback_start(type, path_or_url,
                              ctx->display_dev, ctx->audio_dev,
                              &s_lvgl_video_ops, NULL);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to start playback");
        /* Return to previous screen */
        app_switch_screen(ctx->previous_screen);
        return;
    }

    /* Poll for natural playback completion */
    s_poll_timer = lv_timer_create(playback_poll_timer_cb, 500, NULL);
}

void ui_playback_destroy(void)
{
    if (s_poll_timer) {
        lv_timer_del(s_poll_timer);
        s_poll_timer = NULL;
    }

    /* Stop playback if still active */
    if (playback_is_active()) {
        playback_stop();
    }

    app_context_t *ctx = app_ctx_get();
    avi_lvgl_render_ctx_t *rctx = &ctx->render_ctx;
    rctx->img = NULL;
    rctx->screen = NULL;

    s_progress_bar = NULL;
    s_time_label = NULL;

    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}
