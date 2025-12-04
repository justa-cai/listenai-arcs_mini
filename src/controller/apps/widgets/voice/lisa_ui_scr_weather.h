#ifndef __LISA_UI_WEATHER_H__
#define __LISA_UI_WEATHER_H__

#include "lvgl.h"

lv_obj_t *lisa_ui_weather_create(lv_obj_t *parent);
int lisa_ui_weather_location_set(lv_obj_t *obj, const char *location);
int lisa_ui_weather_date_set(lv_obj_t *obj, const char *date);
int lisa_ui_weather_icon_set(lv_obj_t *obj, const char *icon_url);
int lisa_ui_weather_temp_real_set(lv_obj_t *obj, const char *temp_real);
int lisa_ui_weather_temp_range_set(lv_obj_t *obj, const char *temp_range);
int lisa_ui_weather_description_set(lv_obj_t *obj, const char *weather_description);

#endif
