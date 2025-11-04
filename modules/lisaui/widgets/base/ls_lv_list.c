/**
 * @file ls_lv_list.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "ls_lv_list.h"

// #if LV_USE_LIST

/*********************
 *      DEFINES
 *********************/
#define MV_CLASS &ls_lv_list

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

const lv_obj_class_t ls_lv_list_class = {
    .base_class = &lv_obj_class, .width_def = (LV_DPI_DEF * 3) / 2, .height_def = LV_DPI_DEF * 2};

const lv_obj_class_t ls_lv_list_btn_class = {
    .base_class = &lv_label_class,
};

const lv_obj_class_t ls_lv_list_text_class = {
    .base_class = &lv_label_class,
};

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t *ls_lv_list_create(lv_obj_t *parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t *list_panel = lv_obj_create(parent);
    lv_obj_set_size(list_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(list_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(list_panel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(list_panel, 0, 0);
    // lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(list_panel, lv_color_hex(0x0F0000), LV_PART_MAIN | LV_STATE_DEFAULT);

    return list_panel;
}

lv_obj_t *ls_lv_list_add_text(lv_obj_t *list, const char *txt)
{
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(&ls_lv_list_text_class, list);
    lv_obj_class_init_obj(obj);
    lv_label_set_text(obj, txt);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, LV_DPX(40));
    lv_obj_set_style_text_font(obj, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

lv_obj_t *ls_lv_list_add_btn(lv_obj_t *list, const char *title_text, const char *tips_text, lv_event_cb_t event_cb,
                             void *user_data)
{
    LV_LOG_INFO("begin");
    lv_obj_t *item = lv_obj_create(list);
    lv_obj_set_user_data(item, user_data);
    lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(item, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x24242f), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *title_label = lv_label_create(item);
    lv_obj_set_size(title_label, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(title_label, title_text);
    lv_obj_set_style_text_font(title_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *tip_text = lv_label_create(item);
    lv_obj_align_to(tip_text, title_label, LV_ALIGN_BOTTOM_LEFT, LV_DPX(4), LV_DPX(24));
    lv_obj_set_size(tip_text, LV_PCT(80), LV_SIZE_CONTENT);

    lv_label_set_text(tip_text, tips_text);
    // lv_obj_set_style_text_font(tip_text, &lv_font_notosans_cs_medium_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tip_text, lv_color_hex(0xBCBCBC), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *op_btn = lv_btn_create(item);
    lv_obj_set_size(op_btn, LV_PCT(20), LV_SIZE_CONTENT);
    lv_obj_align(op_btn, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    lv_obj_set_style_bg_color(op_btn, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *op_label = lv_label_create(op_btn);
    lv_label_set_text(op_label, "删除");
    lv_obj_set_height(op_label, LV_DPX(40));
    lv_obj_align(op_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(op_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(op_label, lv_color_hex(0xFF6F7F), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_align(op_label, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    lv_obj_add_event_cb(op_btn, event_cb, LV_EVENT_CLICKED, user_data);

    return item;
}