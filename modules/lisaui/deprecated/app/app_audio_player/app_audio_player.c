/**
 * @file app_audio_player.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_audio_player.h"
#include "app_common.h"
#include "lisaui_app_common.h"
#include "assets/assets_res.h"

#include "lv_img_utils.h"

static const char *TAG = "app_audio_player";
static lv_obj_t *g_app_audio_player = NULL;
static lv_obj_t *g_app_panel = NULL;
static lv_obj_t *temp_music_song_view = NULL;

UI_RES_IMG_NAME(music_icon, APP_AUDIO_PLAYER_UI_RES_PERFIX_PATH("assets/png/ic_music.png"))
UI_RES_IMG_NAME(music_care_view_bg, APP_AUDIO_PLAYER_UI_RES_PERFIX_PATH("assets/png/music_care_view_bg.png"))

lv_obj_t *lisaui_app_audio_player_create_song_view(lv_obj_t *parent, metadata_music_song_t *song);

// 声明[模板]应用需要实现的接口
LISAUI_DECLARE_APP_FUNC(audio_player, create, destroy, enter, exit, get_view);
// 定义[模板]应用
LISAUI_DEFINE_APP(audio_player, UI_APP_ID_AUDIO_PLAYER, "播放器", "Audio_player", &ui_img_icon_composition_png, {});

// App 模板宏原型在 common/lisaui_app_manager.h 中定义
// 定义[模板]应用功能实现, 用于创建、销毁、进入、退出、获取页面
// #define LISAUI_APP_ENTITY_AUDIO_PLAYER \
//     { \
//         if (g_app_audio_player != NULL) { \
//             return LISAUI_ERR_OK; \
//         } \
//         g_app_audio_player = lv_obj_create(parent); \
//         _lisaui_set_style_container(g_app_audio_player, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0); \
//                                                                                                                        \
//         lv_obj_t *label = lv_label_create(g_app_audio_player); \
//         lv_label_set_text(label, "Hello LisaUI!"); \
//         lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT); \
//         lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT); \
//         lv_obj_align(label, LV_ALIGN_CENTER, 0, 0); \
//                                                                                                                        \
//         return LISAUI_ERR_OK; \
//     }
// lisaui_err_t LISAUI_DEFINE_APP_FUNC(audio_player, create, void *parent, {
// });
lisaui_err_t app_audio_player_create(void *parent)
{
    // LISAUI_APP_ENTITY_AUDIO_PLAYER
    if (g_app_audio_player != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_audio_player = lv_obj_create(parent);
    _lisaui_set_style_container(g_app_audio_player, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    g_app_panel = lv_obj_create(g_app_audio_player);
    _lisaui_set_style_container(g_app_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    // lv_obj_set_size(g_app_panel, LV_PCT(100), LV_PCT(80));
    // lv_obj_set_y(g_app_panel, LV_DPX(LISAUI_STATUS_BAR_HEIGHT));

    lv_obj_set_size(g_app_panel, LV_PCT(100), LV_PCT(100));

    LISAUI_LOGI(TAG, "[%d:%s] create\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t LISAUI_DEFINE_APP_FUNC(audio_player, destroy, void, {
    LVGL_OBJ_SAFE_DEL(g_app_audio_player);
    return LISAUI_ERR_OK;
});
lisaui_err_t LISAUI_DEFINE_APP_FUNC(audio_player, enter, void, { return LISAUI_ERR_OK; });
lisaui_err_t LISAUI_DEFINE_APP_FUNC(audio_player, exit, void, { return LISAUI_ERR_OK; });
void *LISAUI_DEFINE_APP_FUNC(audio_player, get_view, void, { return g_app_audio_player; });

/**********************************************************************************************************************/

static lv_obj_t *line_setting_floor;
static lv_point_t lv_setting_line_floor[2];

