
#include <string.h>
#include "lisa_ui.h"

#include "lisa_ui_llm_base.h"
#include "lisa_ui_llm_primary.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include "lisa_ui_anim_ext.h"

LV_IMG_DECLARE(icons_ic_status_full_duplex_png);
LV_IMG_DECLARE(icons_ic_status_alarm_png);

static void lisa_ui_llm_primary_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_llm_primary_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_llm_primary_t),
    .constructor_cb = lisa_ui_llm_primary_class_constructor,
};

static void lisa_ui_llm_primary_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj) 
{
    (void)class_p;
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (NULL == bar) {
        LISA_UI_LOGE("Failed to get base bar");
        return;
    }

    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (NULL == container) {
        LISA_UI_LOGE("Failed to get base container");
        return;
    }

    // 设置任务栏布局
    lv_obj_set_style_pad_left(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_min_height(bar, 36, LV_PART_MAIN);
    lv_obj_set_style_base_dir(bar, LV_BASE_DIR_LTR, LV_PART_MAIN);

#ifndef CONFIG_BOARD_ARCS_MINI
    // 创建设置图标（左边）
    llm_primary->settings_icon = lv_img_create(bar);
    lv_obj_add_flag(llm_primary->settings_icon, LV_OBJ_FLAG_CLICKABLE);
#endif

    // 创建WiFi图标（右边）
    llm_primary->wifi_icon = lv_img_create(bar);

    // 创建交互模式图标（全双工）
    llm_primary->full_duplex_icon = lv_img_create(bar);

    // 创建闹钟图标
    llm_primary->alarm_icon = lv_img_create(bar);

    // 创建电量图标
    llm_primary->battery_icon = lv_img_create(bar);

    // 创建状态文本标签（中间）
    llm_primary->status_label = lv_label_create(bar);

    // 设置主容器为垂直布局
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 创建emoji动画容器（上半部分）
    llm_primary->emoji_container = lv_obj_create(container);

    // 创建emoji动画图片
    llm_primary->emoji_anim = lisa_ui_anim_ext_create(llm_primary->emoji_container);

    // 创建拍照图片（覆盖在emoji上层，默认隐藏）
    llm_primary->img = lv_img_create(container);

    // 创建图片提示文本标签（覆盖在图片下方，默认隐藏）
    llm_primary->img_hint = lv_label_create(obj);

    // 创建内容文本容器（下半部分）
    llm_primary->content_container = lv_obj_create(container);

    // 创建内容文本标签
    llm_primary->content_label = lv_textarea_create(llm_primary->content_container);
}

// ===================== 公共API实现 =====================

lv_obj_t *lisa_ui_llm_primary_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_llm_primary_class, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

#ifndef CONFIG_BOARD_ARCS_MINI
    // 设置图标初始化和位置（左边）
    lv_img_set_src(llm_primary->settings_icon, &icons_ic_launch_setting_png);
    lv_obj_align(llm_primary->settings_icon, LV_ALIGN_LEFT_MID, 0, 0);
#endif
    
    // 设置WiFi图标位置（右边）
    lv_img_set_src(llm_primary->wifi_icon, &icons_ic_status_wifi_no_connect_png);
    lv_obj_align(llm_primary->wifi_icon, LV_ALIGN_LEFT_MID, 0, 0);

    // 设置交互模式图标（默认隐藏）
    lv_img_set_src(llm_primary->full_duplex_icon, &icons_ic_status_full_duplex_png);
    lv_obj_align(llm_primary->full_duplex_icon, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);

    // 设置闹钟图标（默认隐藏）
    lv_img_set_src(llm_primary->alarm_icon, &icons_ic_status_alarm_png);
    lv_obj_align(llm_primary->alarm_icon, LV_ALIGN_RIGHT_MID, -30, 0);
    lv_obj_add_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);

    // 设置电量图标（默认隐藏）
    lv_img_set_src(llm_primary->battery_icon, &icons_ic_status_power0_png);
    lv_obj_align(llm_primary->battery_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);

    // 设置状态文本标签
    lv_obj_set_style_text_letter_space(llm_primary->status_label, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(llm_primary->status_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->status_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(llm_primary->status_label, 0, LV_PART_MAIN);
    lv_obj_align(llm_primary->status_label, LV_ALIGN_CENTER, 0, 0);

    // 设置emoji动画容器
    lv_obj_set_style_bg_opa(llm_primary->emoji_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->emoji_container, 0, LV_PART_MAIN);
    lv_obj_set_size(llm_primary->emoji_container, LV_PCT(100), 104);
    lv_obj_set_style_pad_all(llm_primary->emoji_container, 0, LV_PART_MAIN);
#ifndef CONFIG_BOARD_ARCS_MINI
    lv_obj_set_style_pad_left(llm_primary->emoji_container, 55, LV_PART_MAIN); // 左边距50像素，表情右移30像素
    lv_obj_set_style_pad_right(llm_primary->emoji_container, 20, LV_PART_MAIN);
#endif
    lv_obj_update_layout(llm_primary->emoji_container);

    // 设置emoji容器为居中对齐
    lv_obj_set_layout(llm_primary->emoji_container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(llm_primary->emoji_container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(llm_primary->emoji_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(llm_primary->emoji_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 设置emoji动画图片
    lv_obj_set_size(llm_primary->emoji_anim, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
#ifdef CONFIG_BOARD_ARCS_MINI
    lv_obj_add_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_FLOATING);
    lv_obj_center(llm_primary->emoji_anim);
#endif

    lv_obj_set_size(llm_primary->img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_FLOATING);  /* Make img ignore flex layout from the start */

    lv_label_set_text(llm_primary->img_hint, "图片可在小聆AI小程序中查看");
    lv_obj_set_style_text_color(llm_primary->img_hint, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->img_hint, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->img_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(llm_primary->img_hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_move_foreground(llm_primary->img_hint);

    // 设置内容文本容器
    lv_obj_set_style_bg_opa(llm_primary->content_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->content_container, 0, LV_PART_MAIN);
    lv_obj_set_size(llm_primary->content_container, LV_PCT(100), 88);
    lv_obj_set_style_pad_all(llm_primary->content_container, 10, LV_PART_MAIN);
    
    // 设置内容容器为居中对齐
    lv_obj_set_layout(llm_primary->content_container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(llm_primary->content_container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(llm_primary->content_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(llm_primary->content_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 设置内容文本区域
    lv_textarea_set_text(llm_primary->content_label, "请唤醒我");
    lv_obj_set_style_text_color(llm_primary->content_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->content_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->content_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    
    // 隐藏边框和背景
    lv_obj_set_style_border_width(llm_primary->content_label, 0, LV_PART_MAIN);           // 隐藏边框
    lv_obj_set_style_bg_opa(llm_primary->content_label, LV_OPA_TRANSP, LV_PART_MAIN);     // 背景透明
    lv_obj_set_style_outline_width(llm_primary->content_label, 0, LV_PART_MAIN);          // 隐藏轮廓
    
    // 设置滚动和交互
    lv_textarea_set_cursor_click_pos(llm_primary->content_label, false);  // 禁用光标点击
    lv_obj_add_flag(llm_primary->content_label, LV_OBJ_FLAG_SCROLLABLE);  // 启用滚动
    lv_obj_set_scrollbar_mode(llm_primary->content_label, LV_SCROLLBAR_MODE_AUTO);  // 自动显示滚动条
    lv_obj_clear_flag(llm_primary->content_label, LV_OBJ_FLAG_CLICKABLE);  // 禁用点击
    
    // 设置尺寸和位置 - 使用更大的高度以便滚动
    lv_obj_set_width(llm_primary->content_label, LV_PCT(100));
    lv_obj_set_height(llm_primary->content_label, LV_PCT(100));
    
    // 调整文本样式
    lv_obj_set_style_text_line_space(llm_primary->content_label, 6, LV_PART_MAIN);    // 行间距（行与行之间）
    lv_obj_set_style_text_letter_space(llm_primary->content_label, 1, LV_PART_MAIN);  // 字符间距（字符与字符之间）
    lv_obj_set_style_pad_top(llm_primary->content_label, 20, LV_PART_MAIN);     // 上边距
    lv_obj_set_style_pad_bottom(llm_primary->content_label, 20, LV_PART_MAIN);  // 下边距
    lv_obj_set_style_pad_left(llm_primary->content_label, 12, LV_PART_MAIN);   // 左边距  
    lv_obj_set_style_pad_right(llm_primary->content_label, 12, LV_PART_MAIN);  // 右边距

    return obj;
}

#ifndef CONFIG_BOARD_ARCS_MINI
lv_obj_t *lisa_ui_llm_primary_settings_icon_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->settings_icon;
}
#endif

lv_obj_t *lisa_ui_llm_primary_wifi_icon_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->wifi_icon;
}

lv_obj_t *lisa_ui_llm_primary_battery_icon_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->battery_icon;
}

lv_obj_t *lisa_ui_llm_primary_status_label_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->status_label;
}

lv_obj_t *lisa_ui_llm_primary_content_label_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->content_label;
}

void lisa_ui_llm_primary_set_status_text(lv_obj_t *obj, const char *status)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->status_label) {
        lv_label_set_text(llm_primary->status_label, status);
    }
}

void lisa_ui_llm_primary_set_content_text(lv_obj_t *obj, const char *content)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->content_label) {
        lv_textarea_set_text(llm_primary->content_label, content);
    }
}

void lisa_ui_llm_primary_add_content_text(lv_obj_t *obj, const char *content)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->content_label) {
        lv_textarea_add_text(llm_primary->content_label, content);
    }
}

void lisa_ui_llm_primary_set_wifi_img(lv_obj_t *obj, const void *img_path)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->wifi_icon && img_path) {
        lv_img_set_src(llm_primary->wifi_icon, img_path);
    }
}

