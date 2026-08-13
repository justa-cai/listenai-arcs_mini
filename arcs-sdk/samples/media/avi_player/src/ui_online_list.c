/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui_online_list.h"
#include "app_context.h"
#include "ui_playback.h"
#include "fs_utils.h"
#include "wifi_utils.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_thread.h"

#include <string.h>
#include <ctype.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ui_online_list"
#include <lisa_log.h>

static lv_obj_t *s_screen;
static lv_obj_t *s_status_label;
static lv_timer_t *s_wifi_timer;
static fs_url_list_t s_url_list;
static int s_selected_idx = -1;
static volatile bool s_wifi_connecting;
static volatile bool s_wifi_connect_done;
static volatile bool s_wifi_connect_result;

static bool url_ends_with_avi(const char *url)
{
    size_t len = strlen(url);
    if (len < 4) {
        return false;
    }
    const char *ext = url + len - 4;
    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 'a' &&
            tolower((unsigned char)ext[2]) == 'v' &&
            tolower((unsigned char)ext[3]) == 'i');
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_wifi_timer) {
        lv_timer_del(s_wifi_timer);
        s_wifi_timer = NULL;
    }
    app_switch_screen(SCREEN_MAIN_MENU);
}

static void wifi_connect_task_fn(void *param)
{
    (void)param;
    fs_wifi_config_t wifi_cfg;
    int ret = fs_read_wifi_config(WIFI_CONFIG_PATH, &wifi_cfg);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to read WiFi config");
        s_wifi_connect_result = false;
        s_wifi_connect_done = true;
        vTaskDelete(NULL);
        return;
    }

    ret = wifi_utils_connect(wifi_cfg.ssid, wifi_cfg.pwd, 30);
    s_wifi_connect_result = (ret == 0);
    s_wifi_connect_done = true;
    vTaskDelete(NULL);
}

static void wifi_poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_wifi_connect_done) {
        return;
    }

    if (s_wifi_timer) {
        lv_timer_del(s_wifi_timer);
        s_wifi_timer = NULL;
    }
    s_wifi_connecting = false;

    if (s_wifi_connect_result && s_selected_idx >= 0 && s_selected_idx < s_url_list.count) {
        LISA_LOGI(LOG_TAG, "WiFi connected, starting playback");
        app_ctx_get()->previous_screen = SCREEN_ONLINE_LIST;
        app_ctx_get()->current_screen = SCREEN_PLAYBACK;
        ui_playback_create(s_url_list.entries[s_selected_idx].url, true);
    } else {
        LISA_LOGE(LOG_TAG, "WiFi connection failed");
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi failed!");
            lv_obj_set_style_text_color(s_status_label, lv_color_make(0xff, 0x40, 0x40), LV_PART_MAIN);
            lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void url_item_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_url_list.count) {
        return;
    }

    const char *url = s_url_list.entries[idx].url;

    /* Only .avi URLs are supported */
    if (!url_ends_with_avi(url)) {
        LISA_LOGW(LOG_TAG, "Unsupported format: %s", url);
        if (s_status_label) {
            lv_label_set_text(s_status_label, "Only .avi supported");
            lv_obj_set_style_text_color(s_status_label, lv_color_make(0xff, 0x40, 0x40), LV_PART_MAIN);
            lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    /* Already connected? Play directly */
    if (wifi_utils_is_connected()) {
        LISA_LOGI(LOG_TAG, "WiFi ready, playing: %s", url);
        app_ctx_get()->previous_screen = SCREEN_ONLINE_LIST;
        app_ctx_get()->current_screen = SCREEN_PLAYBACK;
        ui_playback_create(url, true);
        return;
    }

    if (s_wifi_connecting) {
        return;
    }

    s_selected_idx = idx;
    s_wifi_connecting = true;
    s_wifi_connect_done = false;

    if (s_status_label) {
        lv_label_set_text(s_status_label, "Connecting WiFi...");
        lv_obj_set_style_text_color(s_status_label, lv_color_make(0xff, 0xcc, 0x00), LV_PART_MAIN);
        lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
    }

    xTaskCreate(wifi_connect_task_fn, "wifi_conn", 2 * 1024, NULL,
                configMAX_PRIORITIES - 3, NULL);
    s_wifi_timer = lv_timer_create(wifi_poll_timer_cb, 500, NULL);
}

void ui_online_list_create(void)
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
    lv_label_set_text(title, LV_SYMBOL_WIFI " Online Videos");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* WiFi status label */
    s_status_label = lv_label_create(top_bar);
    lv_label_set_text(s_status_label, "");
    lv_obj_align(s_status_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_add_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);

    /* Scan URLs */
    int ret = fs_list_urls(URL_DIR, &s_url_list);

    /* URL list —— 占满 top_bar 下方所有空间 */
    lv_obj_t *list = lv_list_create(s_screen);
    lv_obj_set_size(list, LV_PCT(100), LV_VER_RES - 32);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_make(0x22, 0x22, 0x3a), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);

    if (ret != 0 || s_url_list.count == 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "No URLs in /URL/\nPut .ini files via USB MSC");
        lv_obj_set_style_text_color(empty, lv_color_make(0x88, 0x88, 0x88), LV_PART_MAIN);
    } else {
        for (int i = 0; i < s_url_list.count; i++) {
            lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, s_url_list.entries[i].label);
            lv_obj_add_event_cb(btn, url_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
    }

    lv_scr_load(s_screen);
    LISA_LOGI(LOG_TAG, "Online list: %d URLs", s_url_list.count);
}

void ui_online_list_destroy(void)
{
    if (s_wifi_timer) {
        lv_timer_del(s_wifi_timer);
        s_wifi_timer = NULL;
    }
    s_wifi_connecting = false;
    s_status_label = NULL;

    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}
