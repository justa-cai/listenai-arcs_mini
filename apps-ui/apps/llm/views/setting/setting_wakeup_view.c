/**
 * @file setting_wakeup_view.c
 * @brief Wakeup settings view implementation
 */

#include "setting_wakeup_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#define TAG "setting_wakeup_view"

#define WAKEUP_MODE_MAX 3

typedef struct {
    lv_obj_t *radio_bg;
    lv_obj_t *radio;
    uint16_t radio_id;
    lv_obj_t *tip_text;
    void *view;
} radio_group_item_t;

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *back_btn;
    lv_obj_t *title_label;
    lv_obj_t *radio_group;
    radio_group_item_t radio_items[WAKEUP_MODE_MAX];
    uint16_t current_mode;
    uint16_t radio_group_index;
    lisa_ui_setting_wakeup_back_cb_t back_cb;  /* 返回按钮回调函数 */
    void *back_user_data;                       /* 回调函数用户数据 */
    lisa_ui_setting_wakeup_mode_changed_cb_t mode_changed_cb;
    void *mode_changed_user_data;
} lisa_ui_setting_wakeup_view_t;

static void lisa_ui_setting_wakeup_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_setting_wakeup_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .constructor_cb = lisa_ui_setting_wakeup_view_class_constructor,
    .instance_size = sizeof(lisa_ui_setting_wakeup_view_t),
};

static lv_style_t style_radio;

static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)lv_event_get_user_data(e);
        LISA_UI_LOGD("Wakeup back button clicked");
        /* 调用回调函数，将页面导航逻辑交给presenter层处理 */
        if (view && view->back_cb) {
            view->back_cb(view->back_user_data);
        }
    }
}

static void update_radio_buttons(lisa_ui_setting_wakeup_view_t *view, uint16_t selected_mode)
{
    if (!view || selected_mode >= WAKEUP_MODE_MAX) {
        return;
    }
    
    for (int i = 0; i < view->radio_group_index && i < WAKEUP_MODE_MAX; i++) {
        if (view->radio_items[i].radio && lv_obj_is_valid(view->radio_items[i].radio)) {
            lv_obj_clear_state(view->radio_items[i].radio, LV_STATE_CHECKED);
        }
    }
    
    if (selected_mode < view->radio_group_index && 
        view->radio_items[selected_mode].radio && 
        lv_obj_is_valid(view->radio_items[selected_mode].radio)) {
        lv_obj_add_state(view->radio_items[selected_mode].radio, LV_STATE_CHECKED);
    }
    
    view->current_mode = selected_mode;
}

static void radio_group_bg_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        radio_group_item_t *item = (radio_group_item_t *)lv_event_get_user_data(e);
        if (!item || !item->view) {
            return;
        }
        
        if (item->radio_id >= WAKEUP_MODE_MAX) {
            return;
        }
        
        lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)item->view;
        update_radio_buttons(view, item->radio_id);
        LISA_UI_LOGD("Wakeup mode changed to: %d", item->radio_id);
        if (view->mode_changed_cb) {
            view->mode_changed_cb(item->radio_id, view->mode_changed_user_data);
        }
    }
}

