#include "lisa_ui_scr_alarm.h"
#include "lisa_ui.h"

struct lisa_ui_alarm_item {
    lv_obj_t obj;
};

typedef struct lisa_ui_alarm_item lisa_ui_alarm_item_t;

const lv_obj_class_t lv_lisa_ui_alarm_item_class = {
    .base_class = &lv_obj_class,
    .instance_size = sizeof(lisa_ui_alarm_item_t),
};

static lv_obj_t *lisa_ui_alarm_item_create(lv_obj_t *parent, const char *label, const char *sub_label)
{
    lv_obj_t *item = lv_obj_class_create_obj(&lv_lisa_ui_alarm_item_class, parent);
    lv_obj_class_init_obj(item);

    lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(item, LV_DPX(10), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_hor(item, LV_DPX(10), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(item, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x24242f), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(item, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(item, lv_color_hex(0xfd8926), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_t *time_lbl = lv_label_create(item);
    lv_label_set_text(time_lbl, label);
    lv_obj_align(time_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(time_lbl, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(time_lbl, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sub_lbl = lv_label_create(item);
    lv_label_set_text(sub_lbl, sub_label);
    lv_obj_align(sub_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_align_to(sub_lbl, time_lbl, LV_ALIGN_BOTTOM_LEFT, LV_DPX(4), LV_DPX(24));
    lv_obj_set_size(sub_lbl, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_notosans_cs_medium_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(sub_lbl, lv_color_hex(0xBCBCBC), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *del_btn = lv_btn_create(item);
    lv_obj_set_size(del_btn, LV_PCT(20), LV_SIZE_CONTENT);
    lv_obj_align(del_btn, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(del_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_flag(del_btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *del_label = lv_label_create(del_btn);
    lv_label_set_text(del_label, "删除");
    lv_obj_align(del_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(del_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(del_label, &lv_font_chinese_18, LV_PART_MAIN);

    lv_obj_set_height(del_label, LV_DPX(40));
    lv_obj_set_style_text_font(del_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(del_label, lv_color_hex(0xFF6F7F), LV_PART_MAIN | LV_STATE_DEFAULT);

    return item;
}

struct lisa_ui_alarm {
    lv_obj_t obj;
    lv_obj_t *list;
};

typedef struct lisa_ui_alarm lisa_ui_alarm_t;
extern const lv_obj_class_t lisa_ui_base_class;
const lv_obj_class_t lv_lisa_ui_alarm_class = {
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_alarm_t)
};

lv_obj_t *lisa_ui_alarm_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lv_lisa_ui_alarm_class, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_alarm_t *alarm = (lisa_ui_alarm_t *)obj;

    alarm->list = lv_list_create(obj);
    lv_obj_set_size(alarm->list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(alarm->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(alarm->list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(alarm->list, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(alarm->list, 0, 0);
    lv_obj_set_style_pad_ver(alarm->list, LV_DPX(10), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_hor(alarm->list, LV_DPX(10), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(alarm->list, lv_color_hex(0x0F0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(alarm->list, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(alarm->list, LV_DPX(10), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(alarm->list, LV_OBJ_FLAG_EVENT_BUBBLE);

    return obj;
}

int lisa_ui_alarm_item_add(lv_obj_t *obj, const char *label, const char *sub_label)
{
    lisa_ui_alarm_t *alarm = (lisa_ui_alarm_t *)obj;

    lv_obj_t *item = lisa_ui_alarm_item_create(alarm->list, label, sub_label);
    uint32_t item_idx = lv_obj_get_index(item);

    lv_obj_add_flag(item, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_user_data(item, (void *)item_idx);

    return item_idx;
}

void lisa_ui_alarm_item_clear(lv_obj_t *obj)
{
    lisa_ui_alarm_t *alarm = (lisa_ui_alarm_t *)obj;
    lv_obj_clean(alarm->list);
}

const char *lisa_ui_alarm_item_label_get_by_idx(lv_obj_t *obj, uint32_t idx)
{
    lisa_ui_alarm_t *alarm = (lisa_ui_alarm_t *)obj;
    lv_obj_t *item = lv_obj_get_child(alarm->list, idx);
    lv_obj_t *label = lv_obj_get_child(item, 0);
    const char *label_text = lv_label_get_text(label);

    return label_text;
}