lv_obj_t *lisaui_app_audio_player_create_song_view(lv_obj_t *parent, metadata_music_song_t *song)
{
    // lv_obj_t *song_view = lv_obj_create(parent);
    // _lisaui_set_style_container(song_view, lv_color_hex(0x000000), 0, lv_color_hex(0x000000), 0, 0);
    lv_obj_t *song_view = lv_img_create(parent);
    _lisaui_set_style_container(song_view, lv_color_hex(0x000000), 0, lv_color_hex(0x000000), 0, 0);
    lv_img_png_src_init(UI_RES_IMG_PNG(music_care_view_bg));
    lv_img_set_src(song_view, &LV_IMG_DSC(music_care_view_bg));
    lv_obj_set_size(song_view, LV_PCT(100), LV_PCT(100));

    LV_OBJ_ICON(music_icon) = lv_img_create(song_view);
    lv_img_png_src_init(UI_RES_IMG_PNG(music_icon));
    lv_img_set_src(LV_OBJ_ICON(music_icon), &LV_IMG_DSC(music_icon));
    lv_obj_align(LV_OBJ_ICON(music_icon), LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_flag(LV_OBJ_ICON(music_icon), LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(LV_OBJ_ICON(music_icon), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(LV_OBJ_ICON(music_icon), LV_ALIGN_TOP_MID, -LV_DPX(80), LV_DPX(LISAUI_STATUS_BAR_HEIGHT));

    lv_obj_t *tips_label = lv_label_create(song_view);
    lv_label_set_text(tips_label, "正在为你播放");
    lv_obj_set_style_text_color(tips_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(tips_label, &lv_font_chinese_18, LV_PART_MAIN);
    // lv_obj_align(tips_label, LV_ALIGN_TOP_MID, 0, LV_DPX(4));
    lv_obj_align_to(tips_label, LV_OBJ_ICON(music_icon), LV_ALIGN_OUT_RIGHT_MID, LV_DPX(4), LV_DPX(0));

    lv_obj_t *title_label = lv_label_create(song_view);
    lv_label_set_text(title_label, song->title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -LV_DPX(30));

    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_rounded(&style_line, true);

    line_setting_floor = lv_line_create(song_view);
    lv_obj_add_style(line_setting_floor, &style_line, 0);
    lv_obj_set_style_line_color(line_setting_floor, lv_color_hex(0xaaaaaa), 0);

    lv_setting_line_floor[0].x = LV_DPX(0);
    lv_setting_line_floor[1].x = LV_DPX(LV_HOR_RES - 40);

    lv_setting_line_floor[0].y = 0;
    lv_setting_line_floor[1].y = 0;
    // lv_setting_line_floor[0].y = LV_DPX(LV_VER_RES/2);
    // lv_setting_line_floor[1].y = LV_DPX(LV_VER_RES/2);

    lv_line_set_points(line_setting_floor, lv_setting_line_floor, 2);
    lv_obj_align_to(line_setting_floor, title_label, LV_ALIGN_OUT_BOTTOM_MID, 0, LV_DPX(30));

    lv_obj_t *artist_label = lv_label_create(song_view);
    lv_label_set_text(artist_label, song->artist);
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(artist_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align_to(artist_label, title_label, LV_ALIGN_OUT_BOTTOM_MID, 0, LV_DPX(60));

    return song_view;
}

lisaui_err_t lisaui_app_audio_player_set_song_view(metadata_music_song_t *song, lisaui_app_audio_player_state_t state)
{
    LVGL_UI_LOCK();
    if (g_app_audio_player == NULL) {
        LISAUI_LOGE(TAG, "g_app_weather is NULL");
        LVGL_UI_UNLOCK();
        return LISAUI_ERR_FAIL;
    }

    LVGL_OBJ_SAFE_DEL(temp_music_song_view);
    lisaui_app_enter(UI_APP_ID_AUDIO_PLAYER);
    temp_music_song_view = lisaui_app_audio_player_create_song_view(g_app_panel, song);

    LVGL_UI_UNLOCK();
    return LISAUI_ERR_OK;
}
