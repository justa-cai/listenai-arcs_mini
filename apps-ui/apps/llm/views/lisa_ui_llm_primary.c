
#include <string.h>
#include "lisa_ui.h"

#include "lisa_ui_llm_base.h"
#include "lisa_ui_llm_primary.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include "lisa_ui_anim_ext.h"

LV_IMG_DECLARE(icons_ic_status_duplex_interruptible_png);
LV_IMG_DECLARE(icons_ic_status_duplex_non_interruptible_png);
LV_IMG_DECLARE(icons_ic_status_tf_card_png);
LV_IMG_DECLARE(icons_ic_status_usb_png);
LV_IMG_DECLARE(icons_ic_status_alarm_png);
LV_IMG_DECLARE(icons_icon_finger_png);

#define LISA_UI_EMOJI_DEFAULT_OFFSET_Y      (-10)
#define LISA_UI_CONTENT_TEXT_BOTTOM_MARGIN  12
#define LISA_UI_CONTENT_CONTAINER_PAD       10
#define LISA_UI_CONTENT_TEXT_TOP_PAD        20
#define LISA_UI_CONTENT_TEXT_BOTTOM_PAD     2
#define LISA_UI_CONTENT_TEXT_HORIZONTAL_PAD 12
#define LISA_UI_CONTENT_TEXT_LINE_COUNT     2
#define LISA_UI_CONTENT_TEXT_TOP_CLIP       3

#define LISA_UI_STATUS_USB_BATTERY_GAP     4

#define LISA_UI_STATUS_ICON_WIFI          (1U << 0)
#define LISA_UI_STATUS_ICON_DUPLEX        (1U << 1)
#define LISA_UI_STATUS_ICON_TF            (1U << 2)
#define LISA_UI_STATUS_ICON_USB           (1U << 3)
#define LISA_UI_STATUS_ICON_ALARM         (1U << 4)
#define LISA_UI_STATUS_ICON_BATTERY       (1U << 5)
#define LISA_UI_STATUS_ICON_ALL           (LISA_UI_STATUS_ICON_WIFI | \
                                           LISA_UI_STATUS_ICON_DUPLEX | \
                                           LISA_UI_STATUS_ICON_TF | \
                                           LISA_UI_STATUS_ICON_USB | \
                                           LISA_UI_STATUS_ICON_ALARM | \
                                           LISA_UI_STATUS_ICON_BATTERY)

#if defined(CONFIG_BOARD_ARCS_MINI3) && CONFIG_BOARD_ARCS_MINI3
#define POWER_KEY_HINT_SIZE 20
static uint8_t s_power_key_hint_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(
    POWER_KEY_HINT_SIZE, POWER_KEY_HINT_SIZE)];
#endif

static void lisa_ui_llm_primary_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_llm_primary_align_usb_icon(lisa_ui_llm_primary_t *llm_primary);
static void lisa_ui_llm_primary_align_status_icon_container(lisa_ui_llm_primary_t *llm_primary);
static void lisa_ui_llm_primary_reorder_status_icons(lisa_ui_llm_primary_t *llm_primary);
static uint32_t lisa_ui_llm_primary_status_icons_visibility_get(
    const lisa_ui_llm_primary_t *llm_primary);
static void lisa_ui_llm_primary_status_icons_apply_visibility(
    lisa_ui_llm_primary_t *llm_primary);

lv_obj_t *lisa_ui_llm_primary_power_key_hint_create(lv_obj_t *parent)
{
#if !defined(CONFIG_BOARD_ARCS_MINI3) || !CONFIG_BOARD_ARCS_MINI3
    LV_UNUSED(parent);
    return NULL;
#else
    lv_obj_t *canvas;
    lv_draw_rect_dsc_t draw_dsc;
    lv_point_t points[3] = {
        {POWER_KEY_HINT_SIZE - 1, 0},
        {POWER_KEY_HINT_SIZE - 1, POWER_KEY_HINT_SIZE - 1},
        {0, 0},
    };

    if (!parent) {
        return NULL;
    }

    canvas = lv_canvas_create(parent);
    if (!canvas) {
        return NULL;
    }

    lv_canvas_set_buffer(canvas, s_power_key_hint_buf,
                         POWER_KEY_HINT_SIZE, POWER_KEY_HINT_SIZE,
                         LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = lv_color_white();
    draw_dsc.bg_opa = LV_OPA_COVER;
    lv_canvas_draw_polygon(canvas, points, 3, &draw_dsc);

    lv_obj_set_size(canvas, POWER_KEY_HINT_SIZE, POWER_KEY_HINT_SIZE);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_align(canvas, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_move_foreground(canvas);
    return canvas;
#endif
}

const lv_obj_class_t lisa_ui_llm_primary_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_llm_primary_t),
    .constructor_cb = lisa_ui_llm_primary_class_constructor,
};

