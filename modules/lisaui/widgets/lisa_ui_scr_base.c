#include "lvgl.h"
#include "lisa_ui_bar.h"
#include "lisa_ui_sys_bar.h"
#include "lisa_ui_src_base.h"

static void lisa_ui_base_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_base_class = {
    .base_class = &lv_obj_class,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .instance_size = sizeof(lisa_ui_scr_base_t),
    .constructor_cb = lisa_ui_base_class_constructor,
};

static lv_style_t lisa_ui_base_style;
static uint8_t lisa_ui_base_style_inited = 0;

static void lisa_ui_base_style_init(void)
{
    lv_style_init(&lisa_ui_base_style);
    lv_style_set_bg_color(&lisa_ui_base_style, lv_color_hex(0xFFF6CC));
    lv_style_set_bg_opa(&lisa_ui_base_style, LV_OPA_COVER);
    lv_style_set_border_width(&lisa_ui_base_style, 0);
    lv_style_set_border_opa(&lisa_ui_base_style, LV_OPA_COVER);
}

#define MY_CLASS &lisa_ui_base_class

#define TOP_TASK_BAR_HEIGHT 30

static void lisa_ui_base_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    if (!lisa_ui_base_style_inited) {
        lisa_ui_base_style_init();
        lisa_ui_base_style_inited = 1;
    }
    lv_obj_add_style(obj, &lisa_ui_base_style, LV_PART_MAIN);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);

    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;

    scr_base->bar = lisa_ui_sys_bar_get();
    lv_obj_set_parent(scr_base->bar, obj);
    lv_obj_move_to_index(scr_base->bar, 0);
    lv_obj_clear_flag(scr_base->bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(scr_base->bar);

    scr_base->container = lv_obj_create(obj);
    lv_obj_set_width(scr_base->container, LV_PCT(100));
    lv_obj_set_style_bg_opa(scr_base->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_base->container, 0, LV_PART_MAIN);
    lv_obj_set_flex_grow(scr_base->container, 1);

    memset(scr_base->title, 0, sizeof(scr_base->title));
}

lv_obj_t *lisa_ui_scr_base_container_get(lv_obj_t *obj)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;
    return scr_base->container;
}

void lisa_ui_scr_base_title_set(lv_obj_t *obj, const char *title)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;
    lv_obj_t *p = lv_obj_get_parent(scr_base->bar);
    uint8_t len = strlen(title);

    if (len > sizeof(scr_base->title) - 1) {
        len = sizeof(scr_base->title) - 1;
    }

    strncpy(scr_base->title, title, len);
    scr_base->title[len] = '\0';

    if (p == obj) {
        lisa_ui_bar_title_set(scr_base->bar, scr_base->title);
    }
}

void lisa_ui_scr_base_title_hide(lv_obj_t *obj)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;
    lv_obj_t *p = lv_obj_get_parent(scr_base->bar);
    if (p == obj) {
        lisa_ui_bar_title_hide(scr_base->bar);
    }
}

void lisa_ui_scr_base_title_show(lv_obj_t *obj)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;
    lv_obj_t *p = lv_obj_get_parent(scr_base->bar);
    if (p == obj) {
        lisa_ui_bar_title_show(scr_base->bar);
    }
}

void lisa_ui_scr_base_bar_hide(lv_obj_t *obj)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;

    lv_obj_t *p = lv_obj_get_parent(scr_base->bar);
    if (p == obj) {
        lv_obj_add_flag(scr_base->bar, LV_OBJ_FLAG_HIDDEN);
    }
}

void lisa_ui_scr_base_bar_show(lv_obj_t *obj)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;

    lv_obj_t *sys_bar = lisa_ui_sys_bar_get();
    lv_obj_t *p = lv_obj_get_parent(sys_bar);

    if (p != obj) {
        lv_obj_set_parent(sys_bar, obj);
        lv_obj_move_to_index(sys_bar, 0);
        lv_obj_update_layout(sys_bar);
    }
    lv_obj_clear_flag(sys_bar, LV_OBJ_FLAG_HIDDEN);
    lisa_ui_bar_title_set(sys_bar, scr_base->title);
    lisa_ui_bar_title_show(sys_bar);

    scr_base->bar = sys_bar;
}

void lisa_ui_scr_base_show(lv_obj_t *obj)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lisa_ui_scr_base_bar_show(obj);
    lv_scr_load(obj);
}

void lisa_ui_scr_base_hide(lv_obj_t *obj)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lisa_ui_scr_base_bar_hide(obj);
}
void lisa_ui_scr_base_del(lv_obj_t *obj)
{
    lisa_ui_scr_base_t *scr_base = (lisa_ui_scr_base_t *)obj;

    if (scr_base->bar == lisa_ui_sys_bar_get()) {
        lv_obj_t *p = lv_obj_get_parent(scr_base->bar);

        if (p == obj) {
            lisa_ui_sys_bar_parent_reset(scr_base->bar);
        }

        if (lv_scr_act() == obj) {
            lv_obj_t *parent = lisa_ui_sys_bar_parent_get();
            lv_scr_load(parent);
        }
    }

    lv_obj_invalidate(obj);
    lv_obj_del(obj);
}
