/**
 * @file alarm_ring_view.h
 * @brief Alarm ring page view header
 */

#ifndef __ALARM_RING_VIEW_H__
#define __ALARM_RING_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/** Alarm ring view class definition */
extern const lv_obj_class_t alarm_ring_view_class;

/**
 * @brief Create alarm ring view object
 * 
 * @param parent Parent object, NULL for current screen
 * @return lv_obj_t* Created view object, NULL on failure
 */
lv_obj_t *alarm_ring_view_create(lv_obj_t *parent);

/**
 * @brief Set alarm time text
 * 
 * @param obj View object
 * @param time_str Time string (e.g. "16:30")
 */
void alarm_ring_view_set_time(lv_obj_t *obj, const char *time_str);

/**
 * @brief Set alarm date text
 * 
 * @param obj View object
 * @param date_str Date string (e.g. "12月29日 周日")
 */
void alarm_ring_view_set_date(lv_obj_t *obj, const char *date_str);

/**
 * @brief Set stop button callback
 * 
 * @param obj View object
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void alarm_ring_view_set_stop_cb(lv_obj_t *obj, lv_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __ALARM_RING_VIEW_H__ */
