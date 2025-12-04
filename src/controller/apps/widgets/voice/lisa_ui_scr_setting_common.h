#ifndef __LISA_UI_SCR_SETTING_COMMON_H__
#define __LISA_UI_SCR_SETTING_COMMON_H__

#include "lvgl.h"

typedef enum{
    LISA_UI_SCR_SETTING_COMMON_MIC_AGAIN_SLIDER_VALUE_CHANGED_EVENT = _LV_EVENT_LAST +1,
    LISA_UI_SCR_SETTING_COMMON_VOL_SLIDER_VALUE_CHANGED_EVENT,

}lisa_ui_scr_setting_common_event_e;

lv_obj_t *lisa_ui_scr_setting_common_create(lv_obj_t *parent);
void lisa_ui_scr_setting_common_add_event_cb(lv_obj_t * obj, 
                                                                lv_event_cb_t event_cb, 
                                                                lisa_ui_scr_setting_common_event_e filter, 
                                                                void * user_data);
void lisa_ui_scr_setting_common_mic_again_slider_value_set(lv_obj_t* obj, uint8_t value);
uint8_t lisa_ui_scr_setting_common_mic_again_slider_value_get(lv_obj_t* obj);
void lisa_ui_scr_setting_common_vol_slider_value_set(lv_obj_t* obj, uint8_t value);
uint8_t lisa_ui_scr_setting_common_vol_slider_value_get(lv_obj_t* obj);

#endif /* __LISA_UI_SCR_SETTING_COMMON_H__ */
