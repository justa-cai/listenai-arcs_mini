/**
 * @file setting_wifi_view.c
 * @brief WiFi settings view implementation
 */
#include <stdio.h>

#include "setting_wifi_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include "../lisa_ui_keyboard_input.h"

#define TAG "setting_wifi_view"
#define WIFI_LIST_UPDATE_THROTTLE_MS 500  /* 列表更新限流时间(ms) */
#define WIFI_LIST_BATCH_CREATE_SIZE 5     /* 批量创建列表项数量 */

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *back_btn;
    lv_obj_t *wifi_switch;
    lv_obj_t *wifi_list;                      /* WiFi 列表容器 */
    lv_obj_t *info_label;                     /* 提示信息标签 */
    lisa_ui_setting_wifi_back_cb_t back_cb;   /* 返回按钮回调函数 */
    void *back_user_data;                     /* 回调函数用户数据 */
    lisa_ui_setting_wifi_item_cb_t item_cb;   /* WiFi 项点击回调 */
    void *item_user_data;                     /* WiFi 项回调用户数据 */

    /* 密码输入界面相关 */
    lv_obj_t *keyboard_input;                 /* 键盘输入组件 */
    char current_ssid[32];                    /* 当前选中的SSID */
    lisa_ui_setting_wifi_password_confirm_cb_t password_confirm_cb;  /* 密码确认回调 */
    lisa_ui_setting_wifi_password_cancel_cb_t password_cancel_cb;    /* 密码取消回调 */
    void *password_user_data;                 /* 密码输入回调用户数据 */

    /* 性能优化相关 */
    lv_style_t item_style;                    /* WiFi项样式(复用) */
    lv_style_t item_pressed_style;            /* WiFi项按下样式(复用) */
    uint32_t last_update_time;                /* 上次更新时间戳 */
} lisa_ui_setting_wifi_view_t;

static void lisa_ui_setting_wifi_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_setting_wifi_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .constructor_cb = lisa_ui_setting_wifi_view_class_constructor,
    .instance_size = sizeof(lisa_ui_setting_wifi_view_t),
};

static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)lv_event_get_user_data(e);
        LISA_UI_LOGD("WiFi back button clicked");
        /* 调用回调函数，将页面导航逻辑交给presenter层处理 */
        if (view && view->back_cb) {
            view->back_cb(view->back_user_data);
        }
    }
}

