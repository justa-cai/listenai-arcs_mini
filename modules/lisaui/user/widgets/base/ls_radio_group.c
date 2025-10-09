/**
 * @file radio_group.c
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-22
 *
 * @copyright Copyright (c) 2021 - 2024 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ls_radio_group.h"

typedef struct _ls_radio_item_t {
    lv_obj_t *radio_panel;
    lv_obj_t *radio;
    uint16_t radio_id;
    lv_obj_t *tip_text;
    lv_event_cb_t event_cb;
    void *user_data;
} ls_radio_item_t;

#define RADIO_GROUP_MAX 10
typedef struct _ls_radio_group_t {
    ls_radio_item_t radio_items[RADIO_GROUP_MAX];
    uint16_t selected_index;
    uint16_t size;
    lv_style_t style_radio;
    lv_style_t style_radio_chk;
} ls_radio_group_t;

static ls_radio_group_t g_radio_group = {
    .size = 0,
    .selected_index = 0,
};

lv_obj_t *ls_lv_radio_group_create(lv_obj_t *parent, lv_event_cb_t event_cb, void *user_data)
{
    g_radio_group.size = 0;
    lv_style_init(&g_radio_group.style_radio);
    lv_style_set_radius(&g_radio_group.style_radio, LV_RADIUS_CIRCLE);

    lv_obj_t *radio_group = lv_obj_create(parent);
    lv_obj_set_size(radio_group, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(radio_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(radio_group, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(radio_group, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(radio_group, 0, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(radio_group, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    return radio_group;
}

void radio_group_bg_event_handler(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    if (LV_EVENT_CLICKED == event_code) {
        uint16_t radio_index = *(uint16_t *)lv_event_get_user_data(event);
        lv_lv_radio_group_set_active_id(event->target, radio_index);
        if (g_radio_group.radio_items[radio_index].event_cb) {
            g_radio_group.radio_items[radio_index].event_cb(event);
        }
    }
}

int lv_lv_radio_group_set_active_id(lv_obj_t *group, uint16_t id)
{
    if (id >= RADIO_GROUP_MAX) {
        return -1;
    }
    if (id == g_radio_group.selected_index) {
        return 0;
    }
    lv_obj_clear_state(g_radio_group.radio_items[g_radio_group.selected_index].radio, LV_STATE_CHECKED);
    lv_obj_add_state(g_radio_group.radio_items[id].radio, LV_STATE_CHECKED);
    g_radio_group.selected_index = id;
    return 0;
}

lv_obj_t *ls_lv_radio_group_add(lv_obj_t *group, const char *title_text, const char *tips_text, lv_event_cb_t event_cb,
                                void *user_data)
{
    lv_obj_t *radio_panel = lv_obj_create(group);
    lv_obj_set_size(radio_panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(radio_panel, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_size(radio_panel, LV_PCT(100), LV_SIZE_CONTENT);
    // lv_obj_set_flex_flow(radio_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(radio_panel, lv_color_hex(0x24242f), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *title_checkbox = lv_checkbox_create(radio_panel);

    uint16_t radio_group_index = g_radio_group.size;
    if (radio_group_index >= RADIO_GROUP_MAX) {
        return NULL;
    }
    ls_radio_item_t *item = &g_radio_group.radio_items[radio_group_index];

    item->radio_panel = radio_panel;
    item->radio = title_checkbox;
    item->radio_id = radio_group_index;
    item->event_cb = event_cb;
    lv_obj_add_event_cb(radio_panel, radio_group_bg_event_handler, LV_EVENT_CLICKED, (void *)&item->radio_id);
    lv_obj_set_style_text_color(radio_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(title_checkbox, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(title_checkbox, LV_PCT(90), LV_SIZE_CONTENT);

    lv_checkbox_set_text(title_checkbox, title_text);
    lv_obj_add_flag(title_checkbox, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_style(title_checkbox, &g_radio_group.style_radio, LV_PART_INDICATOR);
    lv_obj_add_style(title_checkbox, &g_radio_group.style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(title_checkbox, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(title_checkbox, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *tip_text = lv_label_create(radio_panel);
    lv_obj_align_to(tip_text, title_checkbox, LV_ALIGN_BOTTOM_LEFT, 30, 25);
    lv_obj_set_size(tip_text, LV_PCT(86), LV_SIZE_CONTENT);

    lv_label_set_text(tip_text, tips_text);
    // lv_obj_set_style_text_font(tip_text, &lv_font_notosans_cs_medium_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tip_text, lv_color_hex(0xBCBCBC), LV_PART_MAIN | LV_STATE_DEFAULT);

    item->tip_text = tip_text;
    g_radio_group.size++;
    if (g_radio_group.size == 1) {
        lv_obj_add_state(title_checkbox, LV_STATE_CHECKED);
        g_radio_group.selected_index = radio_group_index;
    }
    return title_checkbox;
}

void ls_lv_radio_group_example(lv_obj_t *parent)
{
    lv_obj_t *radio_group = ls_lv_radio_group_create(parent, NULL, NULL);

    ls_lv_radio_group_add(radio_group, "Option 1", "This is option 1", NULL, NULL);
    ls_lv_radio_group_add(radio_group, "Option 2", "This is option 2. Option 2 is a long text", NULL, NULL);
    ls_lv_radio_group_add(radio_group, "Option 3", "This is option 3", NULL, NULL);
    ls_lv_radio_group_add(radio_group, "Option 4", "This is option 4. Option 4 is a long text and it is a long text",
                          NULL, NULL);
    ls_lv_radio_group_add(radio_group, "Option 5", "This is option 5", NULL, NULL);
    ls_lv_radio_group_add(radio_group, "Option 6", "This is option 6", NULL, NULL);
}