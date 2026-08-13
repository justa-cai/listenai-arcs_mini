/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_context.h"
#include <string.h>
#include "ui_main_menu.h"
#include "ui_usb_msc.h"
#include "ui_local_list.h"
#include "ui_online_list.h"
#include "ui_playback.h"
#include "lisa_thread.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "app_context"
#include <lisa_log.h>

static app_context_t s_app_ctx;

app_context_t *app_ctx_get(void)
{
    return &s_app_ctx;
}

void app_ctx_init(lisa_device_t *display_dev, lisa_device_t *audio_dev)
{
    memset(&s_app_ctx, 0, sizeof(s_app_ctx));
    s_app_ctx.display_dev = display_dev;
    s_app_ctx.audio_dev = audio_dev;
    s_app_ctx.lv_mutex = xSemaphoreCreateMutex();
    s_app_ctx.current_screen = SCREEN_MAIN_MENU;
    s_app_ctx.previous_screen = SCREEN_MAIN_MENU;
}

static void destroy_current_screen(void)
{
    switch (s_app_ctx.current_screen) {
    case SCREEN_MAIN_MENU:
        ui_main_menu_destroy();
        break;
    case SCREEN_USB_MSC:
        ui_usb_msc_destroy();
        break;
    case SCREEN_LOCAL_LIST:
        ui_local_list_destroy();
        break;
    case SCREEN_ONLINE_LIST:
        ui_online_list_destroy();
        break;
    case SCREEN_PLAYBACK:
        ui_playback_destroy();
        break;
    }
}

static void create_screen(screen_id_t id)
{
    switch (id) {
    case SCREEN_MAIN_MENU:
        ui_main_menu_create();
        break;
    case SCREEN_USB_MSC:
        ui_usb_msc_create();
        break;
    case SCREEN_LOCAL_LIST:
        ui_local_list_create();
        break;
    case SCREEN_ONLINE_LIST:
        ui_online_list_create();
        break;
    case SCREEN_PLAYBACK:
        /* Playback screen is created via ui_playback_create(path, is_url) directly */
        break;
    }
}

void app_switch_screen(screen_id_t id)
{
    LISA_LOGI(LOG_TAG, "Switching screen: %d -> %d", s_app_ctx.current_screen, id);

    if (id != SCREEN_PLAYBACK) {
        s_app_ctx.previous_screen = s_app_ctx.current_screen;
    }

    /*
     * IMPORTANT: create the new screen FIRST (which calls lv_scr_load),
     * then destroy the old screen. This avoids deleting the screen
     * that the current button event callback is running on.
     */
    screen_id_t old_screen = s_app_ctx.current_screen;
    s_app_ctx.current_screen = id;
    create_screen(id);

    /* Now safe to destroy old screen (it's no longer the active screen) */
    switch (old_screen) {
    case SCREEN_MAIN_MENU:   ui_main_menu_destroy(); break;
    case SCREEN_USB_MSC:     ui_usb_msc_destroy(); break;
    case SCREEN_LOCAL_LIST:  ui_local_list_destroy(); break;
    case SCREEN_ONLINE_LIST: ui_online_list_destroy(); break;
    case SCREEN_PLAYBACK:    ui_playback_destroy(); break;
    }
}

/* Note: app_ui_task is no longer used.
 * The UI task (task_ui) is defined in main.c following the lvgl8_widgets sample pattern:
 * - All LVGL init happens in main()
 * - task_ui just builds UI and runs lv_task_handler() loop
 */
