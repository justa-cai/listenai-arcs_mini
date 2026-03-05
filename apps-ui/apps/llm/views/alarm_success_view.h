/**
 * @file alarm_success_view.h
 * @brief Alarm success page view header
 */

#ifndef __ALARM_SUCCESS_VIEW_H__
#define __ALARM_SUCCESS_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/** Alarm success view class definition */
extern const lv_obj_class_t alarm_success_view_class;

/**
 * @brief Create alarm success view object
 * 
 * @param parent Parent object, NULL for current screen
 * @return lv_obj_t* Created view object, NULL on failure
 */
lv_obj_t *alarm_success_view_create(lv_obj_t *parent);

/**
 * @brief Set alarm time text
 * 
 * @param obj View object
 * @param time_str Time string (e.g. "16:30")
 */
void alarm_success_view_set_time(lv_obj_t *obj, const char *time_str);

/**
 * @brief Set alarm date text
 * 
 * @param obj View object
 * @param date_str Date string (e.g. "12月29日 周日")
 */
void alarm_success_view_set_date(lv_obj_t *obj, const char *date_str);

#ifdef __cplusplus
}
#endif

#endif /* __ALARM_SUCCESS_VIEW_H__ */
