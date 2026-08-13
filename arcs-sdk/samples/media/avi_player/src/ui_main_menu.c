/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui_main_menu.h"
#include "app_context.h"
#include "lvgl.h"

static lv_obj_t *s_screen;

static void btn_usb_msc_cb(lv_event_t *e)
{
    (void)e;
    app_switch_screen(SCREEN_USB_MSC);
}

static void btn_online_cb(lv_event_t *e)
{
    (void)e;
    app_switch_screen(SCREEN_ONLINE_LIST);
}

static void btn_local_cb(lv_event_t *e)
{
    (void)e;
    app_switch_screen(SCREEN_LOCAL_LIST);
}

void ui_main_menu_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_make(0x1a, 0x1a, 0x2e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);

    /* 320x240 layout: 上方单行标题，下方 3 个按钮竖排，整体居中
     *   - 标题区 ~36px 高
     *   - 按钮区 ~200px 高，每个按钮 ~56px 高（含间距），宽 78% 屏宽 */

    /* Title (top) */
    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Lisa Media Player");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* Button container (below title) */
    lv_obj_t *cont = lv_obj_create(s_screen);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(82));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Button style */
    static lv_style_t btn_style;
    lv_style_init(&btn_style);
    lv_style_set_radius(&btn_style, 8);
    lv_style_set_bg_color(&btn_style, lv_color_make(0x3a, 0x7b, 0xd5));
    lv_style_set_bg_opa(&btn_style, LV_OPA_COVER);
    lv_style_set_text_color(&btn_style, lv_color_white());
    lv_style_set_pad_hor(&btn_style, 20);
    lv_style_set_pad_ver(&btn_style, 8);

    /* USB MSC button */
    lv_obj_t *btn1 = lv_btn_create(cont);
    lv_obj_add_style(btn1, &btn_style, 0);
    lv_obj_set_width(btn1, LV_PCT(78));
    lv_obj_t *lbl1 = lv_label_create(btn1);
    lv_label_set_text(lbl1, LV_SYMBOL_USB "  USB MSC");
    lv_obj_center(lbl1);
    lv_obj_add_event_cb(btn1, btn_usb_msc_cb, LV_EVENT_CLICKED, NULL);

    /* Online Play button */
    lv_obj_t *btn2 = lv_btn_create(cont);
    lv_obj_add_style(btn2, &btn_style, 0);
    lv_obj_set_width(btn2, LV_PCT(78));
    lv_obj_t *lbl2 = lv_label_create(btn2);
    lv_label_set_text(lbl2, LV_SYMBOL_WIFI "  Online Play");
    lv_obj_center(lbl2);
    lv_obj_add_event_cb(btn2, btn_online_cb, LV_EVENT_CLICKED, NULL);

    /* Local Play button */
    lv_obj_t *btn3 = lv_btn_create(cont);
    lv_obj_add_style(btn3, &btn_style, 0);
    lv_obj_set_width(btn3, LV_PCT(78));
    lv_obj_t *lbl3 = lv_label_create(btn3);
    lv_label_set_text(lbl3, LV_SYMBOL_VIDEO "  Local Play");
    lv_obj_center(lbl3);
    lv_obj_add_event_cb(btn3, btn_local_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(s_screen);
}

void ui_main_menu_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}
