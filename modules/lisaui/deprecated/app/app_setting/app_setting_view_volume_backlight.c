/**
 * @file app_setting_view_volume_backlight.c
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-15
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_setting_view_volume_backlight.h"

#include "app_setting.h"
#include "assets/assets_res.h"
#include "lisaui_app_common.h"
#include "lv_img_utils.h"

static const char *TAG = "app_setting_view_volume_backlight";

// UI_RES_IMG_NAME(back_btn, LISAUI_APP_SETTING_UI_RES_PERFIX_PATH("assets/png/icon_back.png"))
UI_RES_IMG_NAME(volume_min, LISAUI_APP_SETTING_UI_RES_PERFIX_PATH("assets/png/ic_volume_low.png"))
UI_RES_IMG_NAME(volume_max, LISAUI_APP_SETTING_UI_RES_PERFIX_PATH("assets/png/ic_volume_high.png"))
UI_RES_IMG_NAME(brightness_min, LISAUI_APP_SETTING_UI_RES_PERFIX_PATH("assets/png/ic_brightness_low.png"))
UI_RES_IMG_NAME(brightness_max, LISAUI_APP_SETTING_UI_RES_PERFIX_PATH("assets/png/ic_brightness_high.png"))
UI_RES_IMG_NAME(setting_btn, LISAUI_APP_SETTING_UI_RES_PERFIX_PATH("assets/png/ic_launch_setting.png"))

static lv_obj_t *line_setting_floor;
static lv_obj_t *setting_home;
static lv_obj_t *slider_voice;
static lv_obj_t *slider_luminance;
static lv_obj_t *icon_setting_btn_bg;

static lv_point_t lv_setting_line_floor[2];
static lv_style_t style_setting;

static const lv_font_t *font_normal;

static lisaui_app_setting_handler_t m_item_op_handler = NULL;
lisaui_err_t lisaui_app_setting_register_handler(lisaui_app_setting_handler_t handler)
{
    if (handler == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }
    m_item_op_handler = handler;
    return LISAUI_ERR_OK;
}

static void lisaui_app_setting_event_slider_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    // if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
    //     lv_coord_t *s = lv_event_get_param(e);
    //     *s = LV_MAX(*s, 60);
    // } else
    if (code == LV_EVENT_DRAW_PART_END) {
        // lv_obj_draw_part_dsc_t *dsc = lv_event_get_param(e);
        // if (dsc->part == LV_PART_KNOB && lv_obj_has_state(obj, LV_STATE_PRESSED)) {
        //     char buf[8];
        //     lv_snprintf(buf, sizeof(buf), "%" LV_PRId32, lv_slider_get_value(obj));

        //     lv_point_t text_size;
        //     lv_txt_get_size(&text_size, buf, font_normal, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

        //     lv_area_t txt_area;
        //     txt_area.x1 = dsc->draw_area->x1 + lv_area_get_width(dsc->draw_area) / 2 - text_size.x / 2;
        //     txt_area.x2 = txt_area.x1 + text_size.x;
        //     txt_area.y2 = dsc->draw_area->y1 - 10;
        //     txt_area.y1 = txt_area.y2 - text_size.y;

        //     lv_area_t bg_area;
        //     bg_area.x1 = txt_area.x1 - LV_DPX(8);
        //     bg_area.x2 = txt_area.x2 + LV_DPX(8);
        //     bg_area.y1 = txt_area.y1 - LV_DPX(8);
        //     bg_area.y2 = txt_area.y2 + LV_DPX(8);

        //     lv_draw_rect_dsc_t rect_dsc;
        //     lv_draw_rect_dsc_init(&rect_dsc);
        //     rect_dsc.bg_color = lv_palette_darken(LV_PALETTE_GREY, 3);
        //     rect_dsc.radius = LV_DPX(5);
        //     lv_draw_rect(dsc->draw_ctx, &rect_dsc, &bg_area);

        //     lv_draw_label_dsc_t label_dsc;
        //     lv_draw_label_dsc_init(&label_dsc);
        //     label_dsc.color = lv_color_white();
        //     label_dsc.font = font_normal;
        //     lv_draw_label(dsc->draw_ctx, &label_dsc, &txt_area, buf, NULL);
        // }
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        if (obj == slider_luminance) {
            if (m_item_op_handler) {
                uint8_t value = lv_slider_get_value(obj);
                m_item_op_handler(LISAUI_SETTING_ITEM_BACKLIGHT, LISAUI_SETTING_OP_SET, &value);
            }
        } else if (obj == slider_voice) {
            if (m_item_op_handler) {
                uint8_t value = lv_slider_get_value(obj);
                m_item_op_handler(LISAUI_SETTING_ITEM_VOLUME, LISAUI_SETTING_OP_SET, &value);
            }
        }
    }
}

lv_obj_t *private_lisaui_app_view_create_volume_backlight(lv_obj_t *parent)
{

    setting_home = lv_obj_create(parent);
    lv_obj_remove_style_all(setting_home);
    lv_obj_clear_flag(setting_home, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    // lv_obj_set_style_bg_color(setting_home, lv_color_hex(0x242430), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(setting_home, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(setting_home, 250, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(setting_home, LV_PCT(100), LV_PCT(100));

    // 亮度滑块
    slider_luminance = lv_slider_create(setting_home);
    lv_obj_set_style_bg_color(slider_luminance, lv_color_hex(0x454554), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider_luminance, lv_color_hex(0xb0b0b0), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider_luminance, lv_color_hex(0xa0a0a0), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(slider_luminance, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_align(slider_luminance, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(slider_luminance, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(slider_luminance, LV_DPX(0), LV_DPX(20));
    lv_obj_set_width(slider_luminance, LV_PCT(80)); /*Set the width*/
    lv_obj_set_height(slider_luminance, 60);
    lv_obj_set_style_width(slider_luminance, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(slider_luminance, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(slider_luminance, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(slider_luminance, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(slider_luminance, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider_luminance, 10, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider_luminance, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider_luminance, 10, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_slider_set_range(slider_luminance, 2, 100);
    lv_obj_add_event_cb(slider_luminance, lisaui_app_setting_event_slider_handler, LV_EVENT_VALUE_CHANGED, NULL);
    // /*Assign an event

    // 音量滑块
    slider_voice = lv_slider_create(setting_home);
    lv_obj_set_style_bg_color(slider_voice, lv_color_hex(0x454554), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider_voice, lv_color_hex(0xb0b0b0), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider_voice, lv_color_hex(0xF0F0F0), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(slider_voice, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(slider_voice, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(slider_voice, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(slider_voice, 0, LV_DPX(120));
    lv_obj_set_width(slider_voice, LV_PCT(80)); /*Set the width*/
    lv_obj_set_height(slider_voice, 60);
    lv_obj_set_style_pad_top(slider_voice, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(slider_voice, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(slider_voice, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(slider_voice, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider_voice, 10, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider_voice, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider_voice, 10, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(slider_voice, lisaui_app_setting_event_slider_handler, LV_EVENT_VALUE_CHANGED,
                        NULL); /*Assign an event function*/

    // 图标
    LV_OBJ_ICON(brightness_min) = lv_img_create(slider_luminance);

    lv_img_png_src_init(UI_RES_IMG_PNG(brightness_min));
    lv_img_set_src(LV_OBJ_ICON(brightness_min), &LV_IMG_DSC(brightness_min));

    lv_obj_align(LV_OBJ_ICON(brightness_min), LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_flag(LV_OBJ_ICON(brightness_min), LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(LV_OBJ_ICON(brightness_min), LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_add_event_cb(LV_OBJ_ICON(brightness_min), ui_event_setting, LV_EVENT_RELEASED, NULL);
    // lv_obj_add_event_cb(LV_OBJ_ICON(brightness_min), ui_event_setting, LV_EVENT_PRESSED, NULL);
    lv_obj_add_style(LV_OBJ_ICON(brightness_min), &style_setting, 0);
    lv_style_init(&style_setting);

    LV_OBJ_ICON(brightness_max) = lv_img_create(slider_luminance);
    lv_img_png_src_init(UI_RES_IMG_PNG(brightness_max));
    // lv_img_png_src_init(&icon_brightness_max_png, icon_brightness_max_png_data, icon_brightness_max_png_size);
    lv_img_set_src(LV_OBJ_ICON(brightness_max), &LV_IMG_DSC(brightness_max));
    lv_obj_align(LV_OBJ_ICON(brightness_max), LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_flag(LV_OBJ_ICON(brightness_max), LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(LV_OBJ_ICON(brightness_max), LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_add_event_cb(lv_obj_icon_brightness_max, ui_event_setting, LV_EVENT_RELEASED, NULL);
    // lv_obj_add_event_cb(lv_obj_icon_brightness_max, ui_event_setting, LV_EVENT_PRESSED, NULL);
    lv_obj_add_style(LV_OBJ_ICON(brightness_max), &style_setting, 0);
    lv_style_init(&style_setting);
    lv_style_set_opa(&style_setting, LV_OPA_80);
    lv_obj_refresh_style(LV_OBJ_ICON(brightness_max), 0, 0);

    LV_OBJ_ICON(volume_min) = lv_img_create(slider_voice);
    lv_img_png_src_init(UI_RES_IMG_PNG(volume_min));
    lv_img_set_src(LV_OBJ_ICON(volume_min), &LV_IMG_DSC(volume_min));
    lv_obj_align(LV_OBJ_ICON(volume_min), LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_flag(LV_OBJ_ICON(volume_min), LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(LV_OBJ_ICON(volume_min), LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_add_event_cb(LV_OBJ_ICON(volume_min), ui_event_setting, LV_EVENT_RELEASED, NULL);
    // lv_obj_add_event_cb(LV_OBJ_ICON(volume_min), ui_event_setting, LV_EVENT_PRESSED, NULL);
    lv_obj_add_style(LV_OBJ_ICON(volume_min), &style_setting, 0);
    lv_style_init(&style_setting);
    LV_OBJ_ICON(volume_max) = lv_img_create(slider_voice);
    lv_img_png_src_init(UI_RES_IMG_PNG(volume_max));
    // lv_img_png_src_init(&icon_volume_max_png, gicon_volume_max_pngData, gicon_volume_max_pngSize);
    lv_img_set_src(LV_OBJ_ICON(volume_max), &LV_IMG_DSC(volume_max));
    lv_obj_align(LV_OBJ_ICON(volume_max), LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_flag(LV_OBJ_ICON(volume_max), LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(LV_OBJ_ICON(volume_max), LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_add_event_cb(LV_OBJ_ICON(volume_max), ui_event_setting, LV_EVENT_RELEASED, NULL);
    // lv_obj_add_event_cb(LV_OBJ_ICON(volume_max), ui_event_setting, LV_EVENT_PRESSED, NULL);
    lv_obj_add_style(LV_OBJ_ICON(volume_max), &style_setting, 0);
    lv_style_init(&style_setting);

    if (m_item_op_handler) {
        uint8_t value = 0;
        m_item_op_handler(LISAUI_SETTING_ITEM_VOLUME, LISAUI_SETTING_OP_GET, &value);
        lv_slider_set_value(slider_voice, value, LV_ANIM_OFF);
    }
    if (m_item_op_handler) {
        uint8_t value = 0;
        m_item_op_handler(LISAUI_SETTING_ITEM_BACKLIGHT, LISAUI_SETTING_OP_GET, &value);
        lv_slider_set_value(slider_luminance, value, LV_ANIM_OFF);
    }
    return parent;
}
