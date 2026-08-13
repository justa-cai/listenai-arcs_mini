/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include "lisa_device.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "lvgl.h"

#define SDMMC_DEVICE_NAME   "sdmmc0"
#define SDMMC_DEVICE        "SD:"
#define SDMMC_MOUNT_POINT   "/" SDMMC_DEVICE
#define AUDIO_DEVICE_NAME   "audio0"

#define VIDEO_DIR           SDMMC_MOUNT_POINT "/Video"
#define URL_DIR             SDMMC_MOUNT_POINT "/URL"
#define WIFI_CONFIG_PATH    SDMMC_MOUNT_POINT "/WiFi/wifi.ini"

typedef enum {
    SCREEN_MAIN_MENU,
    SCREEN_USB_MSC,
    SCREEN_LOCAL_LIST,
    SCREEN_ONLINE_LIST,
    SCREEN_PLAYBACK,
} screen_id_t;

/* LVGL video render context for AVI playback */
typedef struct {
    SemaphoreHandle_t lock;
    lv_obj_t *screen;
    lv_obj_t *img;
    lv_img_dsc_t img_dsc;
    uint8_t *frame_buf;
    size_t frame_buf_size;
    uint16_t frame_w;
    uint16_t frame_h;
    bool frame_dirty;
    bool first_frame_arrived;
} avi_lvgl_render_ctx_t;

typedef struct {
    lisa_device_t *display_dev;
    lisa_device_t *audio_dev;
    lisa_device_t *touch_dev;
    SemaphoreHandle_t lv_mutex;
    screen_id_t current_screen;
    screen_id_t previous_screen;
    bool ui_ready;
    /* Video render context shared with playback */
    avi_lvgl_render_ctx_t render_ctx;
} app_context_t;

/* Global app context */
app_context_t *app_ctx_get(void);
void app_ctx_init(lisa_device_t *display_dev, lisa_device_t *audio_dev);

/* Screen navigation */
void app_switch_screen(screen_id_t id);

/* LVGL UI task entry (run as FreeRTOS task) */
void app_ui_task(void *param);

/* Screen create/destroy (implemented by each ui_*.c) */
void ui_main_menu_create(void);
void ui_main_menu_destroy(void);
void ui_usb_msc_create(void);
void ui_usb_msc_destroy(void);
void ui_local_list_create(void);
void ui_local_list_destroy(void);
void ui_online_list_create(void);
void ui_online_list_destroy(void);
void ui_playback_create(const char *path_or_url, bool is_url);
void ui_playback_destroy(void);

/* USB MSC functions (defined in main.c, called from ui_usb_msc.c) */
void user_usbd_msc_init(void);
void usb_device_task(void *param);

#endif /* APP_CONTEXT_H */
