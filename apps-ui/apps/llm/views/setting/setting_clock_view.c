/**
 * @file setting_clock_view.c
 * @brief Clock/Alarm settings view implementation
 */

#include "setting_clock_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include <time.h>
#include <stdio.h>
#define TAG "setting_clock_view"

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *back_btn;
    lv_obj_t *scroll_container;
    lv_obj_t *empty_label;
    lisa_ui_setting_clock_back_cb_t back_cb;     /* 返回按钮回调函数 */
    void *back_user_data;                        /* 回调函数用户数据 */
    lisa_ui_setting_clock_delete_cb_t delete_cb; /* 删除闹钟回调函数 */
    alarm_item_t alarm_items[20];                /* 使用统一的alarm结构体 */
    int alarm_count;
} lisa_ui_setting_clock_view_t;

static void lisa_ui_setting_clock_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_setting_clock_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .constructor_cb = lisa_ui_setting_clock_view_class_constructor,
    .instance_size = sizeof(lisa_ui_setting_clock_view_t),
};

static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)lv_event_get_user_data(e);
        LISA_UI_LOGD("Clock back button clicked");
        /* 调用回调函数，将页面导航逻辑交给presenter层处理 */
        if (view && view->back_cb) {
            view->back_cb(view->back_user_data);
        }
    }
}

static void delete_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *view_obj = lv_obj_get_parent(lv_obj_get_parent(btn));

        while (view_obj && !lv_obj_check_type(view_obj, &lisa_ui_setting_clock_view_class)) {
            view_obj = lv_obj_get_parent(view_obj);
        }

        if (!view_obj) {
            return;
        }

        lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)view_obj;
        if (!view || !view->delete_cb) {
            return;
        }

        uint64_t *timestamp = (uint64_t *)lv_event_get_user_data(e);
        if (timestamp) {
            view->delete_cb(*timestamp);
        }
    }
}

/* 手动将timestamp转换为日期时间，不依赖localtime() */
static void timestamp_to_datetime(uint64_t timestamp, int *year, int *month, int *day, int *hour, int *minute,
                                  int *second)
{
    /* 时区偏移：UTC+8 (28800秒) */
    const int timezone_offset = 8 * 3600;
    time_t t = (time_t)timestamp + timezone_offset;

    /* 计算天数 */
    int days = t / 86400;
    int remaining = t % 86400;

    /* 计算时分秒 */
    *hour = remaining / 3600;
    *minute = (remaining % 3600) / 60;
    *second = remaining % 60;

    /* 计算年月日 (从1970年1月1日开始) */
    int y = 1970;
    while (1) {
        int days_in_year = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
        if (days < days_in_year) {
            break;
        }
        days -= days_in_year;
        y++;
    }
    *year = y;

    /* 计算月份 */
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (*year % 4 == 0 && (*year % 100 != 0 || *year % 400 == 0)) {
        days_in_month[1] = 29;
    }

    int m = 0;
    while (m < 12 && days >= days_in_month[m]) {
        days -= days_in_month[m];
        m++;
    }
    *month = m + 1;
    *day = days + 1;
}

/* 使用蔡勒公式计算星期几 */
static int calculate_weekday_zeller(int year, int month, int day)
{
    /* 蔡勒公式：W = (d + (13*(m+1))/5 + y + y/4 - y/100 + y/400) % 7 */
    /* 注意：1月和2月要当作上一年的13月和14月来计算 */
    if (month < 3) {
        month += 12;
        year -= 1;
    }

    int century = year / 100;
    int year_of_century = year % 100;

    int w = (day + (13 * (month + 1)) / 5 + year_of_century + year_of_century / 4 + century / 4 - 2 * century) % 7;

    /* 蔡勒公式结果：0=周六, 1=周日, 2=周一, 3=周二, 4=周三, 5=周四, 6=周五 */
    /* 转换为：0=周日, 1=周一, 2=周二, 3=周三, 4=周四, 5=周五, 6=周六 */
    int weekday = (w + 6) % 7;

    return weekday;
}

