/**
 * @brief   播放器管理
 * @version 0.1
 * @date    2022-08-24
 * @author  mokee
 * 
 * Copyright (C) 2022 ANHUI LISTENAI Co., LTD All Rights Reserved
 */

#ifndef __LISTENAI_APP_PLAYER_H__
#define __LISTENAI_APP_PLAYER_H__

#include <stdint.h>
#include "lisa_player.h"

typedef uint8_t player_t;

#define PLAYER_T_CLOUD  (1)
#define PLAYER_T_TONE   (2)

#define APP_PLAYER_PREPARING (0xEF)

typedef int (*player_status_cb)(uint16_t st);

typedef void (*app_player_data_hook)(const char *const data, uint32_t size);

/**
 * @brief   应用层播放器初始化
 */
void app_player_init();

void app_player_play(player_t type, char *url, player_status_cb cb);

void app_player_play_by_throw(player_t type, char *url, int throw_time_ms, player_status_cb cb);

void app_player_pause(player_t type);

void app_player_resume(player_t type);

void app_player_resume_sync(player_t type);

void app_player_stop(player_t type);

void app_player_stop_sync(player_t type);

void app_player_seek(player_t type, uint32_t seek_ms);

uint32_t app_player_position(player_t type);

uint32_t app_player_duration(player_t type);

void app_player_volume(player_t type, uint8_t volume);

void app_player_reset(player_t type);

void app_player_close(player_t type);

void app_player_set_data_hook(player_t type, app_player_data_hook hook, int flag);

#endif