static lv_obj_t *add_radio_group_item(lv_obj_t *group, lisa_ui_setting_wakeup_view_t *view, 
                                       const char *txt, const char *tips_text, uint16_t radio_id)
{
    if (!group || !txt || !tips_text || !view || radio_id >= WAKEUP_MODE_MAX) {
        return NULL;
    }
    
    lv_obj_t *radio_bg = lv_obj_create(group);
    if (!radio_bg) {
        return NULL;
    }
    lv_obj_clear_flag(radio_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(radio_bg, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(radio_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(radio_bg, lv_color_hex(0x24242f), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(radio_bg, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(radio_bg, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    view->radio_items[radio_id].radio_bg = radio_bg;
    view->radio_items[radio_id].radio_id = radio_id;
    view->radio_items[radio_id].view = view;
    
    lv_obj_add_event_cb(radio_bg, radio_group_bg_event_handler, LV_EVENT_CLICKED, 
                        (void *)&view->radio_items[radio_id]);
    lv_obj_set_style_text_color(radio_bg, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_t *checkbox = lv_checkbox_create(radio_bg);
    if (!checkbox) {
        lv_obj_del(radio_bg);
        return NULL;
    }
    
    view->radio_items[radio_id].radio = checkbox;
    lv_obj_set_size(checkbox, LV_PCT(80), LV_SIZE_CONTENT);
    lv_checkbox_set_text(checkbox, txt);
    lv_obj_set_style_text_font(checkbox, &lv_font_chinese_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(checkbox, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_style(checkbox, &style_radio, LV_PART_INDICATOR);
    lv_obj_clear_flag(checkbox, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t *tip_text = lv_label_create(radio_bg);
    if (!tip_text) {
        lv_obj_del(radio_bg);
        return NULL;
    }
    
    view->radio_items[radio_id].tip_text = tip_text;
    lv_obj_align_to(tip_text, checkbox, LV_ALIGN_OUT_BOTTOM_LEFT, 30, 5);
    lv_obj_set_size(tip_text, LV_PCT(95), LV_SIZE_CONTENT);
    lv_label_set_text(tip_text, tips_text);
    lv_obj_set_style_text_font(tip_text, &lv_font_chinese_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tip_text, lv_color_hex(0xBCBCBC), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_clear_flag(tip_text, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(tip_text, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(tip_text, LV_DIR_NONE);
    
    if (view->radio_group_index < WAKEUP_MODE_MAX) {
        view->radio_group_index++;
    }
    
    return checkbox;
}

static void lisa_ui_setting_wakeup_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)obj;
    
    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (NULL == container) {
        LISA_UI_LOGE("Failed to get base container");
        return;
    }
    
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), LV_PART_MAIN);
    
    view->back_btn = lv_btn_create(container);
    lv_obj_set_size(view->back_btn, 30, 30);
    lv_obj_align(view->back_btn, LV_ALIGN_TOP_LEFT, 5, 0);
    lv_obj_set_style_bg_opa(view->back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->back_btn, 0, LV_PART_MAIN);
    
    lv_obj_t *back_icon = lv_img_create(view->back_btn);
    lv_img_set_src(back_icon, &icons_icon_back_png);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(view->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, view);
    
    view->title_label = lv_label_create(container);
    lv_label_set_text(view->title_label, _("wakeup setting"));
    lv_obj_set_style_text_color(view->title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->title_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(view->title_label, LV_ALIGN_TOP_LEFT, 45, 8);
    
    lv_style_init(&style_radio);
    lv_style_set_radius(&style_radio, LV_RADIUS_CIRCLE);
    
    view->radio_group = lv_obj_create(container);
    if (!view->radio_group) {
        LISA_UI_LOGE("Failed to create radio_group");
        return;
    }
    
    lv_obj_set_size(view->radio_group, LV_PCT(100), LV_PCT(80));
    lv_obj_align(view->radio_group, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(view->radio_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view->radio_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(view->radio_group, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(view->radio_group, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(view->radio_group, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    view->radio_group_index = 0;
    
    add_radio_group_item(view->radio_group, view, _("button wakeup"), 
                         _("button wakeup description"), 0);
    add_radio_group_item(view->radio_group, view, _("voice wakeup(single)"), 
                         _("voice wakeup single description"), 1);
    add_radio_group_item(view->radio_group, view, _("voice wakeup(multiple)"), 
                         _("voice wakeup multiple description"), 2);
    
    view->current_mode = 0;
    update_radio_buttons(view, 0);
}

lv_obj_t *lisa_ui_setting_wakeup_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_setting_wakeup_view_class, parent);
    lv_obj_class_init_obj(obj);
    
    /* 初始化回调函数为NULL */
    lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)obj;
    view->back_cb = NULL;
    view->back_user_data = NULL;
    view->mode_changed_cb = NULL;
    view->mode_changed_user_data = NULL;
    
    return obj;
}

void lisa_ui_setting_wakeup_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_wakeup_back_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }
    
    /* 设置返回按钮回调函数和用户数据 */
    lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)obj;
    view->back_cb = cb;
    view->back_user_data = user_data;
}

void lisa_ui_setting_wakeup_view_set_mode_changed_cb(lv_obj_t *obj, lisa_ui_setting_wakeup_mode_changed_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)obj;
    view->mode_changed_cb = cb;
    view->mode_changed_user_data = user_data;
}

void lisa_ui_setting_wakeup_view_set_mode(lv_obj_t *obj, uint16_t mode)
{
    if (!obj) {
        return;
    }

    lisa_ui_setting_wakeup_view_t *view = (lisa_ui_setting_wakeup_view_t *)obj;
    update_radio_buttons(view, mode);
}