static void format_alarm_time_string(uint64_t timestamp, char *time_str, char *date_str)
{
    int year, month, day, hour, minute, second;

    LISA_UI_LOGI("Format alarm time - timestamp: %llu", (unsigned long long)timestamp);

    /* 检查timestamp是否有效（负数或过大的值都是无效的） */
    if ((int64_t)timestamp < 0 || timestamp > 4102444800ULL) {
        LISA_UI_LOGE("  Invalid timestamp: %lld (negative or too large)", (int64_t)timestamp);
        snprintf(time_str, 16, "--:--");
        snprintf(date_str, 32, _("alarm invalid"));
        return;
    }

    /* 手动转换时间戳 */
    timestamp_to_datetime(timestamp, &year, &month, &day, &hour, &minute, &second);

    LISA_UI_LOGI("  Converted: year=%d, mon=%d, mday=%d, hour=%d, min=%d, sec=%d", year, month, day, hour, minute,
                 second);

    /* 格式化时间 */
    snprintf(time_str, 16, "%02d:%02d", hour, minute);

    /* 使用蔡勒公式计算正确的星期几 */
    int weekday = calculate_weekday_zeller(year, month, day);

    LISA_UI_LOGI("  Zeller weekday: %d", weekday);

    const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    snprintf(date_str, 32, "%d月%d日 %s", month, day, weekdays[weekday]);

    LISA_UI_LOGI("  Formatted: time='%s', date='%s'", time_str, date_str);
}

static lv_obj_t *create_alarm_item(lv_obj_t *parent, const alarm_item_t *alarm_item, int y_offset)
{
    lv_obj_t *alarm_container = lv_obj_create(parent);
    lv_obj_set_size(alarm_container, LV_PCT(95), 80);
    lv_obj_align(alarm_container, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_set_style_bg_color(alarm_container, lv_color_make(0x4A, 0x4A, 0x4A), 0);
    lv_obj_set_style_border_width(alarm_container, 0, 0);
    lv_obj_set_style_radius(alarm_container, 8, 0);
    lv_obj_set_style_pad_all(alarm_container, 10, 0);

    char time_str[16];
    char date_str[32];
    format_alarm_time_string(alarm_item->timestamp, time_str, date_str);

    lv_obj_t *time_label = lv_label_create(alarm_container);
    lv_label_set_text(time_label, time_str);
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_chinese_18, 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 5, 8);

    lv_obj_t *date_label = lv_label_create(alarm_container);
    lv_label_set_text(date_label, date_str);
    lv_obj_set_style_text_color(date_label, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_set_style_text_font(date_label, &lv_font_chinese_18, 0);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_LEFT, 5, -8);

    lv_obj_t *delete_btn = lv_btn_create(alarm_container);
    lv_obj_set_size(delete_btn, 50, 30);
    lv_obj_align(delete_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(delete_btn, lv_color_make(0xFF, 0x44, 0x44), 0);
    lv_obj_set_style_border_width(delete_btn, 0, 0);
    lv_obj_set_style_radius(delete_btn, 4, 0);

    lv_obj_t *delete_label = lv_label_create(delete_btn);
    lv_label_set_text(delete_label, _("cancel"));
    lv_obj_set_style_text_color(delete_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(delete_label, &lv_font_chinese_18, 0);
    lv_obj_center(delete_label);

    lv_obj_add_event_cb(delete_btn, delete_btn_event_cb, LV_EVENT_CLICKED, (void *)&alarm_item->timestamp);

    return alarm_container;
}

static void lisa_ui_setting_clock_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);

    lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)obj;

    // Hide the bar to allow content to start from the very top
    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (NULL == container) {
        LISA_UI_LOGE("Failed to get base container");
        return;
    }

    // Back button at the very top
    view->back_btn = lv_btn_create(container);
    lv_obj_set_size(view->back_btn, 30, 30);
    lv_obj_align(view->back_btn, LV_ALIGN_TOP_LEFT, 5, 0);
    lv_obj_set_style_bg_opa(view->back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->back_btn, 0, LV_PART_MAIN);

    lv_obj_t *back_icon = lv_img_create(view->back_btn);
    lv_img_set_src(back_icon, &icons_icon_back_png);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(view->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, view);

    /* 在返回按钮旁边创建标题文本 */
    lv_obj_t *title = lv_label_create(container);
    lv_label_set_text(title, _("alarm setting"));
    lv_obj_set_style_text_font(title, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 45, 8); /* 紧贴返回按钮右侧 */

    /* 初始化列表容器为NULL，等待刷新时创建 */
    view->scroll_container = NULL;
    view->empty_label = NULL;
    view->delete_cb = NULL;
    view->alarm_count = 0;
}

