/**
 * @file setting_clock_presenter.c
 * @brief Alarm/Clock settings presenter - handles data logic and navigation
 */

#include "lisa_ui_nav_scr.h"
#include "lisa_ui.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_fonts.h"
#include "setting_clock_view.h"
#include "model_alarm.h"
#include "model_voice.h"

#include <stdbool.h>

#define TAG "setting_clock_presenter"

static bool alarm_navigation_initialized = false;

struct setting_clock_nav_scr_data {
    lv_obj_t *view;
    lv_obj_t *confirm_dialog;
    uint64_t pending_delete_timestamp;
};

static struct setting_clock_nav_scr_data *g_current_scr_data = NULL;

static void on_delete_alarm(uint64_t timestamp);
static void create_delete_confirm_dialog(struct setting_clock_nav_scr_data *scr_data, uint64_t timestamp);

/**
 * @brief 返回按钮回调 - 处理返回到设置主页面的导航
 * 这是presenter层的业务逻辑
 */
static void setting_clock_back_btn_cb(void *user_data)
{
    (void)user_data;
    LISA_UI_LOGD("Clock settings back button clicked, returning to settings");
    lisa_ui_nav_scr_nav_back();
}

static int setting_clock_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    (void)scr;
    LISA_UI_LOGD("Clock/Alarm settings nav scr open");

    struct setting_clock_nav_scr_data *scr_data =
        (struct setting_clock_nav_scr_data *)lisa_ui_malloc(sizeof(struct setting_clock_nav_scr_data));
    if (!scr_data) {
        return -1;
    }

    memset(scr_data, 0, sizeof(struct setting_clock_nav_scr_data));

    // Create clock view (UI layout handled by view)
    scr_data->view = lisa_ui_setting_clock_view_create(lv_scr_act());
    if (!scr_data->view) {
        LISA_UI_LOGE("Failed to create clock view");
        lisa_ui_free(scr_data);
        return -1;
    }

    /* 注册返回按钮回调 */
    lisa_ui_setting_clock_view_set_back_cb(scr_data->view, setting_clock_back_btn_cb, NULL);

    /* 注册删除回调 */
    lisa_ui_setting_clock_view_set_delete_cb(scr_data->view, on_delete_alarm);

    /* 设置全局变量 */
    g_current_scr_data = scr_data;

    /* 初始化刷新闹钟列表 */
    lisa_ui_setting_clock_view_refresh_list(scr_data->view);

    *data = scr_data;
    return 0;
}

extern int ls_alarm_delete_by_timestamp(uint64_t timestamp);

static void delete_confirm_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *btn = lv_event_get_target(e);
        struct setting_clock_nav_scr_data *scr_data = (struct setting_clock_nav_scr_data *)lv_event_get_user_data(e);

        if (!scr_data) {
            return;
        }

        lv_obj_t *label = lv_obj_get_child(btn, 0);
        const char *btn_text = lv_label_get_text(label);

        if (strcmp(btn_text, "删除") == 0) {
            if (scr_data->pending_delete_timestamp > 0) {
                LISA_UI_LOGD("Deleting alarm with timestamp: %llu",
                             (unsigned long long)scr_data->pending_delete_timestamp);

                int result = model_alarm_delete_by_timestamp(scr_data->pending_delete_timestamp);

                if (result == 0) {
                    LISA_UI_LOGD("Alarm deletion successful");

                    if (scr_data->confirm_dialog) {
                        lv_obj_del_async(scr_data->confirm_dialog);
                        scr_data->confirm_dialog = NULL;
                    }

                    if (scr_data->view) {
                        /* 重新获取最新的闹钟列表数据 */
                        uint32_t cnt;
                        alarm_data_t *alarms = model_alarm_get_all(&cnt);
                        if (alarms && cnt > 0) {
                            alarm_item_t *alarm_items = lisa_ui_malloc(cnt * sizeof(alarm_item_t));
                            if (alarm_items) {
                                for (int i = 0; i < cnt; i++) {
                                    alarm_items[i].timestamp = alarms[i].timestamp;
                                }
                                lisa_ui_setting_clock_view_set_alarms(scr_data->view, alarm_items, cnt);
                                lisa_ui_free(alarm_items);
                            }
                            model_alarm_free(alarms);
                        } else {
                            /* 如果没有闹钟了，设置空列表 */
                            lisa_ui_setting_clock_view_set_alarms(scr_data->view, NULL, 0);
                        }

                        /* 刷新UI显示 */
                        lisa_ui_setting_clock_view_refresh_list(scr_data->view);
                    }
                } else {
                    LISA_UI_LOGE("Failed to delete alarm, result: %d", result);
                    if (scr_data->confirm_dialog) {
                        lv_obj_del_async(scr_data->confirm_dialog);
                        scr_data->confirm_dialog = NULL;
                    }
                }
            }
        } else {
            LISA_UI_LOGD("Delete cancelled by user");
            if (scr_data->confirm_dialog) {
                lv_obj_del_async(scr_data->confirm_dialog);
                scr_data->confirm_dialog = NULL;
            }
        }

        scr_data->pending_delete_timestamp = 0;
    }
}