void lisa_ui_llm_primary_set_battery_img(lv_obj_t *obj, const void *img_path)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (!llm_primary->battery_icon) {
        return;
    }

    if (img_path) {
        lv_img_set_src(llm_primary->battery_icon, img_path);
        lv_obj_clear_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void lisa_ui_llm_primary_set_full_duplex_icon_visible(lv_obj_t *obj, bool visible)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->full_duplex_icon) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void lisa_ui_llm_primary_set_alarm_icon_visible(lv_obj_t *obj, bool visible)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->alarm_icon) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

#ifndef CONFIG_BOARD_ARCS_MINI
void lisa_ui_llm_primary_set_settings_icon_event_cb(lv_obj_t *obj, lv_event_cb_t event_cb, void *user_data)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (llm_primary->settings_icon && event_cb) {
        lv_obj_add_event_cb(llm_primary->settings_icon, event_cb, LV_EVENT_CLICKED, user_data);
    }
}
#endif

lv_obj_t *lisa_ui_llm_primary_emoji_anim_get(lv_obj_t *obj)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->emoji_anim;
}

static void camera_img_anim_ready_cb(lv_anim_t *a)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)a->user_data;

    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(llm_primary->img, LV_OPA_COVER, 0);  /* Reset opacity for next use */

    lv_obj_clear_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
}