static void lisa_ui_llm_primary_align_usb_icon(lisa_ui_llm_primary_t *llm_primary)
{
    lisa_ui_llm_primary_align_status_icon_container(llm_primary);
}

static void lisa_ui_llm_primary_align_status_icon_container(lisa_ui_llm_primary_t *llm_primary)
{
    if (!llm_primary || !llm_primary->status_icon_container || !llm_primary->battery_icon) {
        return;
    }

    lv_obj_update_layout(llm_primary->status_icon_container);
    lv_obj_update_layout(llm_primary->battery_icon);
    if (lv_obj_has_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_align(llm_primary->status_icon_container, LV_ALIGN_RIGHT_MID, 0, 0);
    } else {
        lv_obj_align_to(llm_primary->status_icon_container, llm_primary->battery_icon,
                        LV_ALIGN_OUT_LEFT_MID, -LISA_UI_STATUS_USB_BATTERY_GAP, 0);
    }
}

static void lisa_ui_llm_primary_reorder_status_icons(lisa_ui_llm_primary_t *llm_primary)
{
    if (!llm_primary || !llm_primary->status_icon_container) {
        return;
    }

    bool usb_visible = !lv_obj_has_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
    bool alarm_visible = !lv_obj_has_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    if (usb_visible && alarm_visible) {
        if (llm_primary->status_icons_usb_first) {
            lv_obj_move_to_index(llm_primary->alarm_icon, 0);
            lv_obj_move_to_index(llm_primary->usb_icon, 1);
        } else {
            lv_obj_move_to_index(llm_primary->usb_icon, 0);
            lv_obj_move_to_index(llm_primary->alarm_icon, 1);
        }
    } else if (usb_visible) {
        lv_obj_move_to_index(llm_primary->usb_icon, 0);
    } else if (alarm_visible) {
        lv_obj_move_to_index(llm_primary->alarm_icon, 0);
    }
}

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

    // 创建 TF 卡图标
    llm_primary->tf_card_icon = lv_img_create(bar);

    llm_primary->status_icon_container = lv_obj_create(bar);
    lv_obj_set_size(llm_primary->status_icon_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(llm_primary->status_icon_container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(llm_primary->status_icon_container, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(llm_primary->status_icon_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(llm_primary->status_icon_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(llm_primary->status_icon_container,
                                LISA_UI_STATUS_USB_BATTERY_GAP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(llm_primary->status_icon_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->status_icon_container, 0, LV_PART_MAIN);

    // 创建 USB Host 图标
    llm_primary->usb_icon = lv_img_create(llm_primary->status_icon_container);

    // 创建闹钟图标
    llm_primary->alarm_icon = lv_img_create(llm_primary->status_icon_container);

    // 创建电量图标
    llm_primary->battery_icon = lv_img_create(bar);

    // 创建状态文本标签（中间）
    llm_primary->status_label = lv_label_create(bar);

    // 设置主容器为垂直布局
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 创建emoji动画图片（挂在页面根对象上，作为屏幕底层）
    llm_primary->emoji_anim = lisa_ui_anim_ext_create(obj);

    // 创建emoji动画容器（保留用于兼容，但不影响表情定位）
    llm_primary->emoji_container = lv_obj_create(container);

    // 创建拍照图片（覆盖在emoji上层，默认隐藏）
    llm_primary->img = lv_img_create(container);

    // 创建图片提示文本标签（覆盖在图片下方，默认隐藏）
    llm_primary->img_hint = lv_label_create(obj);

    // 创建左下角操作提示图标和文本（默认隐藏）
    llm_primary->finger_hint_icon = lv_img_create(obj);
    llm_primary->finger_hint_label = lv_label_create(obj);
#ifdef CONFIG_BOARD_ARCS_MINI3
    /* Keep the hint at the physical top-right corner. */
    llm_primary->power_key_hint = lisa_ui_llm_primary_power_key_hint_create(obj);
    if (llm_primary->power_key_hint) {
        lv_obj_align(llm_primary->power_key_hint, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
#endif

    // 创建内容文本容器（下半部分）
    llm_primary->content_container = lv_obj_create(container);

    // 创建内容文本标签
    llm_primary->content_label = lv_textarea_create(llm_primary->content_container);

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
#endif
}

// ===================== 公共API实现 =====================

lv_obj_t *lisa_ui_llm_primary_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_llm_primary_class, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    llm_primary->status_icons_usb_first = false;
    llm_primary->status_icons_visibility = LISA_UI_STATUS_ICON_ALL;
    llm_primary->status_icons_suppressed = false;

#ifndef CONFIG_BOARD_ARCS_MINI
    // 设置图标初始化和位置（左边）
    lv_img_set_src(llm_primary->settings_icon, &icons_ic_launch_setting_png);
    lv_obj_align(llm_primary->settings_icon, LV_ALIGN_LEFT_MID, 0, 0);
#endif
    
    // 设置WiFi图标位置（右边）
    lv_img_set_src(llm_primary->wifi_icon, &icons_ic_status_wifi_no_connect_png);
    lv_obj_align(llm_primary->wifi_icon, LV_ALIGN_LEFT_MID, 0, 0);

    // 设置交互模式图标（默认隐藏）
    lv_img_set_src(llm_primary->full_duplex_icon, &icons_ic_status_duplex_interruptible_png);
    lv_obj_align(llm_primary->full_duplex_icon, LV_ALIGN_LEFT_MID, 26, 0);
    lv_obj_add_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);

    // TF 卡图标位于左侧状态图标组最右侧，默认隐藏
    lv_img_set_src(llm_primary->tf_card_icon, &icons_ic_status_tf_card_png);
    lv_obj_align(llm_primary->tf_card_icon, LV_ALIGN_LEFT_MID, 52, 0);
    lv_obj_add_flag(llm_primary->tf_card_icon, LV_OBJ_FLAG_HIDDEN);

    lv_img_set_src(llm_primary->usb_icon, &icons_ic_status_usb_png);
    lv_obj_add_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);

    // 设置闹钟图标（默认隐藏）
    lv_img_set_src(llm_primary->alarm_icon, &icons_ic_status_alarm_png);
    lv_obj_add_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);

    // 设置电量图标（默认隐藏）
    lv_img_set_src(llm_primary->battery_icon, &icons_ic_status_power0_png);
    lv_obj_align(llm_primary->battery_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);

    // 设置状态文本标签
    lv_obj_set_style_text_letter_space(llm_primary->status_label, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(llm_primary->status_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->status_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(llm_primary->status_label, 0, LV_PART_MAIN);
    lv_obj_align(llm_primary->status_label, LV_ALIGN_CENTER, 0, 0);
    lisa_ui_llm_primary_align_usb_icon(llm_primary);

    // 设置emoji动画图片（在页面根对象上，使用FLOATING脱离布局）
    lv_obj_set_size(llm_primary->emoji_anim, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_FLOATING);  // 跳出页面 flex 布局
    lv_obj_align(llm_primary->emoji_anim, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_background(llm_primary->emoji_anim);  // 移到屏幕底层

    // 设置emoji动画容器（保留兼容，隐藏不使用）
    lv_obj_set_style_bg_opa(llm_primary->emoji_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->emoji_container, 0, LV_PART_MAIN);
    lv_obj_set_size(llm_primary->emoji_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(llm_primary->emoji_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_grow(llm_primary->emoji_container, 1);
    lv_obj_add_flag(llm_primary->emoji_container, LV_OBJ_FLAG_HIDDEN);  // 隐藏，不影响表情显示

    lv_obj_set_size(llm_primary->img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_FLOATING);  /* Make img ignore flex layout from the start */

    lv_label_set_text(llm_primary->img_hint, "图片可在小聆AI小程序中查看");
    lv_obj_set_style_text_color(llm_primary->img_hint, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->img_hint, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->img_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(llm_primary->img_hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_move_foreground(llm_primary->img_hint);

    lv_img_set_src(llm_primary->finger_hint_icon, &icons_icon_finger_png);
    lv_obj_add_flag(llm_primary->finger_hint_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->finger_hint_icon, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_align(llm_primary->finger_hint_icon, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_move_foreground(llm_primary->finger_hint_icon);

    lv_label_set_text(llm_primary->finger_hint_label, "");
    lv_obj_set_style_text_color(llm_primary->finger_hint_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->finger_hint_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->finger_hint_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(llm_primary->finger_hint_label, LV_PCT(70));
    lv_label_set_long_mode(llm_primary->finger_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(llm_primary->finger_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->finger_hint_label, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_align_to(llm_primary->finger_hint_label, llm_primary->finger_hint_icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_move_foreground(llm_primary->finger_hint_label);

#ifdef CONFIG_BOARD_ARCS_MINI3
    if (llm_primary->power_key_hint) {
        lv_obj_add_flag(llm_primary->power_key_hint, LV_OBJ_FLAG_HIDDEN);
    }
#endif

    // 设置内容文本容器（放在屏幕底部，不遮挡表情）
    lv_coord_t content_text_height =
        lv_font_chinese_16.line_height * LISA_UI_CONTENT_TEXT_LINE_COUNT +
        LISA_UI_CONTENT_TEXT_TOP_PAD +
        LISA_UI_CONTENT_TEXT_BOTTOM_PAD +
        LISA_UI_CONTENT_CONTAINER_PAD * 2 -
        LISA_UI_CONTENT_TEXT_TOP_CLIP;
    lv_obj_set_style_bg_opa(llm_primary->content_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->content_container, 0, LV_PART_MAIN);
    lv_obj_set_size(llm_primary->content_container, LV_PCT(100), content_text_height);
    lv_obj_set_style_pad_all(llm_primary->content_container, LISA_UI_CONTENT_CONTAINER_PAD, LV_PART_MAIN);
    lv_obj_add_flag(llm_primary->content_container, LV_OBJ_FLAG_FLOATING);  // 跳出 flex 布局
    lv_obj_align(llm_primary->content_container, LV_ALIGN_BOTTOM_MID, 0,
                 -LISA_UI_CONTENT_TEXT_BOTTOM_MARGIN);

    // 设置内容容器内部居中
    lv_obj_set_layout(llm_primary->content_container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(llm_primary->content_container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(llm_primary->content_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(llm_primary->content_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 设置内容文本区域
    lv_textarea_set_text(llm_primary->content_label, "请唤醒我");
    lv_obj_set_style_text_color(llm_primary->content_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->content_label, &lv_font_chinese_16, LV_PART_MAIN);
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
    lv_obj_set_style_text_line_space(llm_primary->content_label, 0, LV_PART_MAIN);    // 使用字体自身line_height
    lv_obj_set_style_text_letter_space(llm_primary->content_label, 1, LV_PART_MAIN);  // 字符间距（字符与字符之间）
    lv_obj_set_style_pad_top(llm_primary->content_label, LISA_UI_CONTENT_TEXT_TOP_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(llm_primary->content_label, LISA_UI_CONTENT_TEXT_BOTTOM_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_left(llm_primary->content_label, LISA_UI_CONTENT_TEXT_HORIZONTAL_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_right(llm_primary->content_label, LISA_UI_CONTENT_TEXT_HORIZONTAL_PAD, LV_PART_MAIN);

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
        lv_label_set_text(llm_primary->status_label, status ? status : "");
        lisa_ui_llm_primary_align_usb_icon(llm_primary);
    }
}

void lisa_ui_llm_primary_set_content_text(lv_obj_t *obj, const char *content)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->content_label) {
        lv_textarea_set_text(llm_primary->content_label, content ? content : "");
    }
}

void lisa_ui_llm_primary_add_content_text(lv_obj_t *obj, const char *content)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->content_label) {
        lv_textarea_add_text(llm_primary->content_label, content ? content : "");
    }
}

void lisa_ui_llm_primary_set_wifi_img(lv_obj_t *obj, const void *img_path)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (!llm_primary->wifi_icon || !img_path) {
        return;
    }

    lv_img_set_src(llm_primary->wifi_icon, img_path);

    if (llm_primary->status_icons_suppressed) {
        llm_primary->status_icons_visibility |= LISA_UI_STATUS_ICON_WIFI;
        lv_obj_add_flag(llm_primary->wifi_icon, LV_OBJ_FLAG_HIDDEN);
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
    }

    if (llm_primary->status_icons_suppressed) {
        if (img_path) {
            llm_primary->status_icons_visibility |= LISA_UI_STATUS_ICON_BATTERY;
        } else {
            llm_primary->status_icons_visibility &= ~LISA_UI_STATUS_ICON_BATTERY;
        }
        lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    } else if (img_path) {
        lv_obj_clear_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    }
    lisa_ui_llm_primary_align_status_icon_container(llm_primary);
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

    if (llm_primary->status_icons_suppressed) {
        if (visible) {
            llm_primary->status_icons_visibility |= LISA_UI_STATUS_ICON_DUPLEX;
        } else {
            llm_primary->status_icons_visibility &= ~LISA_UI_STATUS_ICON_DUPLEX;
        }
        lv_obj_add_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (visible) {
        lv_obj_clear_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void lisa_ui_llm_primary_set_full_duplex_icon_img(lv_obj_t *obj, const void *img_path)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->full_duplex_icon || !img_path) {
        return;
    }

    lv_img_set_src(llm_primary->full_duplex_icon, img_path);
}

void lisa_ui_llm_primary_set_tf_card_icon_visible(lv_obj_t *obj, bool visible)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->tf_card_icon) {
        return;
    }

    if (llm_primary->status_icons_suppressed) {
        if (visible) {
            llm_primary->status_icons_visibility |= LISA_UI_STATUS_ICON_TF;
        } else {
            llm_primary->status_icons_visibility &= ~LISA_UI_STATUS_ICON_TF;
        }
        lv_obj_add_flag(llm_primary->tf_card_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (visible) {
        lv_obj_clear_flag(llm_primary->tf_card_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->tf_card_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void lisa_ui_llm_primary_set_usb_icon_visible(lv_obj_t *obj, bool visible)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->usb_icon) {
        return;
    }

    if (llm_primary->status_icons_suppressed) {
        if (visible) {
            llm_primary->status_icons_visibility |= LISA_UI_STATUS_ICON_USB;
        } else {
            llm_primary->status_icons_visibility &= ~LISA_UI_STATUS_ICON_USB;
        }
        lv_obj_add_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    bool was_hidden = lv_obj_has_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
    bool alarm_visible = !lv_obj_has_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    if (visible) {
        if (was_hidden) {
            llm_primary->status_icons_usb_first = !alarm_visible;
        }
        lv_obj_clear_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
    }

    lisa_ui_llm_primary_reorder_status_icons(llm_primary);
    lisa_ui_llm_primary_align_status_icon_container(llm_primary);
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

    if (llm_primary->status_icons_suppressed) {
        if (visible) {
            llm_primary->status_icons_visibility |= LISA_UI_STATUS_ICON_ALARM;
        } else {
            llm_primary->status_icons_visibility &= ~LISA_UI_STATUS_ICON_ALARM;
        }
        lv_obj_add_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    bool was_hidden = lv_obj_has_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    bool usb_visible = !lv_obj_has_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
    if (visible) {
        if (was_hidden) {
            llm_primary->status_icons_usb_first = usb_visible;
        }
        lv_obj_clear_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
    }
    lisa_ui_llm_primary_reorder_status_icons(llm_primary);
    lisa_ui_llm_primary_align_status_icon_container(llm_primary);
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

void lisa_ui_llm_primary_set_emoji_offset(lv_obj_t *obj, int offset_x, int offset_y)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (llm_primary->emoji_anim == NULL) {
        return;
    }

    // 表情中心点相对于整个屏幕中心点偏移
    // 横向：正数向右，负数向左
    // 纵向：默认上移10px，配置正数向下、负数向上
    lv_obj_align(llm_primary->emoji_anim, LV_ALIGN_CENTER, offset_x,
                 offset_y + LISA_UI_EMOJI_DEFAULT_OFFSET_Y);
}

static void camera_img_anim_ready_cb(lv_anim_t *a)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)a->user_data;

    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(llm_primary->img, LV_OPA_COVER, 0);  /* Reset opacity for next use */

    lv_obj_clear_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
#ifndef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_obj_clear_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
#endif
}

static void camera_img_hide_timer_cb(lv_timer_t *timer)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)timer->user_data;

    lv_anim_t a;
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
#ifndef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_obj_clear_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
#endif
    lv_timer_del(timer);
}

void lisa_ui_llm_primary_img_hide(lv_obj_t *obj)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->finger_hint_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->finger_hint_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
#ifndef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_obj_clear_flag(llm_primary->content_container, LV_OBJ_FLAG_HIDDEN);
#endif
}

void lisa_ui_llm_primary_img_show(lv_obj_t *obj, void *img)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    bool enable_zoom = true;

    if (llm_primary->img_hint) {
        lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
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

    lv_obj_align(llm_primary->img, LV_ALIGN_CENTER, 0, -40);

    lv_obj_clear_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(llm_primary->img, LV_OPA_COVER, 0);  /* Ensure fully visible */

    lv_obj_add_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_llm_primary_query_img_show(lv_obj_t *obj, const void *img)
{
    if (!lisa_ui_llm_primary_is_valid(obj) || !img) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->img_hint) {
        lv_obj_add_flag(llm_primary->img_hint, LV_OBJ_FLAG_HIDDEN);
    }

    lv_img_set_src(llm_primary->img, img);
    lv_img_set_zoom(llm_primary->img, LV_IMG_ZOOM_NONE);
    lv_obj_add_flag(llm_primary->img, LV_OBJ_FLAG_FLOATING);
    lv_obj_update_layout(llm_primary->img);
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_obj_align(llm_primary->img, LV_ALIGN_CENTER, 0, 0);
#else
    lv_obj_align(llm_primary->img, LV_ALIGN_CENTER, 0, -40);
#endif
    lv_obj_clear_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(llm_primary->img, LV_OPA_COVER, 0);

    lv_obj_add_flag(llm_primary->emoji_anim, LV_OBJ_FLAG_HIDDEN);
}

bool lisa_ui_llm_primary_img_is_visible(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return false;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->img && !lv_obj_has_flag(llm_primary->img, LV_OBJ_FLAG_HIDDEN);
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

void lisa_ui_llm_primary_finger_hint_show(lv_obj_t *obj, const char *text)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->finger_hint_icon || !llm_primary->finger_hint_label) {
        return;
    }

    lv_label_set_text(llm_primary->finger_hint_label, text ? text : "");
    lv_obj_align(llm_primary->finger_hint_icon, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_align_to(llm_primary->finger_hint_label, llm_primary->finger_hint_icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_move_foreground(llm_primary->finger_hint_icon);
    lv_obj_move_foreground(llm_primary->finger_hint_label);
    lv_obj_clear_flag(llm_primary->finger_hint_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(llm_primary->finger_hint_label, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_llm_primary_finger_hint_hide(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->finger_hint_icon || !llm_primary->finger_hint_label) {
        return;
    }

    lv_obj_add_flag(llm_primary->finger_hint_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->finger_hint_label, LV_OBJ_FLAG_HIDDEN);
}

static uint32_t lisa_ui_llm_primary_status_icons_visibility_get(
    const lisa_ui_llm_primary_t *llm_primary)
{
    uint32_t visibility = 0;

    if (!llm_primary) {
        return visibility;
    }

    if (llm_primary->wifi_icon &&
        !lv_obj_has_flag(llm_primary->wifi_icon, LV_OBJ_FLAG_HIDDEN)) {
        visibility |= LISA_UI_STATUS_ICON_WIFI;
    }
    if (llm_primary->full_duplex_icon &&
        !lv_obj_has_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN)) {
        visibility |= LISA_UI_STATUS_ICON_DUPLEX;
    }
    if (llm_primary->tf_card_icon &&
        !lv_obj_has_flag(llm_primary->tf_card_icon, LV_OBJ_FLAG_HIDDEN)) {
        visibility |= LISA_UI_STATUS_ICON_TF;
    }
    if (llm_primary->usb_icon &&
        !lv_obj_has_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN)) {
        visibility |= LISA_UI_STATUS_ICON_USB;
    }
    if (llm_primary->alarm_icon &&
        !lv_obj_has_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN)) {
        visibility |= LISA_UI_STATUS_ICON_ALARM;
    }
    if (llm_primary->battery_icon &&
        !lv_obj_has_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN)) {
        visibility |= LISA_UI_STATUS_ICON_BATTERY;
    }

    return visibility;
}

static void lisa_ui_llm_primary_status_icons_apply_visibility(
    lisa_ui_llm_primary_t *llm_primary)
{
    if (!llm_primary) {
        return;
    }

#define APPLY_STATUS_ICON_FLAG(icon, mask) \
    do { \
        if (llm_primary->icon) { \
            if ((llm_primary->status_icons_visibility & (mask)) != 0U) { \
                lv_obj_clear_flag(llm_primary->icon, LV_OBJ_FLAG_HIDDEN); \
            } else { \
                lv_obj_add_flag(llm_primary->icon, LV_OBJ_FLAG_HIDDEN); \
            } \
        } \
    } while (0)

    APPLY_STATUS_ICON_FLAG(wifi_icon, LISA_UI_STATUS_ICON_WIFI);
    APPLY_STATUS_ICON_FLAG(full_duplex_icon, LISA_UI_STATUS_ICON_DUPLEX);
    APPLY_STATUS_ICON_FLAG(tf_card_icon, LISA_UI_STATUS_ICON_TF);
    APPLY_STATUS_ICON_FLAG(usb_icon, LISA_UI_STATUS_ICON_USB);
    APPLY_STATUS_ICON_FLAG(alarm_icon, LISA_UI_STATUS_ICON_ALARM);
    APPLY_STATUS_ICON_FLAG(battery_icon, LISA_UI_STATUS_ICON_BATTERY);

#undef APPLY_STATUS_ICON_FLAG
    lisa_ui_llm_primary_reorder_status_icons(llm_primary);
    lisa_ui_llm_primary_align_status_icon_container(llm_primary);
}

void lisa_ui_llm_primary_set_status_icons_visible(lv_obj_t *obj, bool visible)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!visible) {
        if (!llm_primary->status_icons_suppressed) {
            llm_primary->status_icons_visibility =
                lisa_ui_llm_primary_status_icons_visibility_get(llm_primary);
            llm_primary->status_icons_suppressed = true;
        }

        if (llm_primary->wifi_icon) {
            lv_obj_add_flag(llm_primary->wifi_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (llm_primary->full_duplex_icon) {
            lv_obj_add_flag(llm_primary->full_duplex_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (llm_primary->tf_card_icon) {
            lv_obj_add_flag(llm_primary->tf_card_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (llm_primary->usb_icon) {
            lv_obj_add_flag(llm_primary->usb_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (llm_primary->alarm_icon) {
            lv_obj_add_flag(llm_primary->alarm_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (llm_primary->battery_icon) {
            lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
        }
        lisa_ui_llm_primary_align_status_icon_container(llm_primary);
        return;
    }

    if (!llm_primary->status_icons_suppressed) {
        return;
    }

    llm_primary->status_icons_suppressed = false;
    lisa_ui_llm_primary_status_icons_apply_visibility(llm_primary);
}

void lisa_ui_llm_primary_finger_hint_icon_hide(lv_obj_t *obj)
{
#ifndef CONFIG_BOARD_ARCS_MINI3
    LV_UNUSED(obj);
    return;
#else
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (llm_primary->finger_hint_icon) {
        lv_obj_add_flag(llm_primary->finger_hint_icon, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

void lisa_ui_llm_primary_power_key_hint_show(lv_obj_t *obj)
{
#ifndef CONFIG_BOARD_ARCS_MINI3
    LV_UNUSED(obj);
    return;
#else
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (llm_primary->power_key_hint) {
        lv_obj_clear_flag(llm_primary->power_key_hint, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(llm_primary->power_key_hint);
    }
#endif
}

void lisa_ui_llm_primary_power_key_hint_hide(lv_obj_t *obj)
{
#ifndef CONFIG_BOARD_ARCS_MINI3
    LV_UNUSED(obj);
    return;
#else
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (llm_primary->power_key_hint) {
        lv_obj_add_flag(llm_primary->power_key_hint, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}
