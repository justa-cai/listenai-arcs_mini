#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lvgl.h"
#include "lv_port_disp.h"

#include "lisa_log.h"
#include "lisa_display.h"

#include "board.h"

#include "lisa_ui_invoke.h"

#include "lisa_kv.h"
#include "kv.h"

#ifdef CONFIG_LV_CUSTOM_TASK_CPU_PER
extern void lvgl_task_cpu_percent_peroid500ms(void);
#endif
// #include "app_display.h"

#define TAG "view_main"

#define LISA_KV_KEY_LANGUAGE "user.locale"

static bool s_lvgl_handler_suspended;
static bool s_lvgl_handler_pending;

int lisa_ui_lvgl_init()
{
    lv_init();

    char *locale = NULL;

    int r = lisa_kv_get_string(LISA_KV_KEY_LANGUAGE, &locale);
    if (r || locale == NULL) {
        lv_i18n_set_locale("zh-CN");
        LOGW("locale not found, use zn-CN");
    } else {
        LOGI("locale %s found", locale);
        lv_i18n_set_locale(locale);
        lisa_kv_free(locale);
    }

    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lv_port_disp_init(display_device);

    lisa_display_blanking_on(display_device);

    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_scr_load(scr);
    lv_task_handler();
    lisa_display_blanking_off(display_device);

    return 0;
}

static void lisa_ui_invoke_worker(void *arg, uint32_t len)
{
    uint32_t sleep;

    s_lvgl_handler_pending = false;
    if (s_lvgl_handler_suspended) {
        return;
    }

    sleep = lv_task_handler();
    /* LVGL may return LV_NO_TIMER_READY after a static screen is drawn.
     * UI work can invalidate that screen or create timers before the next
     * handler call, so never schedule the handler indefinitely far away.
     */
    if (sleep > LV_DISP_DEF_REFR_PERIOD) {
        sleep = LV_DISP_DEF_REFR_PERIOD;
    }
    if (!s_lvgl_handler_suspended) {
        s_lvgl_handler_pending = true;
        LISA_UI_INVOKE_UI_DELAYED(lisa_ui_invoke_worker, arg, len, sleep);
    }
}

int lisa_ui_lvgl_run(void)
{
    s_lvgl_handler_pending = true;
    LISA_UI_INVOKE_UI_DELAYED(lisa_ui_invoke_worker, NULL, 0, 0);

    return 0;
}

void lisa_ui_handler_suspend(void)
{
    s_lvgl_handler_suspended = true;
    LOGI("LVGL handler suspended");
}

void lisa_ui_handler_resume(void)
{
    if (!s_lvgl_handler_suspended) {
        return;
    }

    s_lvgl_handler_suspended = false;
    if (!s_lvgl_handler_pending) {
        s_lvgl_handler_pending = true;
        LISA_UI_INVOKE_UI_DELAYED(lisa_ui_invoke_worker, NULL, 0, 0);
    }
    LOGI("LVGL handler resumed");
}
