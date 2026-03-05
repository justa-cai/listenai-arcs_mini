/**
 * @file alarm_ring_presenter.c
 * @brief Alarm ring page presenter implementation
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lisa_ui_invoke.h"

#define TAG "alarm_ring_presenter"

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "alarm_ring_view.h"
#include "model_alarm.h"
#include "model_voice.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "voice_msg.h"
#endif

#ifndef CONFIG_ALARM_RING_AUTO_RETURN_MS
#define CONFIG_ALARM_RING_AUTO_RETURN_MS 60000
#endif
#define ALARM_RING_AUTO_RETURN_MS CONFIG_ALARM_RING_AUTO_RETURN_MS

struct alarm_ring_nav_scr_data {
    lv_obj_t *view;
    lv_timer_t *auto_return;
    bool iat_has_valid_text;
};

#ifdef LISA_UI_PLATFORM_ARCS
static void alarm_ring_auto_return_restart(struct alarm_ring_nav_scr_data *scr_data, const char *reason)
{
    if (!scr_data || !scr_data->auto_return) {
        return;
    }

    lv_timer_reset(scr_data->auto_return);
    lv_timer_resume(scr_data->auto_return);
    LISA_UI_LOGI("Alarm ring: auto return timer reset (%s)", reason ? reason : "unknown");
}

static bool alarm_iat_text_is_valid(const char *text, uint32_t len)
{
    if (!text || len == 0) {
        return false;
    }

    uint32_t n = strnlen(text, len);
    for (uint32_t i = 0; i < n; i++) {
        char c = text[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return true;
        }
    }

    return false;
}

static void alarm_ring_nav_home_ui(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;

    LISA_UI_LOGI("Alarm ring: nav_home_ui invoked, top_id=%d", lisa_ui_nav_scr_get_top_id());
    if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_ALARM_RING) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }
}

static void alarm_ring_on_wakeup(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_LOGI("Alarm ring: wakeup event, navigating to home");
    lisa_ui_invoke_ui_delayed(alarm_ring_nav_home_ui, NULL, 0, 0);
}

static void alarm_ring_on_button_change(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_LOGI("Alarm ring: button change, navigating to home");
    lisa_ui_invoke_ui_delayed(alarm_ring_nav_home_ui, NULL, 0, 0);
}

static void alarm_ring_on_iat(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;

    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)user_data;
    if (!scr_data) {
        return;
    }

    if (msg_id == VOICE_MSG_CLOUD_IAT_START) {
        scr_data->iat_has_valid_text = false;
        return;
    }

    if (msg_id == VOICE_MSG_CLOUD_IAT_UPDATE) {
        if (alarm_iat_text_is_valid((const char *)data, len)) {
            scr_data->iat_has_valid_text = true;
            LISA_UI_LOGI("Alarm ring: iat valid text, navigating to home (len=%u)", (unsigned int)len);
            lisa_ui_invoke_ui_delayed(alarm_ring_nav_home_ui, NULL, 0, 0);
        }
        return;
    }

    if (msg_id == VOICE_MSG_CLOUD_IAT_END) {
        scr_data->iat_has_valid_text = false;
    }
}

static void alarm_ring_on_alarm_trigger(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;

    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)user_data;
    if (!scr_data) {
        return;
    }

    LISA_UI_INVOKE_UI_ARG_BASE(scr_data, {
        alarm_ring_auto_return_restart(_invoke_scr_data, "alarm_trigger");
    });
}
#endif

static void format_alarm_time(const alarm_time_info_t *info, char *time_str, size_t time_size, 
                              char *date_str, size_t date_size)
{
    snprintf(time_str, time_size, "%02d:%02d", info->hour, info->minute);
    
    const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    const char *suffix = "";
    if (info->is_today) {
        suffix = "(今天)";
    } else if (info->is_tomorrow) {
        suffix = "(明天)";
    }
    
    snprintf(date_str, date_size, "%d月%d日 %s%s", 
            info->month, info->day, weekdays[info->weekday], suffix);
}

static void stop_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        LISA_UI_LOGI("Alarm ring: stop button clicked, navigating to home");
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }
}

static void alarm_ring_auto_return_cb(lv_timer_t *timer)
{
    (void)timer;
    LISA_UI_LOGI("Alarm ring: auto return timer, navigating to home");
    lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
}

static int alarm_ring_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("Alarm ring nav scr open, id: %d", scr->unique_id);
    
    struct alarm_ring_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct alarm_ring_nav_scr_data));
    if (!scr_data) {
        LISA_UI_LOGE("Failed to allocate memory");
        return -1;
    }

    memset(scr_data, 0, sizeof(struct alarm_ring_nav_scr_data));
    
    scr_data->view = alarm_ring_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create alarm ring view");
        lisa_ui_free(scr_data);
        return -1;
    }
    
    alarm_data_t alarm_data;
    if (model_alarm_get_data(&alarm_data) == 0) {
        alarm_time_info_t time_info;
        if (model_alarm_get_time_info(alarm_data.timestamp, &time_info) == 0) {
            char time_str[16];
            char date_str[32];
            format_alarm_time(&time_info, time_str, sizeof(time_str), date_str, sizeof(date_str));
            
            alarm_ring_view_set_time(scr_data->view, time_str);
            alarm_ring_view_set_date(scr_data->view, date_str);
            
            LISA_UI_LOGI("Alarm ring page - time: %s, date: %s", time_str, date_str);
        } else {
            LISA_UI_LOGW("Failed to get time info");
        }
    } else {
        LISA_UI_LOGW("No alarm data available, using default");
    }
    
    alarm_ring_view_set_stop_cb(scr_data->view, stop_btn_event_cb, NULL);

    scr_data->auto_return = lv_timer_create(alarm_ring_auto_return_cb, ALARM_RING_AUTO_RETURN_MS, NULL);
    if (!scr_data->auto_return) {
        LISA_UI_LOGE("Failed to create auto return timer");
        lv_obj_del(scr_data->view);
        lisa_ui_free(scr_data);
        return -1;
    }
    lv_timer_set_repeat_count(scr_data->auto_return, 1);

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_sub(VOICE_MSG_ALARM_TRIGGER, alarm_ring_on_alarm_trigger, scr_data);
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, alarm_ring_on_wakeup, scr_data);
    voice_msg_sub(VOICE_MSG_WAKEUP_COMMAND, alarm_ring_on_wakeup, scr_data);
    voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, alarm_ring_on_button_change, scr_data);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_START, alarm_ring_on_iat, scr_data);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, alarm_ring_on_iat, scr_data);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_END, alarm_ring_on_iat, scr_data);
#endif
    
    *data = scr_data;
    return 0;
}

static int alarm_ring_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm ring nav scr show, id: %d", scr->unique_id);
    
    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("Invalid alarm ring nav scr data");
        return -1;
    }
    
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int alarm_ring_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm ring nav scr pause, id: %d", scr->unique_id);
    
    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }
    
    return 0;
}

static int alarm_ring_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm ring nav scr resume, id: %d", scr->unique_id);
    
    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }
    
    return 0;
}

static int alarm_ring_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm ring nav scr close, id: %d", scr->unique_id);
    
    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)data;
    if (scr_data) {
#ifdef LISA_UI_PLATFORM_ARCS
        voice_msg_unsub(VOICE_MSG_WAKEUP_KEYWORD, alarm_ring_on_wakeup);
        voice_msg_unsub(VOICE_MSG_WAKEUP_COMMAND, alarm_ring_on_wakeup);
        voice_msg_unsub(VOICE_MSG_BUTTON_CHANGE, alarm_ring_on_button_change);
        voice_msg_unsub(VOICE_MSG_CLOUD_IAT_START, alarm_ring_on_iat);
        voice_msg_unsub(VOICE_MSG_CLOUD_IAT_UPDATE, alarm_ring_on_iat);
        voice_msg_unsub(VOICE_MSG_CLOUD_IAT_END, alarm_ring_on_iat);
        voice_msg_unsub(VOICE_MSG_ALARM_TRIGGER, alarm_ring_on_alarm_trigger);
#endif

        if (scr_data->auto_return) {
            lv_timer_del(scr_data->auto_return);
            scr_data->auto_return = NULL;
        }

        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }
        
        lisa_ui_free(scr_data);
    }
    
    return 0;
}

const struct lisa_ui_nav_scr alarm_ring_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_ALARM_RING,
    .open = alarm_ring_nav_scr_open,
    .show = alarm_ring_nav_scr_show,
    .pause = alarm_ring_nav_scr_pause,
    .resume = alarm_ring_nav_scr_resume,
    .close = alarm_ring_nav_scr_close,
};
