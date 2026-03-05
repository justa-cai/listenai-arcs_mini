/**
 * @file model_alarm.c
 * @brief Alarm data model implementation
 */

#include "model_alarm.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr_ids.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "voice_msg.h"
#include "service_alarm.h"
#include "app_datas.h"

#define ALARM_TRIGGER_NAV_DELAY_MS 200
#endif

#define TAG "model_alarm"

/**
 * @brief Alarm model context
 */
typedef struct {
    alarm_data_t data;
    char event_text[128];
    bool initialized;
    bool is_ringing; /* 闹钟是否正在响铃 */
    uint32_t alarm_count;
} alarm_model_ctx_t;

static alarm_model_ctx_t g_alarm_ctx = {0};

#ifdef LISA_UI_PLATFORM_ARCS
static void alarm_trigger_nav_ui_worker(void *arg, uint32_t len)
{
    if (!arg || len < sizeof(struct service_alarm)) {
        LISA_UI_LOGE("Model: invalid alarm trigger payload");
        return;
    }

    const struct service_alarm *alarm = (const struct service_alarm *)arg;

    LISA_UI_LOGI("Model: Alarm triggered - timestamp: %llu", alarm->timestamp);
    g_alarm_ctx.data.timestamp = alarm->timestamp;
    g_alarm_ctx.event_text[0] = '\0';
    g_alarm_ctx.initialized = true;
    g_alarm_ctx.is_ringing = true;

    int top_id = lisa_ui_nav_scr_get_top_id();
    if (top_id == LISA_UI_NAV_SCR_ID_ALARM_RING) {
        LISA_UI_LOGI("Already on alarm ring page, skipping navigation");
        return;
    }

    if (top_id != LISA_UI_NAV_SCR_ID_HOME) {
        LISA_UI_LOGI("Model: Navigating to home before alarm ring, top_id=%d", top_id);
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }

    LISA_UI_LOGI("Model: Navigating to alarm ring page");
    lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_ALARM_RING);
}

static void on_alarm_create_event(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    struct service_alarm *alarm = (struct service_alarm *)data;

    LISA_UI_LOGI("Model: Alarm created - timestamp: %llu", alarm->timestamp);

    g_alarm_ctx.alarm_count++;

    LISA_UI_INVOKE_UI_ARG_PTR(alarm, sizeof(struct service_alarm), {
        g_alarm_ctx.data.timestamp = _invoke_alarm->timestamp;
        g_alarm_ctx.event_text[0] = '\0';
        g_alarm_ctx.initialized = true;

        if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_ALARM_SUCCESS) {
            LISA_UI_LOGI("Already on alarm success page, skipping navigation");
            return;
        }
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_ALARM_SUCCESS);
    });
}

static void on_alarm_delete_event(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (g_alarm_ctx.alarm_count > 0) {
        g_alarm_ctx.alarm_count--;
    }
}

static void on_alarm_query_event(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!data || len < sizeof(uint32_t)) {
        return;
    }

    g_alarm_ctx.alarm_count = *(uint32_t *)data;
}

static void on_alarm_trigger_event(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    struct service_alarm *alarm = (struct service_alarm *)data;
    if (!alarm) {
        LISA_UI_LOGE("Model: alarm trigger data is NULL");
        return;
    }
    
#ifdef LISA_UI_PLATFORM_ARCS
    struct app_datas *app_datas = get_app_datas();
    if (app_datas) {
        LISA_UI_LOGI("Model: Before setting - can_wakeup=%d, voice_work_mode=%d",
                     app_datas->can_wakeup, app_datas->voice_work_mode);
        app_datas->can_wakeup = true;
        app_datas->voice_work_mode |= VOICE_WORK_MODE_VOICE_WAKEUP;
        LISA_UI_LOGI("Model: After setting - can_wakeup=%d, voice_work_mode=%d",
                     app_datas->can_wakeup, app_datas->voice_work_mode);
    } else {
        LISA_UI_LOGE("Model: Failed to get app_datas!");
    }

    /* Alarm page switch can race with MCP session UI transition; stop MCP first. */
    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);

    if (lisa_ui_invoke_ui_delayed(alarm_trigger_nav_ui_worker, alarm, sizeof(struct service_alarm),
                                  ALARM_TRIGGER_NAV_DELAY_MS) != 0) {
        LISA_UI_LOGE("Model: failed to schedule alarm ring navigation");
    }
#endif
    
}
#endif

