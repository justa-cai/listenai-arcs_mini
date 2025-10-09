/**
 * @file app_dictionary.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-03-03
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_dictionary.h"
#include "lisaui_app_common.h"
#include "../app_common.h"
#include "assets/assets_res.h"
#include "lisaui_log.h"
#include "lv_img_utils.h"

static lv_img_dsc_t img_gif5;

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_dictionary/" path
#else
// #define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_dictionary/" path
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_dictionary/" path
// app/llm/lisaui/app/app_wifi/assets/gif/anim_circle.gif
#endif

INCBIN(img_gif5, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/gif/00b44af.gif"));

static lv_img_dsc_t img_favorite;
static lv_img_dsc_t img_book;
static lv_img_dsc_t img_voice;
INCBIN(img_favorite, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/icons/favorite.png"));
INCBIN(img_book, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/icons/books.png"));
INCBIN(img_voice, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/icons/voice.png"));

static const char *TAG = "app_dictionary";
struct _explain_t {
    const char *tittle;
    const char *description;
};

#define DESCRIPTION_INDEX2                                                                                             \
    "〈动〉(1) (形声。从耳，令声。本义：细听) [3](2) 同本义 [hear]聆广乐之九奏兮。——张衡《思玄赋》\
    宝玉接过来，一面目视其文，耳聆其歌。——《红楼梦》\
    扣而聆之。——宋· 苏轼《石钟山记》\
    (3) 又如：聆偈(听经)；聆受(倾听并接受)；聆训(听受训教)；聆教(聆听教晦)；聆音察理(听到声音就能明察事理)\
    (4) 明了，清楚 [understand]\
    观读之者，晓然若盲之开目，聆然若聋之通耳。——汉· 王充《论衡》 [3]\
    (5) 又如：聆聆(明了，清楚) [1]"

static struct _explain_t explain_list[] = {
    {"[古籍释义]", "《广韵》《集韵》《韵会》𠀤余六切，音育。地之肥也。以其能生长万物，故从育从土地之肥也。以其能生长万"
                   "物，故从育从土以其能生长万物，故从育从土"},
    {"基本字义", "#0000ff 聆#líng ⒈ 听：聆听。聆取。聆教（jiào ）"},
    {"基本词义", DESCRIPTION_INDEX2},
    {"释义", "#0000ff 聆听#。#0000ff 聆取#。#00ff00 聆教（jiao）#"},
    {"同义词", "XXX"},
    {"反义词", "XXX"},
};

/* -------------------------------------------------------------------------- */

static lv_style_t style_box;

static void menu_anim_cb(void *var, int32_t v)
{
    lv_obj_set_x(var, v);
}

static lv_anim_t anim;
static lv_obj_t *menu;
static lv_obj_t *menu_tips;

static void show_menu()
{
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, menu);
    lv_anim_set_values(&anim, -LV_PCT(100) + 100, 0); // 从左侧滑入
    lv_anim_set_time(&anim, 500);                      // 动画时间
    lv_anim_set_exec_cb(&anim, menu_anim_cb);
    // lv_anim_set_path_cb(&anim, bezier_path_cb); // 使用自定义贝塞尔曲线路径
    lv_anim_start(&anim);
    // lv_obj_set_hidden(menu, false);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
}

static void hide_menu()
{
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, menu);
    lv_anim_set_values(&anim, 0, -LV_PCT(100) + 100); // 滑出到左侧
    lv_anim_set_time(&anim, 500);                      // 动画时间
    lv_anim_set_exec_cb(&anim, menu_anim_cb);
    // lv_anim_set_path_cb(&anim, bezier_path_cb); // 使用自定义贝塞尔曲线路径
    lv_anim_start(&anim);
    // lv_obj_set_hidden(menu, true);
    lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
}

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    LISAUI_LOGI(TAG, "menu event code: 0x%02X(%d)\r\n", code, code);
    if (code == LV_EVENT_CLICKED) {
        if (lv_obj_get_x(menu) < 0) {
            show_menu();
            lv_group_focus_obj(menu);
            lv_obj_add_state(menu, LV_STATE_FOCUSED);
            LISAUI_LOGI(TAG, "menu show\r\n");
        } else {
            hide_menu();
        }
    }
}

