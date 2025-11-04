#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_scr_setting_common.h"
#include "lisa_ui_src_base.h"
#include <stdio.h>

struct lisa_ui_scr_setting_common {
    lisa_ui_scr_base_t base;

    lv_obj_t *mic_slider;      // 麦克风增益滑动块
    lv_obj_t *mic_label;       // 麦克风增益标签
    lv_obj_t *mic_value_label; // 显示当前增益值的标签

    lv_obj_t *vol_slider;      // 音量滑动块
    lv_obj_t *vol_label;       // 音量标签
    lv_obj_t *vol_value_label; // 显示当前音量值的标签
};
typedef struct lisa_ui_scr_setting_common lisa_ui_scr_setting_common_t;

static void lisa_ui_scr_setting_common_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
const lv_obj_class_t lv_lisa_ui_scr_setting_common_class = {
    .constructor_cb = lisa_ui_scr_setting_common_constructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_scr_setting_common_t)
};

#define MY_CLASS &lv_lisa_ui_scr_setting_common_class

// 麦克风增益滑动块的事件处理函数
static void mic_slider_event_cb(lv_event_t *e)
{
    char buf[16];
    lv_obj_t *slider = lv_event_get_target(e);
    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)lv_event_get_user_data(e);
    int32_t value = lv_slider_get_value(slider);

    snprintf(buf, sizeof(buf), "%ld%%", value);
    lv_label_set_text(scr_setting_common->mic_value_label, buf);
    lv_event_send((lv_obj_t*)scr_setting_common,LISA_UI_SCR_SETTING_COMMON_MIC_AGAIN_SLIDER_VALUE_CHANGED_EVENT,&value);

}

static void vol_slider_event_cb(lv_event_t *e)
{
    char buf[16];
    lv_obj_t *slider = lv_event_get_target(e);
    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)lv_event_get_user_data(e);
    int32_t value = lv_slider_get_value(slider);

    snprintf(buf, sizeof(buf), "%ld%%", value);
    lv_label_set_text(scr_setting_common->vol_value_label, buf);
    lv_event_send((lv_obj_t*)scr_setting_common,LISA_UI_SCR_SETTING_COMMON_VOL_SLIDER_VALUE_CHANGED_EVENT,&value);

}

static void lisa_ui_scr_setting_common_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)obj;
    

    lv_obj_t *mic_cont = lv_obj_create(container);
    lv_obj_set_scroll_dir(mic_cont, LV_DIR_VER);
    lv_obj_set_size(mic_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mic_cont, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(mic_cont, 0, LV_PART_MAIN);
    
    // 创建麦克风增益标签
    scr_setting_common->mic_label = lv_label_create(mic_cont);
    lv_label_set_text(scr_setting_common->mic_label, "麦克风增益");
    lv_obj_set_style_text_font(scr_setting_common->mic_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(scr_setting_common->mic_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 创建麦克风增益滑动块
    scr_setting_common->mic_slider = lv_slider_create(mic_cont);
    lv_obj_set_height(scr_setting_common->mic_slider, 8);  
    lv_obj_set_width(scr_setting_common->mic_slider, LV_PCT(85));
    lv_obj_align_to(scr_setting_common->mic_slider, scr_setting_common->mic_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // 减少间距
    lv_slider_set_range(scr_setting_common->mic_slider, 0, 100);  // 设置范围为0-100
    lv_slider_set_value(scr_setting_common->mic_slider, 50, LV_ANIM_OFF);  // 默认值设为50
    
    // 创建显示当前值的标签
    scr_setting_common->mic_value_label = lv_label_create(mic_cont);
    lv_label_set_text(scr_setting_common->mic_value_label, "50%%");  // 默认值
    lv_obj_align_to(scr_setting_common->mic_value_label, scr_setting_common->mic_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // 添加事件处理
    lv_obj_add_event_cb(scr_setting_common->mic_slider, mic_slider_event_cb, LV_EVENT_VALUE_CHANGED, scr_setting_common);



    lv_obj_t *vol_cont = lv_obj_create(container);
    lv_obj_set_scroll_dir(vol_cont, LV_DIR_VER);
    lv_obj_set_size(vol_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(vol_cont, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(vol_cont, 0, LV_PART_MAIN);
    lv_obj_align_to(vol_cont, mic_cont, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);  // 减少间距
    
    // 创建麦克风增益标签
    scr_setting_common->vol_label = lv_label_create(vol_cont);
    lv_label_set_text(scr_setting_common->vol_label, "音量");
    lv_obj_set_style_text_font(scr_setting_common->vol_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(scr_setting_common->vol_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 创建麦克风增益滑动块
    scr_setting_common->vol_slider = lv_slider_create(vol_cont);
    lv_obj_set_height(scr_setting_common->vol_slider, 8);  
    lv_obj_set_width(scr_setting_common->vol_slider, LV_PCT(85));
    lv_obj_align_to(scr_setting_common->vol_slider, scr_setting_common->vol_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // 减少间距
    lv_slider_set_range(scr_setting_common->vol_slider, 0, 100);  // 设置范围为0-100
    lv_slider_set_value(scr_setting_common->vol_slider, 50, LV_ANIM_OFF);  // 默认值设为50
    
    // 创建显示当前值的标签
    scr_setting_common->vol_value_label = lv_label_create(vol_cont);
    lv_label_set_text(scr_setting_common->vol_value_label, "50%%");  // 默认值
    lv_obj_align_to(scr_setting_common->vol_value_label, scr_setting_common->vol_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // 添加事件处理
    lv_obj_add_event_cb(scr_setting_common->vol_slider, vol_slider_event_cb, LV_EVENT_VALUE_CHANGED, scr_setting_common);

}

void lisa_ui_scr_setting_common_add_event_cb(lv_obj_t * obj, lv_event_cb_t event_cb, lisa_ui_scr_setting_common_event_e filter,
    void * user_data){
    
    lv_obj_add_event_cb(obj, event_cb, filter, user_data);

}

void lisa_ui_scr_setting_common_mic_again_slider_value_set(lv_obj_t* obj, uint8_t value){

    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)obj;
    
    lv_slider_set_value(scr_setting_common->mic_slider, value, LV_ANIM_OFF);  // 默认值设为50
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", value);
    lv_label_set_text(scr_setting_common->mic_value_label, buf);

}

uint8_t lisa_ui_scr_setting_common_mic_again_slider_value_get(lv_obj_t* obj){
    
    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)obj;
    return lv_slider_get_value(scr_setting_common->mic_slider);
}

void lisa_ui_scr_setting_common_vol_slider_value_set(lv_obj_t* obj, uint8_t value){

    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)obj;
    
    lv_slider_set_value(scr_setting_common->vol_slider, value, LV_ANIM_OFF);  // 默认值设为50
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", value);
    lv_label_set_text(scr_setting_common->vol_value_label, buf);

}

uint8_t lisa_ui_scr_setting_common_vol_slider_value_get(lv_obj_t* obj){
    
    lisa_ui_scr_setting_common_t *scr_setting_common = (lisa_ui_scr_setting_common_t *)obj;
    return lv_slider_get_value(scr_setting_common->vol_slider);
}

lv_obj_t *lisa_ui_scr_setting_common_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}
