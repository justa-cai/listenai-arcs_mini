/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LISA_PLAYER_ADAPTER_H__
#define __LISA_PLAYER_ADAPTER_H__

#include "app_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置 PCM 输出回调
 * @param cb 回调函数，NULL 则使用默认 lisa_audio 输出
 */
void app_player_adapter_set_pcm_output(app_player_pcm_output_cb_t cb);

/**
 * @brief 发送 PCM 数据到音频输出
 * @param data PCM 数据指针
 * @param size 数据大小（字节）
 */
void audio_play_send_pcm(char *data, int size);

#ifdef __cplusplus
}
#endif

#endif /* __LISA_PLAYER_ADAPTER_H__ */
