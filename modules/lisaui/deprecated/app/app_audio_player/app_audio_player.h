/**
 * @file app_audio_player.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief 
 * @version 0.1
 * @date 2025-03-03
 * 
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_AUDIO_PLAYER_H__
#define __APP_AUDIO_PLAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../app_common.h"
#include "lisaui_app_common.h"

typedef struct _metadata_music_song_t {
    char *title;        // 歌曲标题
    char *artist;       // 歌手
    // char *album;        // 专辑
    // char *duration;     // 时长
    // char *cover_path;   // 封面路径
    // char *file_path;    // 文件路径
} metadata_music_song_t;


typedef enum {
    LISAUI_APP_AUDIO_PLAYER_STATE_STOP,
    LISAUI_APP_AUDIO_PLAYER_STATE_PLAY,
    LISAUI_APP_AUDIO_PLAYER_STATE_PAUSE,
    LISAUI_APP_AUDIO_PLAYER_STATE_RESUME,

    LISAUI_APP_AUDIO_PLAYER_STATE_NEXT,
    LISAUI_APP_AUDIO_PLAYER_STATE_PREV,
} lisaui_app_audio_player_state_t;

#if CONFIG_LVGL_ENV_SIMULATOR
#define APP_AUDIO_PLAYER_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_audio_player/" path
#else
#define APP_AUDIO_PLAYER_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_audio_player/" path
// #define APP_AUDIO_PLAYER_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_audio_player/" path
#endif

lisaui_err_t lisaui_app_audio_player_set_song_view(metadata_music_song_t *song, lisaui_app_audio_player_state_t state);
lisaui_err_t lisaui_app_audio_player_del_song_view(void);
lisaui_err_t lisaui_app_audio_player_set_play_state(lisaui_app_audio_player_state_t state);
lisaui_err_t lisaui_app_audio_player_set_play_progress(int progress);

lisaui_err_t app_audio_player_init(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_AUDIO_PLAYER_H__
