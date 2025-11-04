#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_bar.h"

struct lisa_ui_bar {
    lv_obj_t obj;
    lv_obj_t *nav_btn;
    lv_obj_t *nav_icon;
    lv_obj_t *title;
    lv_obj_t *container;
    lisa_ui_bar_nav_btn_click_event_cb_t nav_btn_click_event_cb;
    void *nav_btn_click_event_cb_user_data;
};

typedef struct lisa_ui_bar lisa_ui_bar_t;

const lv_obj_class_t lv_lisa_ui_bar_class = {
    .base_class = &lv_obj_class,
    .width_def = LV_DPI_DEF * 2,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lisa_ui_bar_t)
};

#define MY_CLASS &lv_lisa_ui_bar_class

int lisa_ui_bar_icon_add(lv_obj_t *bar, const void *icon_path)
{
    if (bar == NULL || icon_path == NULL) {
        return -1;
    }

    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;

    if (bar_obj->container == NULL) {
        return -1;
    }

    lv_obj_t *icon = lv_img_create(bar_obj->container);
    lv_img_set_src(icon, icon_path);

    /* TODO: 没有在容器中垂直居中, 暂时这样设置 */
    lv_obj_set_style_pad_bottom(icon, LV_DPX(10), LV_PART_MAIN);

    return lv_obj_get_index(icon);
}

#define TOP_TASK_BAR_HEIGHT 30

static void lisa_ui_bar_nav_btn_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        bar_obj->nav_btn_click_event_cb(bar_obj->nav_btn_click_event_cb_user_data);
    }
}