static void focus_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_DEFOCUSED) {
        if (obj == menu) {
            // if(obj == menu || obj == btn) {
            LISAUI_LOGI(TAG, "menu defocused\r\n");
            hide_menu();
        }
    } else if (code == LV_EVENT_FOCUSED) {
        LISAUI_LOGI(TAG, "menu focused\r\n");
    }
    // LISAUI_LOGI(TAG, "menu event code: 0x%02X(%d)\r\n", code, code);
}

lv_obj_t *lisaui_explain_menu_create(lv_obj_t *parent)
{

#if 1
    lv_obj_t *uiIcon_Book = lv_img_create(parent);
    lv_img_png_src_init(&img_book, gimg_bookData, gimg_bookSize);
    lv_img_set_src(uiIcon_Book, &img_book);
    lv_obj_set_size(uiIcon_Book, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // lv_obj_set_pos(uiIcon_Book, 0, 20);
    // lv_obj_set_align(uiIcon_Book, LV_ALIGN_TOP_RIGHT);
    lv_obj_align(uiIcon_Book, LV_ALIGN_TOP_RIGHT, -LV_DPX(20), LV_DPX(20+LISAUI_STATUS_BAR_HEIGHT) );

    lv_obj_t *uiIcon_Voice = lv_img_create(parent);
    lv_img_png_src_init(&img_voice, gimg_voiceData, gimg_voiceSize);
    lv_img_set_src(uiIcon_Voice, &img_voice);
    lv_obj_set_size(uiIcon_Voice, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // lv_obj_set_pos(uiIcon_Voice, 0, 60);
    // lv_obj_set_align(uiIcon_Voice, LV_ALIGN_TOP_RIGHT);
    lv_obj_align(uiIcon_Voice, LV_ALIGN_TOP_RIGHT, -LV_DPX(20), LV_DPX(80+LISAUI_STATUS_BAR_HEIGHT) );

    lv_obj_t *uiIcon_Favorite = lv_img_create(parent);
    lv_img_png_src_init(&img_favorite, gimg_favoriteData, gimg_favoriteSize);
    lv_img_set_src(uiIcon_Favorite, &img_favorite);
    lv_obj_set_size(uiIcon_Favorite, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // lv_obj_set_pos(uiIcon_Favorite, 0, 100);
    // lv_obj_set_align(uiIcon_Favorite, LV_ALIGN_TOP_RIGHT);
    lv_obj_align(uiIcon_Favorite, LV_ALIGN_TOP_RIGHT, -LV_DPX(20), LV_DPX(140+LISAUI_STATUS_BAR_HEIGHT) );

    // lv_obj_t * btn = lv_btn_create(parent);
    // lv_obj_set_size(btn, 100, 50);
    // lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 10);
    // lv_obj_t * btn_label = lv_label_create(btn);
    // lv_label_set_text(btn_label, "Menu");

    menu = lv_obj_create(lv_layer_top());
    lv_obj_set_size(menu, LV_PCT(100) - 100, lv_obj_get_height(lv_layer_top()));
    lv_obj_set_style_bg_color(menu, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(menu, 0, 0);
    lv_obj_set_style_border_width(menu, 0, 0);
    lv_obj_set_style_outline_width(menu, 0, 0);
    lv_obj_align(menu, LV_ALIGN_LEFT_MID, -200, 0); // 初始位置在屏幕左侧外部
    lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *status_bar_tabview = lv_tabview_create(menu, LV_DIR_RIGHT, 80);
    lv_obj_set_size(status_bar_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(status_bar_tabview, lv_color_hex(0x1E1E1E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(status_bar_tabview, LV_BORDER_SIDE_RIGHT, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t *tab1 = lv_tabview_add_tab(status_bar_tabview, "Books");
    lv_obj_t *tab2 = lv_tabview_add_tab(status_bar_tabview, "Favorites");

    menu_tips = lv_label_create(menu);
    lv_label_set_text(menu_tips, "Menu");
    lv_obj_set_style_text_color(menu_tips, lv_color_hex(0xFFFFFF), 0);
    // lv_obj_set_click(uiIcon_Book, true);
    lv_obj_add_flag(uiIcon_Book, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(uiIcon_Book, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(menu, focus_event_cb, LV_EVENT_ALL, NULL);
#else

    // lv_obj_t * label1 = lv_label_create(lv_scr_act());
    lv_obj_t *label1 = lv_label_create(uiApp_DictionaryPanel);
    lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP); /*Break the long lines*/
    lv_label_set_recolor(label1, true);                 /*Enable re-coloring by commands in the text*/
    lv_label_set_text(label1, "#0000ff Re-color# #ff00ff words# #ff0000 of a# label, align the lines to the center "
                              "and wrap long text automatically.");
    lv_obj_set_width(label1, 150); /*Set smaller width to make the lines wrap*/
    lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *label2 = lv_label_create(uiApp_DictionaryPanel);
    lv_label_set_long_mode(label2, LV_LABEL_LONG_SCROLL_CIRCULAR); /*Circular scroll*/
    lv_obj_set_width(label2, 150);
    lv_label_set_text(label2, "It is a circularly scrolling text. ");
    lv_obj_align(label2, LV_ALIGN_CENTER, 0, 50);
#endif
    return menu;
}

static lv_obj_t *img_gif = NULL;
static void lisaui_img_event_cb(lv_event_t *e)
{
    static bool is_pause = false;
    // printk("gif event code : %d\n", e->code);
    if (e->code == LV_EVENT_CLICKED) {
        lv_obj_t *img_gif = lv_event_get_target(e);
        if (is_pause) {
            // lv_gif_resume(img_gif);
        } else {
            // lv_gif_pause(img_gif);
        }
        is_pause = !is_pause;
    }
}

lisaui_err_t _lisaui_style_set_page(lv_obj_t *page, lv_color_t bg_color, lv_opa_t bg_opa, lv_color_t border_color,
    lv_coord_t border_width, lv_coord_t radius)
{
    if (page == NULL)
        return LISAUI_ERR_INVALID_PARAM;
    // 设置 Flex 布局和对齐方式
    lv_obj_set_layout(page, LV_LAYOUT_FLEX); // 设置为Flex布局
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(page, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLL_ELASTIC);

    // lv_obj_set_width(page, LV_PCT(100));
    // lv_obj_set_height(page, LV_PCT(100) - 20);
    lv_obj_set_size(page, LV_PCT(86), LV_SIZE_CONTENT);
    lv_obj_set_y(page, LV_DPX(LISAUI_STATUS_BAR_HEIGHT));

    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(page, LV_DIR_TOP | LV_DIR_BOTTOM); // 仅允许上下滑动
    lv_obj_set_scrollbar_mode(page, LV_SCROLL_SNAP_NONE);
    // lv_obj_set_style_border_side(base_panel, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_all(base_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_border_width(base_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_outline_width(base_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(base_panel, lv_color_hex(0x00000), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(base_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    _lisaui_set_style_container(page, bg_color, bg_opa, border_color, border_width, radius);

    return LISAUI_ERR_OK;
}

lv_obj_t *lisaui_explain_panel_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *base_panel = lv_obj_create(parent);
    _lisaui_style_set_page(base_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    lv_obj_t *img_container = lv_obj_create(base_panel);
    _lisaui_set_style_container(img_container, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    // lv_obj_set_size(img_container, SCREEN_WIDTH - 100, 100);
    lv_obj_set_width(img_container, LV_PCT(100));
    lv_obj_set_height(img_container, LV_SIZE_CONTENT);
    lv_obj_set_scrollbar_mode(img_container, LV_SCROLL_SNAP_NONE);
    // lv_obj_set_style_border_side(img_container, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(img_container, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(img_container, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *img_gif = lv_gif_create(img_container);
    // lv_gif_set_src(img2, LV_SYMBOL_GIF_FILE_PATH_2);
    lv_img_gif_src_init(&img_gif5, gimg_gif5Data, gimg_gif5Size);
    lv_gif_set_src(img_gif, &img_gif5);
    lv_obj_align(img_gif, LV_ALIGN_TOP_LEFT, LV_DPX(10), 0);
    lv_obj_add_flag(img_gif, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(img_gif, lisaui_img_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *uiDictionary_pinyinBtn = lv_btn_create(img_container);
    lv_obj_set_size(uiDictionary_pinyinBtn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(uiDictionary_pinyinBtn, img_gif, LV_ALIGN_OUT_TOP_RIGHT, 40, 40);

    lv_obj_t *uiDictionary_pinyin = lv_label_create(uiDictionary_pinyinBtn);
    lv_label_set_text(uiDictionary_pinyin, "读作堉 yù");
    lv_obj_set_style_text_font(uiDictionary_pinyin, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(uiDictionary_pinyin, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_align_to(uiDictionary_pinyin, img_gif, LV_ALIGN_OUT_TOP_RIGHT, 100, 40);

    lv_obj_t *uiDictionary_explain = lv_label_create(img_container);
    lv_label_set_text(uiDictionary_explain, "#FFFFFF 释义：# #FFFF00 肥沃的土地#");
    lv_label_set_recolor(uiDictionary_explain, true); /*Enable re-coloring by commands in the text*/
    // lv_label_set_text(uiDictionary_explain, "释义：肥沃的土地");
    // lv_label_set_text(uiDictionary_explain, "#0000ff Re-color# #ff00ff words# #ff0000 of a# label, align the lines to
    // the center "
    //     "and wrap long text automatically.");
    lv_obj_set_style_text_font(uiDictionary_explain, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_text_color(uiDictionary_explain, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(uiDictionary_explain, uiDictionary_pinyin, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);

    static lv_style_t style;
    lv_style_init(&style);

    lv_style_set_radius(&style, 3);

    lv_style_set_bg_opa(&style, LV_OPA_100);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_VER);

    lv_style_set_border_opa(&style, LV_OPA_40);
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_GREY));

    // lv_style_set_shadow_width(&style, 8);
    // lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_GREY));
    // lv_style_set_shadow_ofs_y(&style, 8);

    lv_style_set_outline_opa(&style, LV_OPA_COVER);
    lv_style_set_outline_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_pad_all(&style, 10);

    /*Init the pressed style*/
    static lv_style_t style_pr;
    lv_style_init(&style_pr);

    /*Add a large outline when pressed*/
    lv_style_set_outline_width(&style_pr, 30);
    lv_style_set_outline_opa(&style_pr, LV_OPA_TRANSP);

    lv_style_set_translate_y(&style_pr, 5);
    lv_style_set_shadow_ofs_y(&style_pr, 3);
    lv_style_set_bg_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 4));

    /*Add a transition to the outline*/
    static lv_style_transition_dsc_t trans;
    static lv_style_prop_t props[] = {LV_STYLE_OUTLINE_WIDTH, LV_STYLE_OUTLINE_OPA, 0};
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_linear, 300, 0, NULL);

    lv_style_set_transition(&style_pr, &trans);

    lv_obj_t *explain_container = lv_obj_create(base_panel);
    _lisaui_style_set_page(explain_container, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    // _lisaui_set_style_container(explain_container, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    // // lv_obj_set_size(explain_container, SCREEN_WIDTH - 100, 100);
    lv_obj_set_width(explain_container, LV_PCT(100));
    // lv_obj_set_height(explain_container, LV_SIZE_CONTENT);
    // lv_obj_set_scrollbar_mode(explain_container, LV_SCROLL_SNAP_NONE);

    lv_obj_t *ui_explain_dict_text = NULL;
    lv_obj_t *uiApp_DictionaryBtnStart1 = NULL;
    for (int i = 0; i < sizeof(explain_list) / sizeof(struct _explain_t); i++) {
        uiApp_DictionaryBtnStart1 = lv_btn_create(explain_container);
        lv_obj_remove_style_all(uiApp_DictionaryBtnStart1); /*Remove the style coming from the theme*/
        lv_obj_add_style(uiApp_DictionaryBtnStart1, &style, 0);
        lv_obj_add_style(uiApp_DictionaryBtnStart1, &style_pr, LV_STATE_PRESSED);
        
        lv_obj_align(uiApp_DictionaryBtnStart1, LV_ALIGN_TOP_LEFT, 100, 20 + i * 100);
        lv_obj_set_size(uiApp_DictionaryBtnStart1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        lv_obj_t *ui_BtnTranslate = lv_label_create(uiApp_DictionaryBtnStart1);
        lv_label_set_text(ui_BtnTranslate, explain_list[i].tittle);
        lv_obj_set_style_text_font(ui_BtnTranslate, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_BtnTranslate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_explain_dict_text = lv_label_create(explain_container);
        lv_obj_set_width(ui_explain_dict_text, LV_PCT(100));
        lv_obj_set_height(ui_explain_dict_text, LV_SIZE_CONTENT);
        lv_label_set_text(ui_explain_dict_text, explain_list[i].description);
        lv_label_set_long_mode(ui_explain_dict_text, LV_LABEL_LONG_WRAP);     /*Circular scroll*/
        // lv_label_set_long_mode(ui_explain_dict_text, LV_LABEL_LONG_SCROLL_CIRCULAR);     /*Circular scroll*/

        lv_obj_set_style_text_color(ui_explain_dict_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_explain_dict_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    #if CONFIG_LISAUI_FONT_LANGUAGE_ZH_CN_ENABLE
        lv_obj_set_style_text_font(ui_explain_dict_text, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    #else
        lv_obj_set_style_text_font(ui_explain_dict_text, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    #endif
    }

    // lv_obj_t *uiApp_DictionaryBtnStart1 = lv_btn_create(base_panel);
    lv_obj_set_size(uiApp_DictionaryBtnStart1, 100, 40);
    lv_obj_align_to(uiApp_DictionaryBtnStart1, ui_explain_dict_text, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 40);
    lv_obj_add_flag(uiApp_DictionaryBtnStart1, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(uiApp_DictionaryBtnStart1, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_add_style(uiApp_DictionaryBtnStart1, &style, 0);
    lv_obj_add_style(uiApp_DictionaryBtnStart1, &style_pr, LV_STATE_PRESSED);

    lv_obj_t *ui_BtnTranslate = lv_label_create(uiApp_DictionaryBtnStart1);
    lv_label_set_text(ui_BtnTranslate, "详细解释");
    lv_obj_set_style_text_font(ui_BtnTranslate, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *uiApp_Demo_Text = lv_textarea_create(base_panel);
    lv_obj_align_to(uiApp_Demo_Text, ui_explain_dict_text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 100);
    lv_obj_set_size(uiApp_Demo_Text, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(uiApp_Demo_Text, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(uiApp_Demo_Text, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_textarea_set_text(uiApp_Demo_Text, UI_TEST_STR1);

    lv_obj_set_style_text_color(uiApp_Demo_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(uiApp_Demo_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(uiApp_Demo_Text, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(uiApp_Demo_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(uiApp_Demo_Text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(uiApp_Demo_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(uiApp_Demo_Text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    return base_panel;
}

/* -------------------------------------------------------------------------- */
static lv_obj_t *g_app_dictionary = NULL;
lisaui_err_t app_dictionary_create(void *parent)
{
    if (g_app_dictionary != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_dictionary = lv_obj_create(parent);
    // // _lisaui_set_style_container(g_app_dictionary, lv_color_hex(0x1F1F1F), 255, lv_color_hex(0x000000), 0, 0);
    _lisaui_set_style_container(g_app_dictionary, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    // lv_obj_set_style_local_pad_all(g_app_dictionary, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 30);
    // lv_obj_set_style_local_margin_all(g_app_dictionary, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 30);
    lv_obj_t *base_panel = lisaui_explain_panel_create(g_app_dictionary);
    lv_obj_set_x(base_panel, LV_DPX(20));
    lv_obj_set_y(base_panel, LV_DPX(LISAUI_STATUS_BAR_HEIGHT));

    _lisaui_set_style_container(base_panel, lv_color_hex(0x1F1F1F), 255, lv_color_hex(0x000000), 0, 0);

    lisaui_explain_menu_create(g_app_dictionary);

    LISAUI_LOGI(TAG, "[%d:%s] create\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_dictionary_destroy(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] destroy\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_dictionary_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_dictionary_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_dictionary_get_view(void)
{
    return g_app_dictionary;
}

static struct app_icon_t app_icon_res = {
#if CONFIG_LISAUI_FONT_LANGUAGE_ZH_CN_ENABLE
    .title = "词典",
#else
    .title = "Dictionary",
#endif

    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = &ui_img_icon_fav_png,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_dictionary = {
    .create = app_dictionary_create,
    .destroy = app_dictionary_destroy,
    .enter = app_dictionary_enter,
    .exit = app_dictionary_exit,

    .get_app_view = app_dictionary_get_view,
    .info =
        {
            .name = "Dictionary",
            .package_name = "com.listenai.lisaui.dictionary",
            .id = UI_APP_ID_DICTIONARY,
        },
    .icon = &app_icon_res,
};

lisaui_err_t app_dictionary_init(void)
{
    lisaui_app_register(&app_dictionary);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(dictionary, &app_dictionary, app_dictionary_init);