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
#include "service_alarm.h"
#endif

#ifndef CONFIG_ALARM_RING_AUTO_RETURN_MS
#define CONFIG_ALARM_RING_AUTO_RETURN_MS 60000
#endif
#define ALARM_RING_AUTO_RETURN_MS CONFIG_ALARM_RING_AUTO_RETURN_MS

struct alarm_ring_nav_scr_data {
    lv_obj_t *view;
    lv_timer_t *auto_return;
    bool iat_has_valid_text;
    bool snooze_enabled;            // 是否开启了 snooze
    uint8_t snooze_remaining_count; // 剩余提醒次数
};

struct alarm_ring_toast_ui_data {
    struct alarm_ring_nav_scr_data *scr_data;
    char text[64];
};

#ifdef LISA_UI_PLATFORM_ARCS
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

static void format_alarm_create_toast(const alarm_time_info_t *info, char *toast_str, size_t toast_size)
{
    if (!info || !toast_str || toast_size == 0) {
        return;
    }

    if (info->is_today) {
        snprintf(toast_str, toast_size, "已添加今天 %02d:%02d 的闹钟", info->hour, info->minute);
        return;
    }

    if (info->is_tomorrow) {
        snprintf(toast_str, toast_size, "已添加明天 %02d:%02d 的闹钟", info->hour, info->minute);
        return;
    }

    snprintf(toast_str, toast_size, "已添加%d月%d日%02d:%02d的闹钟",
             info->month, info->day, info->hour, info->minute);
}

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

static void alarm_ring_on_alarm_stopped(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_LOGI("Alarm ring: alarm stopped, navigating to home");
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

static void alarm_ring_on_alarm_create(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)len;

    struct alarm_ring_nav_scr_data *scr_data = (struct alarm_ring_nav_scr_data *)user_data;
    struct service_alarm *alarm = (struct service_alarm *)data;
    if (!scr_data || !alarm) {
        return;
    }

    alarm_time_info_t time_info;
    if (model_alarm_get_time_info(alarm->timestamp, &time_info) != 0) {
        LISA_UI_LOGW("Alarm ring: failed to get created alarm time info");
        return;
    }

    struct alarm_ring_toast_ui_data ui_data = {
        .scr_data = scr_data,
    };
    format_alarm_create_toast(&time_info, ui_data.text, sizeof(ui_data.text));

    LISA_UI_INVOKE_UI_ARG_BASE(ui_data, {
        if (!_invoke_ui_data.scr_data || !_invoke_ui_data.scr_data->view) {
            return;
        }

        if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_ALARM_RING) {
            return;
        }

        alarm_ring_view_show_toast(_invoke_ui_data.scr_data->view, _invoke_ui_data.text);
        LISA_UI_LOGI("Alarm ring: show create toast: %s", _invoke_ui_data.text);
    });
}

static void update_alarm_ring_ui(struct alarm_ring_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
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
            alarm_ring_view_set_text(scr_data->view, alarm_data.text);

            // 获取 snooze 剩余次数并缓存到 scr_data
            int remaining = model_alarm_get_snooze_remaining_count();
            if (remaining > 0) {
                // 有snooze且还有剩余次数，显示"稍后提醒"hint
                scr_data->snooze_enabled = true;
                scr_data->snooze_remaining_count = (uint8_t)remaining;
                alarm_ring_view_set_hint(scr_data->view, "单击: 稍后提醒\n长按: 关闭闹钟");
                LISA_UI_LOGI("Alarm ring UI - snooze enabled, remaining %d times", remaining);
            } else {
                // 没有snooze或已达到最大次数，显示"关闭闹钟"hint
                scr_data->snooze_enabled = false;
                scr_data->snooze_remaining_count = 0;
                alarm_ring_view_set_hint(scr_data->view, "单击: 关闭闹钟");
                LISA_UI_LOGI("Alarm ring UI - snooze disabled or last snooze");
            }

            LISA_UI_LOGI("Alarm ring UI - time: %s, date: %s, text: %s", time_str, date_str, alarm_data.text);
        } else {
            LISA_UI_LOGW("Failed to get time info");
        }
    } else {
        LISA_UI_LOGW("No alarm data available");
    }
}

static void alarm_ring_on_ui_update(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
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
        LISA_UI_LOGI("Alarm ring: received UI update message");
        // g_alarm_ctx.data 已在 alarm_trigger_nav_ui_worker 中更新
        // 现在更新UI显示
        update_alarm_ring_ui(_invoke_scr_data);
    });
}
#endif

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
    struct alarm_ring_nav_scr_data *scr_data = NULL;

    if (timer) {
        scr_data = (struct alarm_ring_nav_scr_data *)timer->user_data;
        if (scr_data) {
            scr_data->auto_return = NULL;
        }
    }

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

    // 初始化UI显示闹钟信息
    update_alarm_ring_ui(scr_data);

    alarm_ring_view_set_stop_cb(scr_data->view, stop_btn_event_cb, NULL);

    scr_data->auto_return =
        lv_timer_create(alarm_ring_auto_return_cb, ALARM_RING_AUTO_RETURN_MS, scr_data);
    if (!scr_data->auto_return) {
        LISA_UI_LOGE("Failed to create auto return timer");
        lv_obj_del(scr_data->view);
        lisa_ui_free(scr_data);
        return -1;
    }
    lv_timer_set_repeat_count(scr_data->auto_return, 1);

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_sub(VOICE_MSG_ALARM_TRIGGER, alarm_ring_on_alarm_trigger, scr_data);
    voice_msg_sub(VOICE_MSG_ALARM_CREATE, alarm_ring_on_alarm_create, scr_data);
    voice_msg_sub(VOICE_MSG_ALARM_RING_UPDATE, alarm_ring_on_ui_update, scr_data);
    voice_msg_sub(VOICE_MSG_ALARM_STOPPED, alarm_ring_on_alarm_stopped, scr_data);
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, alarm_ring_on_wakeup, scr_data);
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
        voice_msg_unsub(VOICE_MSG_CLOUD_IAT_START, alarm_ring_on_iat);
        voice_msg_unsub(VOICE_MSG_CLOUD_IAT_UPDATE, alarm_ring_on_iat);
        voice_msg_unsub(VOICE_MSG_CLOUD_IAT_END, alarm_ring_on_iat);
        voice_msg_unsub(VOICE_MSG_ALARM_TRIGGER, alarm_ring_on_alarm_trigger);
        voice_msg_unsub(VOICE_MSG_ALARM_CREATE, alarm_ring_on_alarm_create);
        voice_msg_unsub(VOICE_MSG_ALARM_RING_UPDATE, alarm_ring_on_ui_update);
        voice_msg_unsub(VOICE_MSG_ALARM_STOPPED, alarm_ring_on_alarm_stopped);
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