lv_obj_t *lisa_ui_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(bar);

    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_height(bar, TOP_TASK_BAR_HEIGHT);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 设置为行布局 */
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    /* 设置内边距 */
    // lv_obj_set_style_pad_ver(bar, LV_DPX(12), LV_PART_MAIN);
    // lv_obj_set_style_pad_hor(bar, LV_DPX(12), LV_PART_MAIN);

    /* 设置行间距 */
    lv_obj_set_style_pad_column(bar, LV_DPX(8), LV_PART_MAIN);

    /* 导航按钮 */
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;

    // bar_obj->nav_btn = lv_btn_create(bar);
    // lv_obj_set_size(bar_obj->nav_btn, LV_DPX(28), LV_DPX(28));
    // lv_obj_align(bar_obj->nav_btn, LV_ALIGN_LEFT_MID, LV_DPX(0), LV_DPX(0));
    // lv_obj_add_flag(bar_obj->nav_btn, LV_OBJ_FLAG_EVENT_BUBBLE);
    // lv_obj_set_style_text_color(bar_obj->nav_btn, lv_color_hex(0x71422A), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(bar_obj->nav_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    // lv_obj_set_style_border_width(bar_obj->nav_btn, 0, LV_PART_MAIN);
    // lv_obj_set_style_radius(bar_obj->nav_btn, 0, LV_PART_MAIN);
    // lv_obj_set_style_bg_color(bar_obj->nav_btn, lv_color_hex(0xfd8926), LV_STATE_PRESSED);
    // lv_obj_set_style_bg_opa(bar_obj->nav_btn, LV_OPA_COVER, LV_STATE_PRESSED);
    // lv_obj_add_event_cb(bar_obj->nav_btn, lisa_ui_bar_nav_btn_click_event_cb, LV_EVENT_CLICKED, bar_obj);
    // lv_obj_add_flag(bar_obj->nav_btn, LV_OBJ_FLAG_CLICKABLE);

    /* 导航按钮图标 */
    bar_obj->nav_icon = lv_img_create(bar);
    // lv_obj_set_align(bar_obj->nav_icon, LV_ALIGN_CENTER);
    // lv_obj_set_size(bar_obj->nav_icon, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // lv_obj_set_style_pad_all(bar_obj->nav_icon, LV_DPX(12), LV_PART_MAIN);
    // lv_img_set_src(bar_obj->nav_icon, LV_SYMBOL_SETTINGS);
    // lv_obj_align(bar_obj->nav_icon, LV_ALIGN_LEFT_MID, LV_DPX(0), LV_DPX(0));
    // lv_obj_set_style_text_color(bar_obj->nav_icon, lv_color_hex(0x71422A), LV_PART_MAIN);
    // lv_obj_set_style_bg_color(bar_obj->nav_icon, lv_color_hex(0xFF0000), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(bar_obj->nav_icon, LV_OPA_COVER, LV_PART_MAIN);
    // lv_obj_set_style_border_width(bar_obj->nav_icon, 0, LV_PART_MAIN);
    // lv_obj_set_style_radius(bar_obj->nav_icon, 0, LV_PART_MAIN);
    // lv_obj_add_event_cb(bar_obj->nav_icon, lisa_ui_bar_nav_btn_click_event_cb, LV_EVENT_CLICKED, bar_obj);
    // lv_obj_add_flag(bar_obj->nav_icon, LV_OBJ_FLAG_CLICKABLE);

    bar_obj->nav_icon = lv_label_create(bar);
    lv_label_set_text(bar_obj->nav_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_width(bar_obj->nav_icon, LV_DPX(36));
    lv_obj_set_style_pad_ver(bar_obj->nav_icon, LV_DPX(12), LV_PART_MAIN);
    lv_obj_align(bar_obj->nav_icon, LV_ALIGN_LEFT_MID, LV_DPX(0), LV_DPX(0));
    lv_obj_set_style_text_align(bar_obj->nav_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(bar_obj->nav_icon, lv_color_hex(0x71422A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_obj->nav_icon, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_obj->nav_icon, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_obj->nav_icon, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(bar_obj->nav_icon, lisa_ui_bar_nav_btn_click_event_cb, LV_EVENT_CLICKED, bar_obj);
    lv_obj_add_flag(bar_obj->nav_icon, LV_OBJ_FLAG_CLICKABLE);

    /* 标题 */
    bar_obj->title = lv_label_create(bar);
    lv_obj_set_size(bar_obj->title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_text(bar_obj->title, "未知标题");
    lv_obj_set_align(bar_obj->title, LV_ALIGN_CENTER);
    lv_obj_set_style_text_align(bar_obj->title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(bar_obj->title, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(bar_obj->title, lv_color_hex(0x71422A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_obj->title, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_obj->title, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_obj->title, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_label_set_long_mode(bar_obj->title, LV_LABEL_LONG_SCROLL_CIRCULAR);

    /* 图标容器 */
    bar_obj->container = lv_obj_create(bar);
    lv_obj_set_flex_grow(bar_obj->container, 1);
    lv_obj_set_style_pad_ver(bar_obj->container, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(bar_obj->container, 10, LV_PART_MAIN);
    lv_obj_set_height(bar_obj->container, LV_PCT(100));
    lv_obj_set_style_bg_opa(bar_obj->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_obj->container, 0, LV_PART_MAIN);
    /* 设置图标容器为行布局 */
    lv_obj_set_flex_flow(bar_obj->container, LV_FLEX_FLOW_ROW);
    /* 图标容器从右到左 */
    lv_obj_set_style_flex_main_place(bar_obj->container, LV_FLEX_ALIGN_END, 0);
    /* 图标容器交叉轴对齐为居中 */
    lv_obj_set_style_flex_cross_place(bar_obj->container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    return bar;
}

void lisa_ui_bar_nav_btn_click_event_cb_set(lv_obj_t *bar, lisa_ui_bar_nav_btn_click_event_cb_t cb, void *user_data)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    bar_obj->nav_btn_click_event_cb = cb;
    bar_obj->nav_btn_click_event_cb_user_data = user_data;
}

void lisa_ui_bar_title_set(lv_obj_t *bar, const char *title)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_label_set_text(bar_obj->title, title);
}

void lisa_ui_bar_title_hide(lv_obj_t *bar)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_obj_add_flag(bar_obj->title, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_bar_title_show(lv_obj_t *bar)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_obj_clear_flag(bar_obj->title, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_bar_nav_btn_icon_set(lv_obj_t *bar, const void *icon_path)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_label_set_text(bar_obj->nav_icon, icon_path);
}

int lisa_ui_bar_icon_update_by_index(lv_obj_t *bar, uint8_t idx, const void *icon_path)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_img_set_src(bar_obj->nav_icon, icon_path);
    lv_obj_t *icon = lv_obj_get_child(bar_obj->container, idx);
    if (!icon) {
        return -1;
    }

    lv_img_set_src(icon, icon_path);

    return 0;
}

int lisa_ui_bar_icon_animate_set(lv_obj_t *bar, uint8_t idx, const void **icon_path, uint8_t icon_count)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_obj_t *icon = lv_obj_get_child(bar_obj->container, idx);
    if (!icon) {
        return -1;
    }

    return 0;
}

int lisa_ui_bar_icon_animate_start(lv_obj_t *bar, uint8_t idx)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_obj_t *icon = lv_obj_get_child(bar_obj->container, idx);
    if (!icon) {
        return -1;
    }

    return 0;
}

int lisa_ui_bar_icon_clean(lv_obj_t *bar)
{
    lisa_ui_bar_t *bar_obj = (lisa_ui_bar_t *)bar;
    lv_obj_clean(bar_obj->container);

    return 0;
}