int model_alarm_init(void)
{
    if (g_alarm_ctx.initialized) {
        return 0;
    }

    LISA_UI_LOGI("Initializing alarm model");

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_sub(VOICE_MSG_ALARM_CREATE, on_alarm_create_event, NULL);
    voice_msg_sub(VOICE_MSG_ALARM_DELETE, on_alarm_delete_event, NULL);
    voice_msg_sub(VOICE_MSG_ALARM_QUERY, on_alarm_query_event, NULL);
    voice_msg_sub(VOICE_MSG_ALARM_TRIGGER, on_alarm_trigger_event, NULL);

    uint32_t cnt = 0;
    struct service_alarm *service_alarms = service_alarm_get_all(&cnt);
    if (service_alarms) {
        lisa_mem_free(service_alarms);
    }
    g_alarm_ctx.alarm_count = cnt;
#endif

    g_alarm_ctx.initialized = true;

    LISA_UI_LOGI("Alarm model initialized");
    return 0;
}

void model_alarm_set_data(const alarm_data_t *data)
{
    if (!data) {
        LISA_UI_LOGE("Invalid alarm data");
        return;
    }

    g_alarm_ctx.data = *data;

    LISA_UI_LOGI("Alarm data set - timestamp: %llu", (unsigned long long)data->timestamp);
}

int model_alarm_get_data(alarm_data_t *data)
{
    if (!data) {
        LISA_UI_LOGE("Invalid output parameter");
        return -1;
    }

    if (!g_alarm_ctx.initialized) {
        LISA_UI_LOGW("Alarm model not initialized");
        return -1;
    }

    *data = g_alarm_ctx.data;
    return 0;
}

const char *model_alarm_get_event_text(void)
{
    return g_alarm_ctx.event_text;
}

int model_alarm_get_time_info(uint64_t timestamp, alarm_time_info_t *info)
{
    if (!info) {
        LISA_UI_LOGE("Invalid parameter");
        return -1;
    }

    struct timeval tv;
    time_t current_time = time(NULL);
    if (gettimeofday(&tv, NULL) == 0) {
        current_time = tv.tv_sec;
    }

    time_t alarm_time = (time_t)timestamp;

    struct tm alarm_tm_struct;
    struct tm *alarm_tm = localtime_r(&alarm_time, &alarm_tm_struct);

    if (!alarm_tm) {
        LISA_UI_LOGE("Failed to convert timestamp");
        return -1;
    }

    info->year = alarm_tm->tm_year + 1970;
    info->month = alarm_tm->tm_mon + 1;
    info->day = alarm_tm->tm_mday;
    info->hour = alarm_tm->tm_hour;
    info->minute = alarm_tm->tm_min;

    int64_t local_ts = (int64_t)timestamp + (int64_t)8 * 3600;
    int64_t days_since_epoch = local_ts / 86400;
    info->weekday = (int)((4 + (days_since_epoch % 7) + 7) % 7);

    int64_t alarm_local_days = ((int64_t)timestamp + (int64_t)8 * 3600) / 86400;
    int64_t current_local_days = ((int64_t)current_time + (int64_t)8 * 3600) / 86400;
    int64_t day_diff = alarm_local_days - current_local_days;

    info->is_today = (day_diff == 0);
    info->is_tomorrow = (day_diff == 1);

    return 0;
}

int model_alarm_delete_by_timestamp(uint64_t timestamp)
{
#ifdef LISA_UI_PLATFORM_ARCS
    int r = ls_alarm_delete_by_timestamp(timestamp);
    return r;
#endif
    return 0;
}

void model_alarm_free(void *alarms)
{
    if (alarms) {
        lisa_ui_free(alarms);
    }
}

uint32_t model_alarm_count_get(void)
{
    return g_alarm_ctx.alarm_count;
}

alarm_data_t *model_alarm_get_all(uint32_t *cnt)
{
    if (!cnt) {
        LISA_UI_LOGE("Invalid parameter: cnt is NULL");
        return NULL;
    }

#ifdef LISA_UI_PLATFORM_ARCS

    if (!service_alarm_init_done()) {
        *cnt = 0;
        return NULL;
    }

    uint32_t alarm_count = 0;
    struct service_alarm *service_alarms = service_alarm_get_all(&alarm_count);

    *cnt = alarm_count;

    if (alarm_count == 0 || service_alarms == NULL) {
        return NULL;
    }

    // Allocate memory for alarm_data_t array
    alarm_data_t *alarms = lisa_ui_malloc(sizeof(alarm_data_t) * alarm_count);
    if (alarms == NULL) {
        LISA_UI_LOGE("Failed to allocate memory for alarms");
        lisa_mem_free(service_alarms);
        *cnt = 0;
        return NULL;
    }

    // Convert service_alarm to alarm_data_t
    for (uint32_t i = 0; i < alarm_count; i++) {
        alarms[i].timestamp = service_alarms[i].timestamp;
        LISA_UI_LOGI("alarm %d, timestamp:%llu", i, alarms[i].timestamp);
    }

    // Free the service_alarm array
    lisa_mem_free(service_alarms);

    LISA_UI_LOGI("Got %u alarms", alarm_count);
    return alarms;
#else
    *cnt = 0;
    return NULL;
#endif
}
