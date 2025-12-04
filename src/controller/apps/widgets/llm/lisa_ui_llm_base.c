#include "lvgl.h"
#include "lisa_ui_llm_base.h"
#include <string.h>

static void lisa_ui_llm_base_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_llm_base_class = {
    .base_class = &lv_obj_class,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .instance_size = sizeof(lisa_ui_llm_base_t),
    .constructor_cb = lisa_ui_llm_base_class_constructor,
};

static lv_style_t lisa_ui_base_style;
static uint8_t lisa_ui_base_style_inited = 0;

static void lisa_ui_base_style_init(void)
{
    lv_style_init(&lisa_ui_base_style);
    lv_style_set_bg_color(&lisa_ui_base_style, lv_color_hex(LISA_UI_LLM_BASE_DEFAULT_BG_COLOR));
    lv_style_set_bg_opa(&lisa_ui_base_style, LV_OPA_COVER);
    lv_style_set_border_width(&lisa_ui_base_style, 0);
    lv_style_set_border_opa(&lisa_ui_base_style, LV_OPA_COVER);
}

static void lisa_ui_llm_base_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    if (!lisa_ui_base_style_inited) {
        lisa_ui_base_style_init();
        lisa_ui_base_style_inited = 1;
    }
    lv_obj_add_style(obj, &lisa_ui_base_style, LV_PART_MAIN);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    
    // 设置flex布局和对齐方式
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_START, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_START, LV_PART_MAIN);
    
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    // 清除所有间距
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(obj, 12, LV_PART_MAIN);

    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;

    // 创建任务栏
    llm_base->bar = lv_obj_create(obj);
    lv_obj_set_width(llm_base->bar, LV_PCT(100));
    lv_obj_set_height(llm_base->bar, LISA_UI_LLM_BASE_DEFAULT_BAR_HEIGHT);
    lv_obj_set_style_bg_color(llm_base->bar, lv_color_hex(LISA_UI_LLM_BASE_DEFAULT_BAR_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(llm_base->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_base->bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(llm_base->bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(llm_base->bar, 0, LV_PART_MAIN);

    // 创建容器
    llm_base->container = lv_obj_create(obj);
    lv_obj_set_width(llm_base->container, LV_PCT(100));
    lv_obj_set_style_bg_color(llm_base->container, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(llm_base->container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_base->container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(llm_base->container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(llm_base->container, 0, LV_PART_MAIN);
    lv_obj_set_flex_grow(llm_base->container, 1);

    // 禁用滑动功能
    lv_obj_clear_flag(llm_base->container, LV_OBJ_FLAG_SCROLLABLE);
    // 或者设置滑动条模式为关闭
    lv_obj_set_scrollbar_mode(llm_base->container, LV_SCROLLBAR_MODE_OFF);
    // 设置滑动方向为无
    lv_obj_set_scroll_dir(llm_base->container, LV_DIR_NONE);

    // 初始化字符串
    memset(llm_base->title, 0, sizeof(llm_base->title));
}

// ===================== 公共API实现 =====================

lv_obj_t *lisa_ui_llm_base_bar_get(lv_obj_t *obj)
{
    if (obj == NULL) {
        // 添加错误日志
        return NULL;
    }
    
    if (!LISA_UI_LLM_BASE_CLASS_CHECK(obj)) {
        // 添加类型检查失败的错误日志  
        return NULL;
    }
    
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    return llm_base->bar;
}

lv_obj_t *lisa_ui_llm_base_container_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_base_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    return llm_base->container;
}

void lisa_ui_llm_base_set_title(lv_obj_t *obj, const char *title)
{
    if (!lisa_ui_llm_base_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    if (title) {
        strncpy(llm_base->title, title, sizeof(llm_base->title) - 1);
        llm_base->title[sizeof(llm_base->title) - 1] = '\0';
    } else {
        memset(llm_base->title, 0, sizeof(llm_base->title));
    }
}

const char *lisa_ui_llm_base_get_title(lv_obj_t *obj)
{
    if (!lisa_ui_llm_base_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    return llm_base->title;
}

void lisa_ui_llm_base_set_bar_color(lv_obj_t *obj, lv_color_t color)
{
    if (!lisa_ui_llm_base_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    if (llm_base->bar) {
        lv_obj_set_style_bg_color(llm_base->bar, color, LV_PART_MAIN);
    }
}

void lisa_ui_llm_base_set_bar_height(lv_obj_t *obj, lv_coord_t height)
{
    if (!lisa_ui_llm_base_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    if (llm_base->bar && height > 0) {
        lv_obj_set_height(llm_base->bar, height);
    }
}

void lisa_ui_llm_base_clear_container(lv_obj_t *obj)
{
    if (!lisa_ui_llm_base_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_base_t *llm_base = (lisa_ui_llm_base_t *)obj;
    if (llm_base->container) {
        lv_obj_clean(llm_base->container);
    }
}


