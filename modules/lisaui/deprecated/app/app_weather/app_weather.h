/**
 * @file app_weather.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_WEATHER_H__
#define __APP_WEATHER_H__

#ifdef __cplusplus
extern "C" {
#endif


#define WEATHER_STRING_MAX_LEN 32
#define WEATHER_CITY_STRING_MAX_LEN 32
#define WEATHER_DATE_STRING_MAX_LEN 16
#define WEATHER_ICON_PATH_STRING_MAX_LEN 64
#define WEATHER_TEMP_STRING_MAX_LEN 16
#define WEATHER_TEMP_RANGE_STRING_MAX_LEN 16
#define WEATHER_DESC_STRING_MAX_LEN 64
#define WEATHER_TYPE_STRING_MAX_LEN 16

typedef struct _metadata_weather_t {
    char weather[WEATHER_STRING_MAX_LEN]; // 天气
    char city[WEATHER_CITY_STRING_MAX_LEN]; // 城市
    char date[WEATHER_DATE_STRING_MAX_LEN]; // 日期
    char icon_path[WEATHER_ICON_PATH_STRING_MAX_LEN]; // 天气图标路径
    char icon_url[WEATHER_ICON_PATH_STRING_MAX_LEN]; // 天气图标URL
    char temperature[WEATHER_TEMP_STRING_MAX_LEN]; // 温度
    char temperature_range[WEATHER_TEMP_RANGE_STRING_MAX_LEN]; // 温度范围
    char description[WEATHER_DESC_STRING_MAX_LEN]; // 天气描述
    char type_str[WEATHER_TYPE_STRING_MAX_LEN]; // 天气类型字符串
} metadata_weather_t;

#include "../app_common.h"
#include "lisaui_app_common.h"

#if CONFIG_LVGL_ENV_SIMULATOR
#define APP_WEATHER_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_weather/" path
#define ASSETS_MGR_APP_WEATHER_UI_RES_PERFIX_PATH(path) "/SD:/lisaui/app/app_weather/" path
#else
// #define APP_WEATHER_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_weather/" path
#define APP_WEATHER_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_weather/" path
#define ASSETS_MGR_APP_WEATHER_UI_RES_PERFIX_PATH(path)
#endif

lisaui_err_t app_weather_init(void);

lisaui_err_t lisaui_app_weather_skill_show_weather_view(metadata_weather_t *weather);
lisaui_err_t lisaui_app_weather_skill_close_weather_view(void);

lisaui_err_t lisaui_app_weather_json_parse(const char *json_str, metadata_weather_t *weather);

#ifdef __cplusplus
}
#endif

#endif // __APP_WEATHER_H__
