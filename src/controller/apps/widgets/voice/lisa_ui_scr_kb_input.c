#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_setting_net.h"
#include "lisa_ui_bar.h"
#include "lisa_ui_sys_bar.h"

struct lisa_ui_scr_kb_input {
    lisa_ui_scr_base_t base;
    lv_obj_t *input_ta;
    lv_obj_t *kb_tbv;
    lv_obj_t *title;
    lv_obj_t *btn_mtx;
    lv_obj_t *btn_back;
};

typedef struct lisa_ui_scr_kb_input lisa_ui_scr_kb_input_t;

static void lisa_ui_scr_kb_input_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
const lv_obj_class_t lv_lisa_ui_scr_kb_input_class = {
    .constructor_cb = lisa_ui_scr_kb_input_constructor,
    .base_class = &lv_obj_class,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .instance_size = sizeof(lisa_ui_scr_kb_input_t),
};

#define MY_CLASS &lv_lisa_ui_scr_kb_input_class

static const char * btnm_map_num[] = {
    "1", "2", "3", "4", "5","6", "7", "8", "9", "0", "\n",
    "-", "/", ":", ";", "(", ")", "_", "@", "&", "\n",
    "~", ",", ".", "?", "'", "!","%", "DEL", "\n",
    "abc", "#+-", " ", "GO", ""
};

static const char * btnm_map_spec[] = {
    "[", "]", "{", "}", "<",">", "^", "*", "+", "=", "\n",
    "-", "\\", "|", "#", "(", ")", "%", "\"", ":", "\n",
    "~", ",", ".", "?", "'", "!","%", "DEL", "\n",
    "abc", "123", " ", "GO", ""
};

static const char * btnm_map_lc[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "z", "x", "c", "v", "b", "n", "m", "DEL", "\n",
    "123", "a/A", " ", "GO", ""
};

static const char * btnm_map_uc[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", "DEL", "\n",
    "123", "A/a", " ", "GO", ""
};

static void lisa_ui_scr_kb_input_value_changed_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);
    uint16_t btn_id   = lv_btnmatrix_get_selected_btn(obj);
    lisa_ui_scr_kb_input_t *scr_kb_input = lv_obj_get_user_data(obj);

    if (btn_id == LV_BTNMATRIX_BTN_NONE) {
        return;
    }

    const char *txt = lv_btnmatrix_get_btn_text(obj, btn_id);
    if (txt == NULL) {
        return;
    }

    if (strcmp(txt, "DEL") == 0) {
        lv_textarea_del_char(scr_kb_input->input_ta);
    } else if (strcmp(txt, "GO") == 0) {
        lv_event_send((lv_obj_t *)scr_kb_input, LV_EVENT_READY, NULL);
    } else if (strcmp(txt, "A/a") == 0) {
        lv_btnmatrix_set_map(scr_kb_input->btn_mtx, btnm_map_lc);
    } else if (strcmp(txt, "a/A") == 0) {
        lv_btnmatrix_set_map(scr_kb_input->btn_mtx, btnm_map_uc);
    } else if (strcmp(txt, "#+-") == 0) {
        lv_btnmatrix_set_map(scr_kb_input->btn_mtx, btnm_map_spec);
    } else if (strcmp(txt, "123") == 0) {
        lv_btnmatrix_set_map(scr_kb_input->btn_mtx, btnm_map_num);
    } else if (strcmp(txt, "abc") == 0) {
        lv_btnmatrix_set_map(scr_kb_input->btn_mtx, btnm_map_lc);
    } else {
        lv_textarea_add_text(scr_kb_input->input_ta, txt);
    }
}

static void lisa_ui_scr_kb_input_clear_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    lisa_ui_scr_kb_input_t *scr_kb_input = (lisa_ui_scr_kb_input_t *)lv_event_get_user_data(event);
    lv_textarea_set_text(scr_kb_input->input_ta, "");
}

static void lisa_ui_scr_kb_input_back_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *obj = lv_event_get_user_data(event);
    lv_event_send(obj, LV_EVENT_CANCEL, NULL);
}

