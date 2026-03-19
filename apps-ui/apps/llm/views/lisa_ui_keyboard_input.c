/**
 * @file lisa_ui_keyboard_input.c
 * @brief 通用键盘输入组件实现
 */

#include <stdio.h>
#include <string.h>
#include "lisa_ui_keyboard_input.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"

#define TAG "keyboard_input"

typedef struct {
    lv_obj_t *panel;                /* 全屏面板 */
    lv_obj_t *title_label;          /* 标题标签 */
    lv_obj_t *input_label;          /* 输入框标签 (如"密码:") */
    lv_obj_t *input;                /* 文本输入框 */
    lv_obj_t *keyboard;             /* 键盘 */

    lisa_ui_keyboard_input_confirm_cb_t confirm_cb;  /* 确认回调 */
    void *confirm_user_data;                          /* 确认回调用户数据 */
    lisa_ui_keyboard_input_cancel_cb_t cancel_cb;    /* 取消回调 */
    void *cancel_user_data;                           /* 取消回调用户数据 */

    bool switching_keyboard;        /* 是否正在切换键盘 */
} lisa_ui_keyboard_input_t;

/* 键盘按钮映射 - 小写字母 */
static const char *keyboard_map_lc[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "#+=", " ", " ", "enter", ""
};

/* 键盘按钮映射 - 大写字母 */
static const char *keyboard_map_uc[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "#+=", " ", " ", "enter", ""
};

/* 键盘按钮映射 - 符号 */
static const char *keyboard_map_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "-", "_", "=", "+", "[", "]", "{", "}", "\n",
    ";", ":", "'", "\"", ",", ".", "/", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "ABC", " ", " ", "enter", ""
};

/* 键盘事件处理 */
static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    lv_obj_t *keyboard = lv_event_get_target(e);
    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)lv_event_get_user_data(e);

    uint16_t btn_id = lv_btnmatrix_get_selected_btn(keyboard);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) {
        return;
    }

    const char *txt = lv_btnmatrix_get_btn_text(keyboard, btn_id);
    if (!txt) {
        return;
    }

    /* 处理删除键 */
    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_del_char(kb_input->input);
        return;
    }

    /* 处理确定按钮 */
    if (strcmp(txt, "enter") == 0) {
        if (kb_input->confirm_cb) {
            const char *text = lv_textarea_get_text(kb_input->input);
            kb_input->confirm_cb(text, kb_input->confirm_user_data);
        }
        return;
    }

    /* 处理键盘切换 */
    if (strcmp(txt, "abc") == 0) {
        lv_btnmatrix_set_map(keyboard, keyboard_map_lc);
        return;
    }

    if (strcmp(txt, "ABC") == 0) {
        lv_btnmatrix_set_map(keyboard, keyboard_map_uc);
        return;
    }

    if (strcmp(txt, "#+=") == 0) {
        lv_btnmatrix_set_map(keyboard, keyboard_map_special);
        return;
    }

    /* 处理普通字符输入 */
    lv_textarea_add_text(kb_input->input, txt);
}

/* 返回按钮事件处理 */
static void back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)lv_event_get_user_data(e);

    /* 调用取消回调 */
    if (kb_input->cancel_cb) {
        kb_input->cancel_cb(kb_input->cancel_user_data);
    }

    /* 隐藏界面 */
    lisa_ui_keyboard_input_hide((lv_obj_t *)kb_input);
}