lv_obj_t *lisa_ui_setting_clock_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_setting_clock_view_class, parent);
    lv_obj_class_init_obj(obj);

    /* 初始化回调函数为NULL */
    lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)obj;
    view->back_cb = NULL;
    view->back_user_data = NULL;
    view->delete_cb = NULL;

    return obj;
}

void lisa_ui_setting_clock_view_set_alarms(lv_obj_t *obj, const alarm_item_t *alarms, uint32_t count)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)obj;

    /* 限制最大数量 */
    uint32_t copy_count = (count > 20) ? 20 : count;

    /* 复制闹钟数据到view */
    for (uint32_t i = 0; i < copy_count; i++) {
        view->alarm_items[i].timestamp = alarms[i].timestamp;
        LISA_UI_LOGI("set alarms, timestamp: %llu", view->alarm_items[i].timestamp);
    }

    view->alarm_count = copy_count;

    LISA_UI_LOGD("Set %u alarms to view", copy_count);
}

void lisa_ui_setting_clock_view_refresh_list(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)obj;

    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (!container) {
        return;
    }

    /* 清理旧的UI元素 */
    if (view->scroll_container) {
        lv_obj_del(view->scroll_container);
        view->scroll_container = NULL;
    }
    if (view->empty_label) {
        lv_obj_del(view->empty_label);
        view->empty_label = NULL;
    }

    LISA_UI_LOGD("Refreshing alarm list, count=%d", view->alarm_count);

    if (view->alarm_count == 0) {
        LISA_UI_LOGD("No alarms to display, showing empty state");
        view->empty_label = lv_label_create(container);
        lv_label_set_text(view->empty_label, _("no alarm"));
        lv_obj_set_style_text_color(view->empty_label, lv_color_make(0x88, 0x88, 0x88), 0);
        lv_obj_set_style_text_font(view->empty_label, &lv_font_chinese_18, 0);
        lv_obj_set_style_text_align(view->empty_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(view->empty_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_width(view->empty_label, lv_pct(100));
        lv_obj_set_height(view->empty_label, LV_SIZE_CONTENT);
        lv_label_set_long_mode(view->empty_label, LV_LABEL_LONG_WRAP);
        return;
    }

    /* 创建滚动容器 */
    view->scroll_container = lv_obj_create(container);
    lv_obj_set_size(view->scroll_container, LV_PCT(100), LV_PCT(75));
    lv_obj_align(view->scroll_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(view->scroll_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(view->scroll_container, 0, 0);
    lv_obj_set_style_pad_all(view->scroll_container, 10, 0);

    /* 创建UI元素 */
    int y_offset = 10;
    for (int i = 0; i < view->alarm_count; i++) {
        /* 跳过无效的时间戳 */
        if (view->alarm_items[i].timestamp == 0) {
            LISA_UI_LOGD("Skipping alarm with timestamp=0 at index %d", i);
            continue;
        }

        LISA_UI_LOGI("Creating alarm item %d: timestamp=%llu", i, (unsigned long long)view->alarm_items[i].timestamp);

        lv_obj_t *item = create_alarm_item(view->scroll_container, &view->alarm_items[i], y_offset);
        if (item) {
            y_offset += 90;
        }
    }
}

void lisa_ui_setting_clock_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_clock_back_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }

    /* 设置返回按钮回调函数和用户数据 */
    lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)obj;
    view->back_cb = cb;
    view->back_user_data = user_data;
}

void lisa_ui_setting_clock_view_set_delete_cb(lv_obj_t *obj, lisa_ui_setting_clock_delete_cb_t cb)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_clock_view_t *view = (lisa_ui_setting_clock_view_t *)obj;
    view->delete_cb = cb;
}
