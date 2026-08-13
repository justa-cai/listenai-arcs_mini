/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui_usb_msc.h"
#include "app_context.h"
#include "lvgl.h"
#include "tusb.h"
#include "lsfs.h"
#include "FreeRTOS.h"
#include "task.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ui_usb_msc"
#include <lisa_log.h>

static lv_obj_t *s_screen;
static bool s_usb_started;
static TaskHandle_t s_usb_task_handle;

/* Defined in main.c */
extern struct lsfs_mount_t sdmmc_mnt;
extern void user_usbd_msc_init(void);
extern void usb_device_task(void *param);

static void usb_msc_start(void)
{
    if (s_usb_started) {
        return;
    }

    /* Unmount filesystem so USB host has exclusive disk access */
    lsfs_unmount(&sdmmc_mnt);
    LISA_LOGI(LOG_TAG, "Filesystem unmounted");

    /* Init and start USB MSC */
    user_usbd_msc_init();
    if (xTaskCreate(usb_device_task, "usbd", 3 * configMINIMAL_STACK_SIZE,
                    NULL, configMAX_PRIORITIES - 1, &s_usb_task_handle) == pdPASS) {
        s_usb_started = true;
        LISA_LOGI(LOG_TAG, "USB MSC started");
    }
}

static void usb_msc_stop(void)
{
    if (!s_usb_started) {
        return;
    }

    /* Disconnect USB */
    tud_disconnect();
    LISA_LOGI(LOG_TAG, "USB disconnected");

    /* Re-mount filesystem */
    int ret = lsfs_mount(&sdmmc_mnt);
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Remount failed: %d", ret);
    } else {
        LISA_LOGI(LOG_TAG, "Filesystem remounted");
    }

    s_usb_started = false;
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    usb_msc_stop();
    app_switch_screen(SCREEN_MAIN_MENU);
}

void ui_usb_msc_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_make(0x1a, 0x1a, 0x2e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);

    /* USB icon / title */
    lv_obj_t *icon = lv_label_create(s_screen);
    lv_label_set_text(icon, LV_SYMBOL_USB "  USB MSC Active");
    lv_obj_set_style_text_color(icon, lv_color_make(0x3a, 0x7b, 0xd5), LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 8);

    /* Status text —— 320x240 空间紧张，精简为关键信息 */
    lv_obj_t *status = lv_label_create(s_screen);
    lv_label_set_text(status,
        "Copy files to SD:\n"
        "  /Video/ - AVI files\n"
        "  /URL/   - URL .ini\n"
        "  /WiFi/  - wifi.ini\n"
        "\n"
        "Eject before Back");
    lv_obj_set_style_text_color(status, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 10, 36);

    /* Back button */
    lv_obj_t *btn = lv_btn_create(s_screen);
    lv_obj_set_size(btn, 72, 30);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
    lv_obj_set_style_bg_color(btn, lv_color_make(0x80, 0x30, 0x30), 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, btn_back_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(s_screen);

    /* Start USB MSC after screen is loaded */
    usb_msc_start();
}

void ui_usb_msc_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}
