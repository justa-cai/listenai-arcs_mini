#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_bar.h"

static lv_obj_t *sys_bar = NULL;
static lv_obj_t *sys_bar_parent = NULL;

#include <stdio.h>

static void sys_bar_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        printf("[#####][%s %d]\n", __FUNCTION__, __LINE__);
    }
}

lv_obj_t *lisa_ui_sys_bar_parent_get(void)
{
    return sys_bar_parent;
}

lv_obj_t *lisa_ui_sys_bar_get(void)
{
    if (sys_bar && lv_obj_is_valid(sys_bar)) {
        return sys_bar;
    }
    sys_bar_parent = lv_obj_create(NULL);
    sys_bar = lisa_ui_bar_create(sys_bar_parent);
    lv_obj_set_style_bg_color(sys_bar, lv_color_hex(0xFFF6CC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sys_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sys_bar, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(sys_bar, sys_bar_event_cb, LV_EVENT_DELETE, NULL);

    return sys_bar;
}

void lisa_ui_sys_bar_parent_reset(lv_obj_t *bar)
{
    lv_obj_set_parent(bar, sys_bar_parent);
    lv_obj_move_to_index(bar, 0);
    lv_obj_update_layout(bar);
    lv_obj_invalidate(bar);
}

void lisa_ui_sys_bar_title_set(const char *title)
{
    lisa_ui_bar_title_set(lisa_ui_sys_bar_get(), title);
}

void lisa_ui_sys_bar_btn_nav_img_set(const void *icon_path)
{
    lisa_ui_bar_nav_btn_icon_set(lisa_ui_sys_bar_get(), icon_path);
}

void lisa_ui_sys_bar_btn_nav_click_event_cb_set(lisa_ui_bar_nav_btn_click_event_cb_t cb, void *user_data)
{
    lisa_ui_bar_nav_btn_click_event_cb_set(lisa_ui_sys_bar_get(), cb, user_data);
}

void lisa_ui_sys_bar_title_hide(void)
{
    lisa_ui_bar_title_hide(lisa_ui_sys_bar_get());
}

void lisa_ui_sys_bar_title_show(void)
{
    lisa_ui_bar_title_show(lisa_ui_sys_bar_get());
}
