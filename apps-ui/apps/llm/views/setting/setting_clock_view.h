/**
 * @file setting_clock_view.h
 * @brief Clock/Alarm settings view
 */

#ifndef __SETTING_CLOCK_VIEW_H__
#define __SETTING_CLOCK_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_ui_llm_base.h"
#include <stdint.h>

/**
 * @brief Alarm item structure for display
 */
typedef struct {
    uint64_t timestamp;
    char text[128];
} alarm_item_t;

extern const lv_obj_class_t lisa_ui_setting_clock_view_class;

typedef void (*lisa_ui_setting_clock_back_cb_t)(void *user_data);
typedef void (*lisa_ui_setting_clock_delete_cb_t)(uint64_t timestamp);

lv_obj_t *lisa_ui_setting_clock_view_create(lv_obj_t *parent);
void lisa_ui_setting_clock_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_clock_back_cb_t cb, void *user_data);
void lisa_ui_setting_clock_view_set_delete_cb(lv_obj_t *obj, lisa_ui_setting_clock_delete_cb_t cb);
void lisa_ui_setting_clock_view_set_alarms(lv_obj_t *obj, const alarm_item_t *alarms, uint32_t count);
void lisa_ui_setting_clock_view_refresh_list(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* __SETTING_CLOCK_VIEW_H__ */