static void create_delete_confirm_dialog(struct setting_clock_nav_scr_data *scr_data, uint64_t timestamp)
{
    if (!scr_data) {
        return;
    }

    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_center(bg);

    lv_obj_t *dialog = lv_obj_create(bg);
    lv_obj_set_size(dialog, 280, 160);
    lv_obj_set_style_bg_color(dialog, lv_color_make(0x2A, 0x2A, 0x2A), 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_set_style_radius(dialog, 12, 0);
    lv_obj_center(dialog);

    lv_obj_t *title_label = lv_label_create(dialog);
    lv_label_set_text(title_label, "是否删除该闹钟");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_chinese_16, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, LV_PCT(90), 50);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, 100, 40);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_make(0x55, 0x55, 0x55), 0);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_set_style_radius(cancel_btn, 8, 0);

    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "取消");
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(cancel_label, &lv_font_chinese_16, 0);
    lv_obj_center(cancel_label);

    lv_obj_t *delete_btn = lv_btn_create(btn_container);
    lv_obj_set_size(delete_btn, 100, 40);
    lv_obj_set_style_bg_color(delete_btn, lv_color_make(0xFF, 0x44, 0x44), 0);
    lv_obj_set_style_border_width(delete_btn, 0, 0);
    lv_obj_set_style_radius(delete_btn, 8, 0);

    lv_obj_t *delete_label = lv_label_create(delete_btn);
    lv_label_set_text(delete_label, "删除");
    lv_obj_set_style_text_color(delete_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(delete_label, &lv_font_chinese_16, 0);
    lv_obj_center(delete_label);

    lv_obj_add_event_cb(cancel_btn, delete_confirm_btn_event_cb, LV_EVENT_CLICKED, scr_data);
    lv_obj_add_event_cb(delete_btn, delete_confirm_btn_event_cb, LV_EVENT_CLICKED, scr_data);

    scr_data->confirm_dialog = bg;
    scr_data->pending_delete_timestamp = timestamp;

    LISA_UI_LOGD("Created delete confirmation dialog for timestamp: %llu", (unsigned long long)timestamp);
}

static void on_delete_alarm(uint64_t timestamp)
{
    if (g_current_scr_data) {
        create_delete_confirm_dialog(g_current_scr_data, timestamp);
    }
}

static int setting_clock_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    struct setting_clock_nav_scr_data *scr_data = (struct setting_clock_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);

    uint32_t cnt;
    alarm_data_t *alarms = model_alarm_get_all(&cnt);
    if (alarms && cnt) {
        alarm_item_t *alarm_items = lisa_ui_malloc(cnt * sizeof(alarm_item_t));
        if (alarm_items) {
            for (int i = 0; i < cnt; i++) {
                alarm_items[i].timestamp = alarms[i].timestamp;
                LISA_UI_LOGI("alarm item: %llu", alarm_items[i].timestamp);
            }

            lisa_ui_setting_clock_view_set_alarms(scr_data->view, alarm_items, cnt);
            lisa_ui_setting_clock_view_refresh_list(scr_data->view);
            lisa_ui_free(alarm_items);
        }
        model_alarm_free(alarms);
    }

    return 0;
}

static int setting_clock_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    struct setting_clock_nav_scr_data *scr_data = (struct setting_clock_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->confirm_dialog) {
            lv_obj_del(scr_data->confirm_dialog);
            scr_data->confirm_dialog = NULL;
        }
        if (scr_data->view) {
            lv_obj_del(scr_data->view);
            scr_data->view = NULL;
        }

        /* 清空全局变量 */
        if (g_current_scr_data == scr_data) {
            g_current_scr_data = NULL;
        }

        lisa_ui_free(scr_data);
    }
    return 0;
}

const struct lisa_ui_nav_scr setting_clock_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SETTING_CLOCK,
    .open = setting_clock_nav_scr_open,
    .show = setting_clock_nav_scr_show,
    .close = setting_clock_nav_scr_close,
};

/**
 * @brief 启用语音唤醒（Presenter订阅Model的语音控制指令）
 */
static void enable_voice_wakeup(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_LOGI("Presenter: Executing voice wakeup enable");
    model_voice_on();
}

/**
 * @brief 禁用语音唤醒（Presenter订阅Model的语音控制指令）
 */
static void disable_voice_wakeup(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_LOGI("Presenter: Executing voice wakeup disable");
    model_voice_off();
}

/**
 * @brief 初始化闹钟导航处理器（Presenter订阅Model的导航指令）
 */
void alarm_navigation_init(void)
{
    if (alarm_navigation_initialized) {
        LISA_UI_LOGI("Alarm navigation handler already initialized");
        return;
    }

    LISA_UI_LOGI("Initializing alarm navigation handler");

    alarm_navigation_initialized = true;

    LISA_UI_LOGI("Alarm navigation and voice control handler initialized");
}