lv_obj_t *lisa_ui_keyboard_input_create(lv_obj_t *parent)
{
    if (!parent) {
        parent = lv_scr_act();
    }

    /* 分配内存 */
    lisa_ui_keyboard_input_t *kb_input = lv_mem_alloc(sizeof(lisa_ui_keyboard_input_t));
    if (!kb_input) {
        LISA_UI_LOGE("Failed to allocate memory for keyboard input");
        return NULL;
    }

    memset(kb_input, 0, sizeof(lisa_ui_keyboard_input_t));
    kb_input->switching_keyboard = false;

    /* 创建全屏面板 */
    kb_input->panel = lv_obj_create(parent);
    lv_obj_set_size(kb_input->panel, LV_PCT(100), LV_PCT(100));
    lv_obj_align(kb_input->panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(kb_input->panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(kb_input->panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(kb_input->panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb_input->panel, 0, LV_PART_MAIN);
    lv_obj_add_flag(kb_input->panel, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 保存结构体指针到panel的user_data */
    lv_obj_set_user_data(kb_input->panel, kb_input);

    /* 创建返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(kb_input->panel);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back_btn, 0, LV_PART_MAIN);

    lv_obj_t *back_icon = lv_img_create(back_btn);
    lv_img_set_src(back_icon, &icons_icon_back_png);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(back_btn, back_event_cb, LV_EVENT_CLICKED, kb_input);

    /* 创建标题标签 */
    kb_input->title_label = lv_label_create(kb_input->panel);
    lv_obj_set_style_text_font(kb_input->title_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(kb_input->title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(kb_input->title_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(kb_input->title_label, "");

    /* 创建输入框容器 */
    lv_obj_t *input_container = lv_obj_create(kb_input->panel);
    lv_obj_set_size(input_container, LV_PCT(90), 40);
    lv_obj_align(input_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(input_container, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_color(input_container, lv_color_hex(0x4a4a4a), LV_PART_MAIN);
    lv_obj_set_style_border_width(input_container, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(input_container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(input_container, 10, LV_PART_MAIN);

    /* 创建输入框 */
    kb_input->input = lv_textarea_create(input_container);
    lv_obj_set_size(kb_input->input, LV_PCT(100), LV_PCT(100));
    lv_obj_align(kb_input->input, LV_ALIGN_CENTER, 0, 0);
    // lv_obj_set_style_text_font(kb_input->input, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(kb_input->input, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb_input->input, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(kb_input->input, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(kb_input->input, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb_input->input, 8, LV_PART_MAIN);
    lv_textarea_set_one_line(kb_input->input, true);
    lv_textarea_set_placeholder_text(kb_input->input, "please input");

    /* 输入框标签(保留变量以兼容set_label函数,但设为隐藏) */
    kb_input->input_label = lv_label_create(kb_input->panel);
    lv_obj_add_flag(kb_input->input_label, LV_OBJ_FLAG_HIDDEN);

    /* 创建键盘 */
    kb_input->keyboard = lv_btnmatrix_create(kb_input->panel);
    lv_obj_set_size(kb_input->keyboard, LV_PCT(100), 150);  /* 降低键盘高度从180到150 */
    lv_obj_align(kb_input->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb_input->keyboard, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_border_width(kb_input->keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb_input->keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(kb_input->keyboard, 2, LV_PART_MAIN);
    // lv_obj_set_style_text_font(kb_input->keyboard, &lv_font_chinese_16, LV_PART_MAIN);  /* 字体从18降到16 */

    /* 按钮样式 */
    lv_obj_set_style_bg_color(kb_input->keyboard, lv_color_hex(0x3a3a3a), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb_input->keyboard, lv_color_hex(0x5a5a5a), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb_input->keyboard, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_radius(kb_input->keyboard, 4, LV_PART_ITEMS);

    lv_btnmatrix_set_map(kb_input->keyboard, keyboard_map_lc);
    lv_obj_add_event_cb(kb_input->keyboard, keyboard_event_cb, LV_EVENT_PRESSED, kb_input);

    LISA_UI_LOGD("Keyboard input created");

    return (lv_obj_t *)kb_input;
}

void lisa_ui_keyboard_input_set_title(lv_obj_t *obj, const char *title)
{
    if (!obj || !title) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    lv_label_set_text(kb_input->title_label, title);
}

void lisa_ui_keyboard_input_set_placeholder(lv_obj_t *obj, const char *placeholder)
{
    if (!obj || !placeholder) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    lv_textarea_set_placeholder_text(kb_input->input, placeholder);
}

void lisa_ui_keyboard_input_set_label(lv_obj_t *obj, const char *label)
{
    if (!obj || !label) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    lv_label_set_text(kb_input->input_label, label);
}

void lisa_ui_keyboard_input_set_password_mode(lv_obj_t *obj, bool enable)
{
    if (!obj) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    lv_textarea_set_password_mode(kb_input->input, enable);
}

void lisa_ui_keyboard_input_set_confirm_cb(lv_obj_t *obj,
                                           lisa_ui_keyboard_input_confirm_cb_t confirm_cb,
                                           void *user_data)
{
    if (!obj) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    kb_input->confirm_cb = confirm_cb;
    kb_input->confirm_user_data = user_data;
}

void lisa_ui_keyboard_input_set_cancel_cb(lv_obj_t *obj,
                                          lisa_ui_keyboard_input_cancel_cb_t cancel_cb,
                                          void *user_data)
{
    if (!obj) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    kb_input->cancel_cb = cancel_cb;
    kb_input->cancel_user_data = user_data;
}

void lisa_ui_keyboard_input_show(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;

    /* 清空输入框 */
    lv_textarea_set_text(kb_input->input, "");

    /* 重置键盘为小写模式 */
    lv_btnmatrix_set_map(kb_input->keyboard, keyboard_map_lc);

    /* 显示面板 */
    lv_obj_clear_flag(kb_input->panel, LV_OBJ_FLAG_HIDDEN);

    LISA_UI_LOGD("Keyboard input shown");
}

void lisa_ui_keyboard_input_hide(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    lv_obj_add_flag(kb_input->panel, LV_OBJ_FLAG_HIDDEN);

    LISA_UI_LOGD("Keyboard input hidden");
}

const char *lisa_ui_keyboard_input_get_text(lv_obj_t *obj)
{
    if (!obj) {
        return "";
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    return lv_textarea_get_text(kb_input->input);
}

void lisa_ui_keyboard_input_clear(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lisa_ui_keyboard_input_t *kb_input = (lisa_ui_keyboard_input_t *)obj;
    lv_textarea_set_text(kb_input->input, "");
}