static void lisa_ui_setting_wifi_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);

    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;

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

    /* 初始化可复用的样式 */
    lv_style_init(&view->item_style);
    lv_style_set_bg_color(&view->item_style, lv_color_hex(0x1a1a1a));
    lv_style_set_border_width(&view->item_style, 0);
    lv_style_set_radius(&view->item_style, 8);
    lv_style_set_pad_all(&view->item_style, 12);
    lv_style_set_shadow_width(&view->item_style, 4);
    lv_style_set_shadow_color(&view->item_style, lv_color_black());
    lv_style_set_shadow_opa(&view->item_style, LV_OPA_20);

    lv_style_init(&view->item_pressed_style);
    lv_style_set_bg_color(&view->item_pressed_style, lv_color_hex(0x2a2a2a));

    view->last_update_time = 0;

    /* ============ 标题栏区域 ============ */
    // 创建标题栏容器
    lv_obj_t *header = lv_obj_create(container);
    lv_obj_set_size(header, LV_PCT(100), 45);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);

    // 返回按钮
    view->back_btn = lv_btn_create(header);
    lv_obj_set_size(view->back_btn, 35, 35);
    lv_obj_align(view->back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_opa(view->back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(view->back_btn, 0, LV_PART_MAIN);

    lv_obj_t *back_icon = lv_img_create(view->back_btn);
    lv_img_set_src(back_icon, &icons_icon_back_png);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(view->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, view);

    // 标题文本
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, _("wifi setting"));
    lv_obj_set_style_text_font(title, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 50, 0);

    // /* ============ WiFi 开关区域 ============ */
    // // 创建 WiFi 开关容器
    // lv_obj_t *switch_container = lv_obj_create(container);
    // lv_obj_set_size(switch_container, LV_PCT(100), 50);
    // lv_obj_align(switch_container, LV_ALIGN_TOP_MID, 0, 50);
    // lv_obj_set_style_bg_color(switch_container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    // lv_obj_set_style_border_width(switch_container, 0, LV_PART_MAIN);
    // lv_obj_set_style_radius(switch_container, 8, LV_PART_MAIN);
    // lv_obj_set_style_pad_all(switch_container, 15, LV_PART_MAIN);
    // lv_obj_clear_flag(switch_container, LV_OBJ_FLAG_SCROLLABLE);

    // // WiFi 标签
    // lv_obj_t *wifi_label = lv_label_create(switch_container);
    // lv_label_set_text(wifi_label, _("AutoScan"));
    // lv_obj_set_style_text_font(wifi_label, &lv_font_chinese_16, LV_PART_MAIN);
    // lv_obj_set_style_text_color(wifi_label, lv_color_white(), LV_PART_MAIN);
    // lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 0, 0);

    // // WiFi 开关
    // view->wifi_switch = lv_switch_create(switch_container);
    // lv_obj_set_size(view->wifi_switch, 50, 26);
    // lv_obj_align(view->wifi_switch, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ============ 列表区域 ============ */
    // WiFi 列表容器（可滚动）
    view->wifi_list = lv_obj_create(container);
    lv_obj_set_size(view->wifi_list, LV_PCT(100), 180);
    lv_obj_align(view->wifi_list, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_opa(view->wifi_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(view->wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(view->wifi_list, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(view->wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view->wifi_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(view->wifi_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(view->wifi_list, LV_DIR_VER);

    // 提示信息标签（默认隐藏）
    view->info_label = lv_label_create(container);
    lv_label_set_text(view->info_label, _("scaning"));
    lv_obj_set_style_text_font(view->info_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(view->info_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_align(view->info_label, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_flag(view->info_label, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *lisa_ui_setting_wifi_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_setting_wifi_view_class, parent);
    lv_obj_class_init_obj(obj);
    
    /* 初始化回调函数为NULL */
    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;
    view->back_cb = NULL;
    view->back_user_data = NULL;
    view->item_cb = NULL;
    view->item_user_data = NULL;
    view->keyboard_input = NULL;
    view->current_ssid[0] = '\0';
    view->password_confirm_cb = NULL;
    view->password_cancel_cb = NULL;
    view->password_user_data = NULL;

    return obj;
}

lv_obj_t *lisa_ui_setting_wifi_view_get_switch(lv_obj_t *obj)
{
    if (!obj) {
        return NULL;
    }
    
    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;
    return view->wifi_switch;
}

void lisa_ui_setting_wifi_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_wifi_back_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }

    /* 设置返回按钮回调函数和用户数据 */
    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;
    view->back_cb = cb;
    view->back_user_data = user_data;
}

void lisa_ui_setting_wifi_view_set_item_cb(lv_obj_t *obj, lisa_ui_setting_wifi_item_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;
    view->item_cb = cb;
    view->item_user_data = user_data;
}

static void wifi_item_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)lv_event_get_user_data(e);

        if (view && view->item_cb) {
            /* 从 item 的 user_data 获取 SSID */
            const char *ssid = (const char *)lv_obj_get_user_data(item);
            if (ssid) {
                view->item_cb(ssid, view->item_user_data);
            }
        }
    }
}

/* 获取信号强度图标 */
static const char *get_signal_icon(int rssi)
{
    if (rssi >= -50) {
        return LV_SYMBOL_WIFI;  /* 强信号 */
    } else if (rssi >= -70) {
        return LV_SYMBOL_WIFI;  /* 中等信号 */
    } else {
        return LV_SYMBOL_WIFI;  /* 弱信号 */
    }
}

/* 创建单个WiFi列表项(优化版) */
static lv_obj_t *create_wifi_item(lisa_ui_setting_wifi_view_t *view, const lisa_ui_wifi_ap_info_t *ap_info)
{
    /* 创建列表项容器 */
    lv_obj_t *item = lv_obj_create(view->wifi_list);
    lv_obj_set_size(item, LV_PCT(100), 55);

    /* 使用预定义样式,避免重复设置 */
    lv_obj_add_style(item, &view->item_style, LV_PART_MAIN);
    lv_obj_add_style(item, &view->item_pressed_style, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);

    /* 保存 SSID 到 item 的 user_data (需要动态分配内存) */
    char *ssid_copy = lv_mem_alloc(strlen(ap_info->ssid) + 1);
    if (ssid_copy) {
        strcpy(ssid_copy, ap_info->ssid);
        lv_obj_set_user_data(item, ssid_copy);
    }

    /* 连接状态打勾标记 */
    if (ap_info->is_connected) {
        lv_obj_t *check_icon = lv_label_create(item);
        lv_label_set_text(check_icon, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(check_icon, lv_color_hex(0x4CAF50), LV_PART_MAIN);
        lv_obj_set_style_text_font(check_icon, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(check_icon, LV_ALIGN_LEFT_MID, 0, 0);
    }

    /* SSID 标签 - 统一左边距，保持对齐 */
    lv_obj_t *ssid_label = lv_label_create(item);
    lv_label_set_text(ssid_label, ap_info->ssid);
    lv_obj_set_style_text_font(ssid_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(ssid_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(ssid_label, 125);
    lv_obj_align(ssid_label, LV_ALIGN_LEFT_MID, 25, 0);

    /* 信号强度显示 */
    lv_obj_t *signal_label = lv_label_create(item);
    char signal_buf[16];

    /* 根据信号强度设置颜色 */
    lv_color_t signal_color;
    if (ap_info->rssi >= -50) {
        signal_color = lv_color_hex(0x4CAF50);  /* 强信号 - 绿色 */
    } else if (ap_info->rssi >= -70) {
        signal_color = lv_color_hex(0xFFC107);  /* 中等信号 - 黄色 */
    } else {
        signal_color = lv_color_hex(0xFF5722);  /* 弱信号 - 橙红色 */
    }

    snprintf(signal_buf, sizeof(signal_buf), "%ddBm", ap_info->rssi);
    lv_label_set_text(signal_label, signal_buf);
    lv_obj_set_style_text_font(signal_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(signal_label, signal_color, LV_PART_MAIN);
    lv_obj_align(signal_label, LV_ALIGN_RIGHT_MID, 0, 0);

    /* 注册点击事件 */
    lv_obj_add_event_cb(item, wifi_item_event_cb, LV_EVENT_CLICKED, view);

    return item;
}

/* 批量创建任务的定时器回调 */
typedef struct {
    lisa_ui_setting_wifi_view_t *view;
    const lisa_ui_wifi_ap_info_t *ap_list;
    int total_count;
    int current_index;
} wifi_list_create_data_t;

static void batch_create_timer_cb(lv_timer_t *timer)
{
    wifi_list_create_data_t *data = (wifi_list_create_data_t *)timer->user_data;

    if (!data || !data->view || !data->ap_list) {
        lv_timer_del(timer);
        if (data) {
            if (data->ap_list) lv_mem_free((void *)data->ap_list);
            lv_mem_free(data);
        }
        return;
    }

    /* 批量创建一部分列表项 */
    int end_index = data->current_index + WIFI_LIST_BATCH_CREATE_SIZE;
    if (end_index > data->total_count) {
        end_index = data->total_count;
    }

    for (int i = data->current_index; i < end_index; i++) {
        create_wifi_item(data->view, &data->ap_list[i]);
    }

    data->current_index = end_index;

    /* 如果所有项都已创建,删除定时器并释放内存 */
    if (data->current_index >= data->total_count) {
        LISA_UI_LOGD("Batch creation completed: %d items", data->total_count);
        lv_mem_free((void *)data->ap_list);
        lv_mem_free(data);
        lv_timer_del(timer);
    }
}

void lisa_ui_setting_wifi_view_update_list(lv_obj_t *obj, const lisa_ui_wifi_ap_info_t *ap_list, int ap_count)
{
    if (!obj || !ap_list || ap_count <= 0) {
        return;
    }

    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;

    /* 限流检查:避免过于频繁的更新 */
    uint32_t current_time = lv_tick_get();
    if ((current_time - view->last_update_time) < WIFI_LIST_UPDATE_THROTTLE_MS) {
        LISA_UI_LOGD("Update throttled, skipping");
        return;
    }
    view->last_update_time = current_time;

    /* 释放之前动态分配的内存 */
    uint32_t child_count = lv_obj_get_child_cnt(view->wifi_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(view->wifi_list, i);
        if (child) {
            char *ssid_copy = (char *)lv_obj_get_user_data(child);
            if (ssid_copy) {
                lv_mem_free(ssid_copy);
            }
        }
    }

    /* 清空现有列表 */
    lv_obj_clean(view->wifi_list);

    /* 隐藏提示信息 */
    lv_obj_add_flag(view->info_label, LV_OBJ_FLAG_HIDDEN);

    /* 性能优化:对于大量WiFi列表,使用批量创建 */
    if (ap_count > WIFI_LIST_BATCH_CREATE_SIZE * 2) {
        /* 先创建一批可见项,其余延迟创建 */
        int initial_count = WIFI_LIST_BATCH_CREATE_SIZE;
        for (int i = 0; i < initial_count && i < ap_count; i++) {
            create_wifi_item(view, &ap_list[i]);
        }

        /* 如果还有更多项,使用定时器批量创建 */
        if (ap_count > initial_count) {
            /* 复制AP列表数据 */
            lisa_ui_wifi_ap_info_t *ap_copy = lv_mem_alloc(sizeof(lisa_ui_wifi_ap_info_t) * ap_count);
            if (ap_copy) {
                memcpy(ap_copy, ap_list, sizeof(lisa_ui_wifi_ap_info_t) * ap_count);

                wifi_list_create_data_t *data = lv_mem_alloc(sizeof(wifi_list_create_data_t));
                if (data) {
                    data->view = view;
                    data->ap_list = ap_copy;
                    data->total_count = ap_count;
                    data->current_index = initial_count;

                    /* 创建定时器,每50ms创建一批 */
                    lv_timer_t *timer = lv_timer_create(batch_create_timer_cb, 50, data);
                    if (!timer) {
                        lv_mem_free(data);
                        lv_mem_free(ap_copy);
                        LISA_UI_LOGE("Failed to create batch timer");
                    }
                } else {
                    lv_mem_free(ap_copy);
                }
            }
        }

        LISA_UI_LOGD("Started batch creation for %d WiFi items", ap_count);
    } else {
        /* 对于少量WiFi,直接一次性创建 */
        for (int i = 0; i < ap_count; i++) {
            create_wifi_item(view, &ap_list[i]);
        }

        LISA_UI_LOGD("Updated WiFi list with %d items", ap_count);
    }
}

void lisa_ui_setting_wifi_view_clear_list(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;

    /* 释放之前动态分配的内存 */
    uint32_t child_count = lv_obj_get_child_cnt(view->wifi_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(view->wifi_list, i);
        if (child) {
            char *ssid_copy = (char *)lv_obj_get_user_data(child);
            if (ssid_copy) {
                lv_mem_free(ssid_copy);
            }
        }
    }

    /* 清空列表 */
    lv_obj_clean(view->wifi_list);

    /* 显示提示信息 */
    lv_label_set_text(view->info_label, _("no network"));
    lv_obj_clear_flag(view->info_label, LV_OBJ_FLAG_HIDDEN);

    LISA_UI_LOGD("Cleared WiFi list");
}

/* 密码输入确认回调的包装函数 */
static void password_confirm_wrapper(const char *text, void *user_data)
{
    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)user_data;

    if (view) {
        /* 恢复当前页面的显示 */
        lv_obj_clear_flag((lv_obj_t *)view, LV_OBJ_FLAG_HIDDEN);

        if (view->password_confirm_cb) {
            view->password_confirm_cb(view->current_ssid, text, view->password_user_data);
        }
    }
}

/* 密码输入取消回调的包装函数 */
static void password_cancel_wrapper(void *user_data)
{
    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)user_data;

    if (view) {
        /* 恢复当前页面的显示 */
        lv_obj_clear_flag((lv_obj_t *)view, LV_OBJ_FLAG_HIDDEN);

        if (view->password_cancel_cb) {
            view->password_cancel_cb(view->password_user_data);
        }
    }
}

void lisa_ui_setting_wifi_view_show_password_input(lv_obj_t *obj, const char *ssid,
                                                    lisa_ui_setting_wifi_password_confirm_cb_t confirm_cb,
                                                    lisa_ui_setting_wifi_password_cancel_cb_t cancel_cb,
                                                    void *user_data)
{
    if (!obj || !ssid) {
        return;
    }

    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;

    /* 保存回调函数和SSID */
    view->password_confirm_cb = confirm_cb;
    view->password_cancel_cb = cancel_cb;
    view->password_user_data = user_data;
    strncpy(view->current_ssid, ssid, sizeof(view->current_ssid) - 1);
    view->current_ssid[sizeof(view->current_ssid) - 1] = '\0';

    /* 如果键盘输入组件不存在，创建它 */
    if (!view->keyboard_input) {
        view->keyboard_input = lisa_ui_keyboard_input_create(lv_scr_act());
        if (!view->keyboard_input) {
            LISA_UI_LOGE("Failed to create keyboard input");
            return;
        }

        lisa_ui_keyboard_input_set_placeholder(view->keyboard_input, "Please input password");
        lisa_ui_keyboard_input_set_password_mode(view->keyboard_input, false);

        /* 设置回调 - 使用包装函数 */
        lisa_ui_keyboard_input_set_confirm_cb(view->keyboard_input, password_confirm_wrapper, view);
        lisa_ui_keyboard_input_set_cancel_cb(view->keyboard_input, password_cancel_wrapper, view);
    }

    /* 设置标题 */
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "%s", ssid);
    lisa_ui_keyboard_input_set_title(view->keyboard_input, title_text);

    /* 隐藏当前页面 */
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

    /* 显示键盘输入界面 */
    lisa_ui_keyboard_input_show(view->keyboard_input);

    LISA_UI_LOGD("Showing password input for SSID: %s", ssid);
}

void lisa_ui_setting_wifi_view_hide_password_input(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_wifi_view_t *view = (lisa_ui_setting_wifi_view_t *)obj;

    if (view->keyboard_input) {
        lisa_ui_keyboard_input_hide(view->keyboard_input);
        LISA_UI_LOGD("Password input hidden");
    }

    /* 恢复当前页面的显示 */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
