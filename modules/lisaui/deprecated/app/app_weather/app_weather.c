/**
 * @file app_weather.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_weather.h"

#include "lisaui_log.h"
#include "lv_img_utils.h"

#include "lisaui_app_common.h"
#include "lisaui_asset_manager.h"

#include "cJSON.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "app_weather";
static lv_obj_t *g_app_weather = NULL;
static lv_obj_t *g_app_panel = NULL;
static lv_obj_t *temp_weather_view = NULL;
static lisaui_resource_item_t weather_icon_item = {
    .type = LISAUI_RES_TYPE_BIN_IMAGE,
    .load_dst = LISAUI_RES_LOAD_DST_PSRAM, // 默认加载到 PSRAM
    .path = NULL,
    .data = NULL,
    .size = 0,
    .is_loaded = false,
};

UI_RES_IMG_NAME(weather_undefined, APP_WEATHER_UI_RES_PERFIX_PATH("assets/png/weather_undefined.png"));
UI_RES_IMG_NAME(weather_care_view_bg, APP_WEATHER_UI_RES_PERFIX_PATH("assets/png/weather_care_view_bg.png"))

// http://cdn9002.iflyos.cn/osweathericon/00.png
// "/SD:/lisaui/app/app_weather/png/weather_00.png"
lisaui_err_t weather_icon_url_to_path(const char *url, char *path, size_t path_len)
{
    if (url == NULL || path == NULL || path_len == 0) {
        LISAUI_LOGE(TAG, "url or path is NULL");
        return LISAUI_ERR_FAIL;
    }

    const char *last_slash = strrchr(url, '/');
    if (last_slash != NULL) {
        snprintf(path, path_len,
                 "/SD:/lisaui/app/app_weather/png/weather_"
                 "%s",
                 last_slash + 1);
    } else {
        LISAUI_LOGE(TAG, "Invalid URL format");
        return LISAUI_ERR_FAIL;
    }

    return LISAUI_ERR_OK;
}

lv_img_dsc_t icon_img_dsc;

lv_obj_t *app_weather_create_weather_view(lv_obj_t *parent, metadata_weather_t *weather)
{
    lv_obj_t *weather_view = lv_img_create(parent);
    _lisaui_set_style_container(weather_view, lv_color_hex(0x000000), 0, lv_color_hex(0x000000), 0, 0);
    lv_img_png_src_init(UI_RES_IMG_PNG(weather_care_view_bg));
    lv_img_set_src(weather_view, &LV_IMG_DSC(weather_care_view_bg));
    lv_obj_set_size(weather_view, LV_PCT(100), LV_PCT(100));

    lv_obj_t *city_label = lv_label_create(weather_view);
    lv_label_set_text(city_label, weather->city);
    lv_obj_set_style_text_color(city_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(city_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(city_label, LV_ALIGN_TOP_LEFT, LV_DPX(40), LV_DPX(LISAUI_STATUS_BAR_HEIGHT - 10));

    lv_obj_t *date_label = lv_label_create(weather_view);
    lv_label_set_text(date_label, weather->date);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(date_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align_to(date_label, city_label, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(10), 0);

    // lv_obj_t *weather_icon = lv_img_create(weather_view);
    // lv_img_set_src(weather_icon, &icon_img_weather_sunny_png);
#if CONFIG_LVGL_ENV_SIMULATOR
    LV_OBJ_ICON(weather_undefined) = lv_img_create(weather_view);
    lv_img_png_src_init(UI_RES_IMG_PNG(weather_undefined));
    lv_img_set_src(LV_OBJ_ICON(weather_undefined), &LV_IMG_DSC(weather_undefined));
    lv_obj_align(LV_OBJ_ICON(weather_undefined), LV_ALIGN_CENTER, -LV_DPX(100), 0);
#endif

#if CONFIG_LISAUI_ENV_ARCS_SDK
    if (weather->icon_url == NULL || strlen(weather->icon_url) == 0) {
        strncpy(weather->icon_url, APP_WEATHER_UI_RES_PERFIX_PATH("assets/png/weather_undefined.png"),
                sizeof(weather->icon_url) - 1);
    } else {
        // 这里需要将天气图标的 URL 转换为本地路径
        // http://cdn9002.iflyos.cn/osweathericon/00.png
        // weather->icon_path = weather->icon_url;
        // strncpy(weather->icon_path, weather->icon_url, sizeof(weather->icon_path) - 1);
        weather_icon_url_to_path(weather->icon_url, weather->icon_path, sizeof(weather->icon_path));
    }

    lv_obj_t *weather_icon = lv_img_create(weather_view);
    // weather_icon_item = {
    // };
    weather_icon_item.type = LISAUI_RES_TYPE_BIN_IMAGE;
    weather_icon_item.load_dst = LISAUI_RES_LOAD_DST_PSRAM; // 默认加载到 PSRAM
    weather_icon_item.path = weather->icon_path;
    weather_icon_item.data = NULL;
    weather_icon_item.size = 0;
    weather_icon_item.is_loaded = false;
    lisaui_asset_manager_get_asset_by_emmc_path(weather_icon_item.path, &weather_icon_item);
    if (weather_icon_item.is_loaded == false) {
        LISAUI_LOGW(TAG, "Failed to load weather icon resource from emmc");
        // weather_icon_item.path = APP_WEATHER_UI_RES_PERFIX_PATH("assets/png/weather_undefined.png");
        // lisaui_asset_manager_get_asset_by_emmc_path(weather_icon_item.path, &weather_icon_item);
        lv_img_png_src_init(UI_RES_IMG_PNG(weather_undefined));
        lv_img_set_src(weather_icon, &LV_IMG_DSC(weather_undefined));
    } else {
        LISAUI_LOGI(TAG, "Loaded weather icon resource from emmc");
        lv_img_png_src_init(&icon_img_dsc, weather_icon_item.data, weather_icon_item.size);
        lv_img_set_src(weather_icon, &icon_img_dsc);
        // lv_img_set_src(weather_icon, &weather_icon_item.data);
    }

    // ASSETS_MGR_APP_WEATHER_UI_RES_PERFIX_PATH(weather_undefined);
    lv_obj_align(weather_icon, LV_ALIGN_CENTER, -LV_DPX(100), 0);
#endif

    lv_obj_t *temp_label = lv_label_create(weather_view);
    lv_label_set_text(temp_label, weather->temperature);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp_label, &lv_font_rubik_bold_64_ascii, LV_PART_MAIN);
    lv_obj_align(temp_label, LV_ALIGN_RIGHT_MID, -LV_DPX(30), -LV_DPX(36));

    lv_obj_t *temp_range_label = lv_label_create(weather_view);
    lv_label_set_text(temp_range_label, weather->temperature_range);
    lv_obj_set_style_text_color(temp_range_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp_range_label, &lv_font_chinese_18, LV_PART_MAIN);

    lv_obj_align_to(temp_range_label, temp_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPX(10));

    lv_obj_t *description_label = lv_label_create(weather_view);
    // lv_label_set_text(description_label, weather->description);
    lv_label_set_text_fmt(description_label, "%s  %s", weather->weather, weather->description);
    lv_obj_set_style_text_color(description_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(description_label, &lv_font_chinese_18, LV_PART_MAIN);

    lv_obj_align_to(description_label, temp_range_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPX(0));

    return weather_view;
}

lisaui_err_t lisaui_app_weather_skill_show_weather_view(metadata_weather_t *weather)
{
    LVGL_UI_LOCK();
    if (g_app_weather == NULL) {
        LISAUI_LOGE(TAG, "g_app_weather is NULL");
        LVGL_UI_UNLOCK();
        return LISAUI_ERR_FAIL;
    }

    LVGL_OBJ_SAFE_DEL(temp_weather_view);
    // lisaui_app_enter(UI_APP_ID_WEATHER);
    temp_weather_view = app_weather_create_weather_view(g_app_panel, weather);

    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_weather_skill_close_weather_view(void)
{
    LVGL_UI_LOCK();
    LVGL_OBJ_SAFE_DEL(temp_weather_view);
    LISAUI_ASSET_SAFE_FREE(&weather_icon_item);
    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}

lisaui_err_t app_weather_create(void *parent)
{
    if (g_app_weather != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_weather = lv_obj_create(parent);
    _lisaui_set_style_container(g_app_weather, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    g_app_panel = lv_obj_create(g_app_weather);
    _lisaui_set_style_container(g_app_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    // lv_obj_set_size(g_app_panel, LV_PCT(100), LV_PCT(80));
    // lv_obj_set_y(g_app_panel, LV_DPX(LISAUI_STATUS_BAR_HEIGHT));

    lv_obj_set_size(g_app_panel, LV_PCT(100), LV_PCT(100));

    LISAUI_LOGI(TAG, "[%d:%s] create\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_weather_destroy(void)
{
    LVGL_OBJ_SAFE_DEL(g_app_weather);

    LISAUI_LOGI(TAG, "[%d:%s] destroy\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_weather_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_weather_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_weather_get_view(void)
{
    return g_app_weather;
}

static struct app_icon_t app_icon_res_weather = {
    .title = "天气",
    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = NULL, //&icon_img_app_store_png,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_weather = {
    .create = app_weather_create,
    .destroy = app_weather_destroy,
    .enter = app_weather_enter,
    .exit = app_weather_exit,

    .get_app_view = app_weather_get_view,
    .info =
        {
            .name = "Weather",
            .package_name = "com.listenai.lisaui.weather",
            .id = UI_APP_ID_WEATHER,
        },
    .icon = &app_icon_res_weather,
};

lisaui_err_t app_weather_init(void)
{
    lisaui_app_register(&app_weather);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(weather, &app_weather, app_weather_init);

lisaui_err_t private_json_parse_item(cJSON *json, const char *key, char *value, size_t value_len)
{
    cJSON *item = cJSON_GetObjectItem(json, key);
    if (item && cJSON_IsString(item)) {
        strncpy(value, item->valuestring,
                strlen(item->valuestring) > value_len ? value_len - 1 : strlen(item->valuestring));
        value[value_len - 1] = '\0';
    } else {
        LISAUI_LOGE(TAG, "Failed to parse JSON item: %s", key);
        return LISAUI_ERR_FAIL;
    }
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_weather_json_parse(const char *json_str, metadata_weather_t *weather)
{
    // LISAUI_LOGI(TAG, "[%s] json_str: %s", __FUNCTION__, json_str);
    if (json_str == NULL || weather == NULL) {
        LISAUI_LOGE(TAG, "json_str or weather is NULL");
        return LISAUI_ERR_FAIL;
    }

    cJSON *weather_json = cJSON_Parse(json_str);
    if (weather_json == NULL) {
        LISAUI_LOGE(TAG, "cJSON_Parse failed");
        return LISAUI_ERR_FAIL;
    }
    // LISAUI_LOGI(TAG, "[%s] weather_json: %s", __FUNCTION__, cJSON_Print(weather_json));

    private_json_parse_item(weather_json, "city", weather->city, WEATHER_CITY_STRING_MAX_LEN);
    private_json_parse_item(weather_json, "weather", weather->weather, WEATHER_STRING_MAX_LEN);
    private_json_parse_item(weather_json, "date", weather->date, WEATHER_DATE_STRING_MAX_LEN);
    private_json_parse_item(weather_json, "img", weather->icon_url, WEATHER_ICON_PATH_STRING_MAX_LEN);
    private_json_parse_item(weather_json, "tempReal", weather->temperature, WEATHER_TEMP_STRING_MAX_LEN);
    private_json_parse_item(weather_json, "tempRange", weather->temperature_range, WEATHER_TEMP_RANGE_STRING_MAX_LEN);
    private_json_parse_item(weather_json, "weatherDescription", weather->description, WEATHER_DESC_STRING_MAX_LEN);

    cJSON_Delete(weather_json);

    LISAUI_LOGD(TAG, "[%s] weather city: %s", __FUNCTION__, weather->city);
    LISAUI_LOGD(TAG, "[%s] weather weather: %s", __FUNCTION__, weather->weather);
    LISAUI_LOGD(TAG, "[%s] weather date: %s", __FUNCTION__, weather->date);
    LISAUI_LOGD(TAG, "[%s] weather icon_url: %s", __FUNCTION__, weather->icon_url);
    LISAUI_LOGD(TAG, "[%s] weather temperature: %s", __FUNCTION__, weather->temperature);
    LISAUI_LOGD(TAG, "[%s] weather temperature_range: %s", __FUNCTION__, weather->temperature_range);
    LISAUI_LOGD(TAG, "[%s] weather description: %s", __FUNCTION__, weather->description);

    return LISAUI_ERR_OK;
}