static void lisa_ui_scr_kb_input_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lv_obj_create(obj);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lisa_ui_scr_kb_input_t *scr_kb_input = (lisa_ui_scr_kb_input_t *)obj;

    // lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    // lv_obj_set_flex_grow(container, 1);

    lv_obj_t *back_lbl = lv_label_create(container);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_width(back_lbl, LV_DPX(46));
    lv_obj_set_style_pad_ver(back_lbl, LV_DPX(12), LV_PART_MAIN);
    lv_obj_align(back_lbl, LV_ALIGN_TOP_LEFT, LV_DPX(0), LV_DPX(0));
    lv_obj_set_style_text_align(back_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0x71422A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_lbl, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(back_lbl, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back_lbl, lisa_ui_scr_kb_input_back_event_cb, LV_EVENT_CLICKED, obj);
    lv_obj_add_flag(back_lbl, LV_OBJ_FLAG_CLICKABLE);

    scr_kb_input->title = lv_label_create(container);
    lv_obj_align_to(scr_kb_input->title, back_lbl, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_set_flex_grow(scr_kb_input->title, 1);
    lv_obj_set_style_text_font(scr_kb_input->title, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_kb_input->title, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_kb_input->title, 0, LV_PART_MAIN);
    lv_obj_set_height(scr_kb_input->title, LV_SIZE_CONTENT);
    lv_label_set_long_mode(scr_kb_input->title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(scr_kb_input->title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(scr_kb_input->title, "标题");

    lv_obj_t * input_cont = lv_obj_create(container);
    lv_obj_set_size(input_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(input_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(input_cont, LV_FLEX_ALIGN_END, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(input_cont, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(input_cont, 0, LV_PART_MAIN);
    lv_obj_align_to(input_cont, back_lbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPX(12));

    scr_kb_input->input_ta = lv_textarea_create(input_cont);
    lv_obj_set_size(scr_kb_input->input_ta, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(scr_kb_input->input_ta, &lv_font_chinese_18, LV_PART_MAIN);
    lv_textarea_set_one_line(scr_kb_input->input_ta, true);
    lv_textarea_set_placeholder_text(scr_kb_input->input_ta, "请输入");
    lv_obj_set_flex_grow(scr_kb_input->input_ta, 1);
    lv_obj_set_style_border_width(scr_kb_input->input_ta, 0, LV_PART_MAIN);

    lv_obj_t *btn_lbl = lv_label_create(input_cont);
    lv_obj_add_event_cb(btn_lbl, lisa_ui_scr_kb_input_clear_event_cb, LV_EVENT_CLICKED, scr_kb_input);
    lv_obj_align(btn_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_size(btn_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_lbl, 0, LV_PART_MAIN);
    lv_obj_add_flag(btn_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(btn_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_pad_hor(btn_lbl, LV_DPX(5), LV_PART_MAIN);

    scr_kb_input->btn_mtx = lv_btnmatrix_create(container);
    lv_obj_set_style_bg_opa(scr_kb_input->btn_mtx, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_kb_input->btn_mtx, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr_kb_input->btn_mtx, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(scr_kb_input->btn_mtx, LV_DPX(1), LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr_kb_input->btn_mtx, LV_DPX(3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(scr_kb_input->btn_mtx, lv_color_hex(0xFABE00), LV_PART_SELECTED);

    lv_btnmatrix_set_map(scr_kb_input->btn_mtx, btnm_map_lc);
    lv_obj_set_size(scr_kb_input->btn_mtx, LV_PCT(100), LV_PCT(70));
    lv_obj_set_style_text_font(scr_kb_input->btn_mtx, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(scr_kb_input->btn_mtx, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_btnmatrix_set_btn_ctrl(scr_kb_input->btn_mtx, 26, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(scr_kb_input->btn_mtx, 27, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(scr_kb_input->btn_mtx, 28, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(scr_kb_input->btn_mtx, 30, LV_BTNMATRIX_CTRL_CHECKED);

    lv_btnmatrix_set_btn_width(scr_kb_input->btn_mtx, 26, 2);
    lv_btnmatrix_set_btn_width(scr_kb_input->btn_mtx, 29, 2);

    lv_obj_set_user_data(scr_kb_input->btn_mtx, obj);
    lv_obj_add_event_cb(scr_kb_input->btn_mtx, lisa_ui_scr_kb_input_value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *lisa_ui_scr_kb_input_create()
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, lv_layer_sys());
    lv_obj_class_init_obj(obj);

    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_FLOATING);

    return obj;
}

void lisa_ui_scr_kb_input_set_title(lv_obj_t *obj, const char *title)
{
    lisa_ui_scr_kb_input_t *scr_kb_input = (lisa_ui_scr_kb_input_t *)obj;
    lv_label_set_text(scr_kb_input->title, title);
}

void lisa_ui_scr_kb_input_set_tips(lv_obj_t *obj, const char *tips)
{
    lisa_ui_scr_kb_input_t *scr_kb_input = (lisa_ui_scr_kb_input_t *)obj;
    lv_textarea_set_placeholder_text(scr_kb_input->input_ta, tips);
}

const char * lisa_ui_scr_kb_input_get_text(lv_obj_t *obj)
{
    lisa_ui_scr_kb_input_t *scr_kb_input = (lisa_ui_scr_kb_input_t *)obj;
    return lv_textarea_get_text(scr_kb_input->input_ta);
}
