/**
 * @file app_alarm_skill_view_add_alarm.c
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-24
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_alarm_skill_view_add_alarm.h"

#include "app_alarm.h"
#include "assets/assets_res.h"
#include "lisaui_app_common.h"
#include "lv_img_utils.h"

static const char *TAG = "app_alarm_skill_view_add_alarm";

static lv_obj_t *private_lisaui_app_alarm_create_alarm_view(lv_obj_t *parent, const char *alarm_time_text,
                                                            const char *alarm_date_text)
{
    lv_obj_t *alarm_page = lv_obj_create(parent);
    lv_obj_set_size(alarm_page, LV_PCT(100), LV_PCT(100));
    _lisaui_set_style_container(alarm_page, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    lv_obj_set_style_radius(alarm_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(alarm_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(alarm_page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *tip_label = lv_label_create(alarm_page);
    lv_label_set_text(tip_label, "闹钟设置成功");
    lv_obj_align(tip_label, LV_ALIGN_TOP_MID, 0, LV_DPX(LISAUI_STATUS_BAR_HEIGHT));
    lv_obj_set_style_text_color(tip_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tip_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(tip_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(tip_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *alarm_label = lv_label_create(alarm_page);
    // lv_label_set_text(alarm_label, "07:00");
    lv_label_set_text(alarm_label, alarm_time_text);

    lv_obj_align(alarm_label, LV_ALIGN_CENTER, 0, -LV_DPX(20));
    lv_obj_set_style_text_color(alarm_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(alarm_label, &lv_font_rubik_bold_64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(alarm_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(alarm_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *date_label = lv_label_create(alarm_page);
    // lv_label_set_text(date_label, "4 月 1 日 周四");
    lv_label_set_text(date_label, alarm_date_text);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -LV_DPX(20));
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(date_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(date_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    return alarm_page;
}

static lv_obj_t *g_alarm_skill_view = NULL;

lisaui_err_t lisaui_app_alarm_close_skill_alarm_view(void)
{
    LVGL_UI_LOCK();
    LISAUI_LOGI(TAG, "[%d:%s] g_alarm_skill_view: %p", __LINE__, __func__, g_alarm_skill_view);
    LVGL_OBJ_SAFE_DEL(g_alarm_skill_view);
    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_alarm_show_skill_alarm_view(const char *alarm_time_text, const char *alarm_date_text)
{
    LVGL_UI_LOCK();
    lisaui_app_alarm_close_skill_alarm_view();
    g_alarm_skill_view = private_lisaui_app_alarm_create_alarm_view(lv_layer_top(), alarm_time_text, alarm_date_text);
    LISAUI_LOGI(TAG, "[%d:%s] g_alarm_skill_view: %p", __LINE__, __func__, g_alarm_skill_view);
    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}
