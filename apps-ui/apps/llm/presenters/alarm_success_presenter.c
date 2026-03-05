/**
 * @file alarm_success_presenter.c
 * @brief Alarm success page presenter implementation
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "alarm_success_presenter"

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "alarm_success_view.h"
#include "model_alarm.h"

struct alarm_success_nav_scr_data {
    lv_obj_t *view;
    lv_timer_t *auto_return;
};

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

static void auto_return_timer_cb(lv_timer_t *timer)
{
    LISA_UI_LOGD("Auto return timer triggered, navigating to home");
    lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
}

static int alarm_success_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("Alarm success nav scr open, id: %d", scr->unique_id);
    
    struct alarm_success_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct alarm_success_nav_scr_data));
    if (!scr_data) {
        LISA_UI_LOGE("Failed to allocate memory");
        return -1;
    }
    
    scr_data->view = alarm_success_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create alarm success view");
        lisa_ui_free(scr_data);
        return -1;
    }
    
    /* 先隐藏view，避免在设置文本时闪烁 */
    lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    
    alarm_data_t alarm_data;
    if (model_alarm_get_data(&alarm_data) == 0) {
        alarm_time_info_t time_info;
        if (model_alarm_get_time_info(alarm_data.timestamp, &time_info) == 0) {
            char time_str[16];
            char date_str[32];
            format_alarm_time(&time_info, time_str, sizeof(time_str), date_str, sizeof(date_str));
            
            alarm_success_view_set_time(scr_data->view, time_str);
            alarm_success_view_set_date(scr_data->view, date_str);
            
            LISA_UI_LOGI("Alarm success page - time: %s, date: %s", time_str, date_str);
        } else {
            LISA_UI_LOGW("Failed to get time info");
        }
    } else {
        LISA_UI_LOGW("No alarm data available, using default");
    }
    
    /* 数据设置完成后立即显示，避免在show时才显示导致闪烁 */
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    
    scr_data->auto_return = lv_timer_create(auto_return_timer_cb, 3000, NULL);
    lv_timer_set_repeat_count(scr_data->auto_return, 1);
    
    *data = scr_data;
    return 0;
}

static int alarm_success_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm success nav scr show, id: %d", scr->unique_id);
    
    struct alarm_success_nav_scr_data *scr_data = (struct alarm_success_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("Invalid alarm success nav scr data");
        return -1;
    }
    
    /* view已经在open时显示，这里不需要再操作 */
    return 0;
}

static int alarm_success_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm success nav scr pause, id: %d", scr->unique_id);
    
    struct alarm_success_nav_scr_data *scr_data = (struct alarm_success_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }
    
    return 0;
}

static int alarm_success_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm success nav scr resume, id: %d", scr->unique_id);
    
    struct alarm_success_nav_scr_data *scr_data = (struct alarm_success_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }
    
    return 0;
}

static int alarm_success_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Alarm success nav scr close, id: %d", scr->unique_id);
    
    struct alarm_success_nav_scr_data *scr_data = (struct alarm_success_nav_scr_data *)data;
    if (scr_data) {
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

const struct lisa_ui_nav_scr alarm_success_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_ALARM_SUCCESS,
    .open = alarm_success_nav_scr_open,
    .show = alarm_success_nav_scr_show,
    .pause = alarm_success_nav_scr_pause,
    .resume = alarm_success_nav_scr_resume,
    .close = alarm_success_nav_scr_close,
};
