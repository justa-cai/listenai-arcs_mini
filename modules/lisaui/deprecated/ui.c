/**
 * @file ui.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-24
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui.h"
#include "app/app_common.h"
#include "app_common/lisaui_log.h"
#include "lisaui_app_common.h"
#include "lisaui_app_manager.h"

#if CONFIG_LVGL_ENV_SIMULATOR
#include <pthread.h>
#include <stdbool.h>
#else
#include "FreeRTOS.h"
#include "semphr.h"
#endif
static const char *TAG = "UI";

#if CONFIG_LVGL_ENV_SIMULATOR
pthread_mutex_t lvgl_mutex;
#else
SemaphoreHandle_t lvgl_mutex;
#endif


void lisaui_ui_init(void)
{
#if CONFIG_LVGL_ENV_SIMULATOR
    if (pthread_mutex_init(&lvgl_mutex, NULL) != 0) {
        LISAUI_LOGE(TAG, "[%s] Failed to create lvgl_mutex", __FUNCTION__);
        return;
    }
#else
    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == NULL) {
        LISAUI_LOGE(TAG, "[%s] Failed to create lvgl_mutex", __FUNCTION__);
        return;
    }
#endif

#if 0
    lv_disp_t *dispp = lv_disp_get_default();

    lv_theme_t *theme = LV_THEME_DEFAULT_INIT(
        lv_theme_get_color_primary(), lv_theme_get_color_secondary(), LV_THEME_MATERIAL_FLAG_DARK,
        lv_theme_get_font_small(), lv_theme_get_font_normal(), lv_theme_get_font_subtitle(), lv_theme_get_font_title());
    lv_theme_set_act(theme);
#endif

#if CONFIG_LVGL_ENV_SIMULATOR
    LISAUI_USE_APP(standby);
    LISAUI_USE_APP(launcher);
    LISAUI_USE_APP(taskbar);
    LISAUI_USE_APP(setting);

    LISAUI_USE_APP(weather);
    LISAUI_USE_APP(template);
    LISAUI_USE_APP(alarm);
    LISAUI_USE_APP(audio_player);
    LISAUI_USE_APP(dictionary);
    LISAUI_USE_APP(wifi);

#endif
    lisaui_app_manager_init();
    // lisaui_app_manager_show_all_app_info();

    LISAUI_LOGI(TAG, "UI init done");

}