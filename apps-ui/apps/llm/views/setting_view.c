/**
 * @file setting_view.c
 * @brief Setting main page view implementation
 * 
 * Pure UI layer for settings menu with 2x2 grid card layout.
 */

#define TAG "setting_view"

#include "setting_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include <string.h>


/**
 * @brief Setting view structure
 */
struct lisa_ui_setting_view {
    lisa_ui_llm_base_t base_obj;    /**< Base object */
    
    lv_obj_t *back_btn;             /**< Back button (X icon) */
    lv_obj_t *list;                 /**< Setting items container */
    lisa_ui_setting_item_click_cb_t click_cb;
    void *click_user_data;
    lisa_ui_setting_back_cb_t back_cb;
    void *back_user_data;
};

typedef struct lisa_ui_setting_view lisa_ui_setting_view_t;

static void lisa_ui_setting_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_setting_view_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_setting_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_setting_view_t),
    .constructor_cb = lisa_ui_setting_view_class_constructor,
    .destructor_cb = lisa_ui_setting_view_class_destructor,
};

static void list_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)lv_event_get_user_data(e);
        if (setting->click_cb) {
            lv_obj_t *card = lv_event_get_target(e);
            // Get text label (second child of card)
            uint32_t child_cnt = lv_obj_get_child_cnt(card);
            if (child_cnt >= 2) {
                lv_obj_t *text_label = lv_obj_get_child(card, 1);
                const char *name = lv_label_get_text(text_label);
                setting->click_cb(name, setting->click_user_data);
            }
        }
    }
}

static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)lv_event_get_user_data(e);
        LISA_UI_LOGD("Setting back button clicked");
        if (setting && setting->back_cb) {
            setting->back_cb(setting->back_user_data);
        }
    }
}

static void lisa_ui_setting_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)obj;
    
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
    
    // Create back button (X icon) in top-left corner at the very top
    setting->back_btn = lv_btn_create(container);
    lv_obj_set_size(setting->back_btn, 30, 30);
    lv_obj_align(setting->back_btn, LV_ALIGN_TOP_LEFT, 5, 0);
    lv_obj_set_style_bg_opa(setting->back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(setting->back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(setting->back_btn, 0, LV_PART_MAIN);
    lv_obj_add_flag(setting->back_btn, LV_OBJ_FLAG_CLICKABLE);  // Ensure clickable
    
    lv_obj_t *back_icon = lv_img_create(setting->back_btn);
    lv_img_set_src(back_icon, &icons_icon_closs_png);
    lv_obj_center(back_icon);
    
    lv_obj_add_event_cb(setting->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, setting);
    LISA_UI_LOGD("Setting back button created and event added");
    
    // Create transparent container for cards (no grid, use absolute positioning like original)
    setting->list = lv_obj_create(container);
    lv_obj_set_size(setting->list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(setting->list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(setting->list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(setting->list, 0, LV_PART_MAIN);
    lv_obj_clear_flag(setting->list, LV_OBJ_FLAG_CLICKABLE);  // List not clickable, only cards
    
    // Move back button to front so it's not covered by cards
    lv_obj_move_foreground(setting->back_btn);
}

static void lisa_ui_setting_view_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    (void)class_p;
    (void)obj;
    // Child objects auto-deleted by LVGL
}

// ===================== Public API Implementation =====================

lv_obj_t *lisa_ui_setting_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_setting_view_class, parent);
    lv_obj_class_init_obj(obj);
    
    lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)obj;
    setting->click_cb = NULL;
    setting->click_user_data = NULL;
    setting->back_cb = NULL;
    setting->back_user_data = NULL;
    
    // Set page title
    lisa_ui_llm_base_set_title(obj, "Settings");
    
    return obj;
}

void lisa_ui_setting_view_item_add(lv_obj_t *obj, const char *label, const void *icon_src, int col, int row)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_setting_view_class)) {
        return;
    }
    
    lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)obj;
    
    // Calculate position like original UI (2x2 grid with absolute positioning)
    lv_coord_t card_width = 140;
    lv_coord_t card_height = 55;
    lv_coord_t margin = 10;
    lv_coord_t start_x = (320 - (2 * card_width + margin)) / 2;  // Center horizontally
    lv_coord_t start_y = 35;  // Start below back button (30px height + 5px margin)
    
    lv_coord_t x = start_x + col * (card_width + margin);
    lv_coord_t y = start_y + row * (card_height + margin);
    
    // Create card (exactly like original UI)
    lv_obj_t *card = lv_obj_create(setting->list);
    lv_obj_set_size(card, card_width, card_height);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 15, LV_PART_MAIN);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    
    // Icon on left (PNG image from assets, like original UI)
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, icon_src);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);
    
    // Text on right (like original UI)
    lv_obj_t *text = lv_label_create(card);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_color(text, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(text, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(text, LV_ALIGN_RIGHT_MID, -10, 0);
    
    // Add click event
    lv_obj_add_event_cb(card, list_btn_event_handler, LV_EVENT_CLICKED, setting);
}

void lisa_ui_setting_view_set_click_cb(lv_obj_t *obj, lisa_ui_setting_item_click_cb_t cb, void *user_data)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_setting_view_class)) {
        return;
    }
    
    lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)obj;
    setting->click_cb = cb;
    setting->click_user_data = user_data;
}

void lisa_ui_setting_view_clear(lv_obj_t *obj)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_setting_view_class)) {
        return;
    }
    
    lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)obj;
    lv_obj_clean(setting->list);
}

void lisa_ui_setting_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_back_cb_t cb, void *user_data)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_setting_view_class)) {
        return;
    }
    
    lisa_ui_setting_view_t *setting = (lisa_ui_setting_view_t *)obj;
    setting->back_cb = cb;
    setting->back_user_data = user_data;
}