static void camera_img_hide_timer_cb(lv_timer_t *timer)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)timer->user_data;

    lv_anim_t a;
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
    lv_timer_del(timer);
}

void lisa_ui_llm_primary_img_hide(lv_obj_t *obj)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_llm_primary_img_show(lv_obj_t *obj, void *img)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    bool enable_zoom = true;

    if (llm_primary->img_hint) {
        lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
    }
    if (llm_primary->content_label) {
        lv_textarea_set_text(llm_primary->content_label, "");
    }

    lv_img_set_src(llm_primary->img, img);

    /* URL网络图当前是LV_IMG_CF_RAW，很多平台在RAW路径不支持zoom变换 */
    if (lv_img_src_get_type(img) == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *dsc = (const lv_img_dsc_t *)img;
        if (dsc && (dsc->header.cf == LV_IMG_CF_RAW ||
                    dsc->header.cf == LV_IMG_CF_RAW_ALPHA ||
                    dsc->header.cf == LV_IMG_CF_RAW_CHROMA_KEYED)) {
            enable_zoom = false;
        }
    }

    /* 128 = 256 * 0.5 (50% scale) */
    lv_img_set_zoom(llm_primary->img, enable_zoom ? 128 : LV_IMG_ZOOM_NONE);

    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_FLOATING);

    lv_obj_update_layout(llm_primary->img);

    lv_obj_align(llm_primary->img, LV_ALIGN_CENTER, 0, -25);

    lv_obj_clear_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(llm_primary->img, LV_OPA_COVER, 0);  /* Ensure fully visible */

    lv_obj_add_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_llm_primary_img_hint_show(lv_obj_t *obj, const char *text)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->img_hint) {
        return;
    }

    lv_label_set_text(llm_primary->img_hint, text ? text : "");
    lv_obj_align(llm_primary->img_hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_move_foreground(llm_primary->img_hint);
    lv_obj_clear_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_llm_primary_img_hint_hide(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->img_hint) {
        return;
    }

    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
}
