/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui_local_list.h"
#include "app_context.h"
#include "ui_playback.h"
#include "fs_utils.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ui_local_list"
#include <lisa_log.h>

static lv_obj_t *s_screen;
static fs_file_list_t s_file_list;

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    app_switch_screen(SCREEN_MAIN_MENU);
}

static void file_item_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_file_list.count) {
        return;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", VIDEO_DIR, s_file_list.entries[idx].name);
    LISA_LOGI(LOG_TAG, "Selected: %s", path);

    app_ctx_get()->previous_screen = SCREEN_LOCAL_LIST;
    app_ctx_get()->current_screen = SCREEN_PLAYBACK;

    /* Create playback screen (it calls lv_scr_load internally) */
    ui_playback_create(path, false);
}

void ui_local_list_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_make(0x1a, 0x1a, 0x2e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);

    /* Top bar (height 32) */
    lv_obj_t *top_bar = lv_obj_create(s_screen);
    lv_obj_remove_style_all(top_bar);
    lv_obj_set_size(top_bar, LV_PCT(100), 32);
    lv_obj_align(top_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Back button */
    lv_obj_t *btn_back = lv_btn_create(top_bar);
    lv_obj_set_size(btn_back, 64, 26);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_make(0x80, 0x30, 0x30), 0);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    /* Title */
    lv_obj_t *title = lv_label_create(top_bar);
    lv_label_set_text(title, LV_SYMBOL_VIDEO " Local Videos");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* Scan files */
    int ret = fs_list_avi_files(VIDEO_DIR, &s_file_list);

    /* File list —— 占满 top_bar 下方所有空间 */
    lv_obj_t *list = lv_list_create(s_screen);
    lv_obj_set_size(list, LV_PCT(100), LV_VER_RES - 32);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_make(0x22, 0x22, 0x3a), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);

    if (ret != 0 || s_file_list.count == 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "No AVI files in /Video/");
        lv_obj_set_style_text_color(empty, lv_color_make(0x88, 0x88, 0x88), LV_PART_MAIN);
    } else {
        for (int i = 0; i < s_file_list.count; i++) {
            char item_text[80];
            uint32_t size_kb = s_file_list.entries[i].size / 1024;
            snprintf(item_text, sizeof(item_text), "%s  (%u KB)",
                     s_file_list.entries[i].name, size_kb);

            lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_VIDEO, item_text);
            lv_obj_add_event_cb(btn, file_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
    }

    lv_scr_load(s_screen);
    LISA_LOGI(LOG_TAG, "Local list: %d files", s_file_list.count);
}

void ui_local_list_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}
