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

/*
 * audio0 的整机共享播放契约。
 *
 * audio0 是单实例, 且底层在 RUNNING 状态**拒绝重配采样率**。语音侧
 * (唤醒引擎的占位播放流 / 提示音 / TTS) 与游戏 (NES 直接写 audio0) 都要用它,
 * 只要双方采样率不一致, 就会出现"一方的 PCM 灌进另一方的流" -> 变调/失真,
 * 或一方停流后另一方写不进去 -> 播放线程卡死。
 *
 * 因此约定: 全机只有这一套播放参数, 谁先来谁配置, 后来者直接写。
 * 采样率取 16kHz —— 语音链路 (预编译 lisa_player track 内置 16k) 本来就是 16k,
 * 由游戏侧降采样到 16k 的代价最小。
 *
 * DMA 粒度必须与之匹配: arcs_audio_play_write() 会为每个不足一个 DMA buffer 的
 * 写入补静音, 所以生产者每次写入的样本数应当是 APP_AUDIO0_DMA_SAMPLES 的整数倍,
 * 否则自己给自己掺静音 (听感上就是断续)。
 */
#define APP_AUDIO0_SAMPLE_RATE   16000
#define APP_AUDIO0_DMA_SAMPLES   256U
#define APP_AUDIO0_BUF_COUNT     12

/**
 * @brief 设置 PCM 输出回调
 * @param cb 回调函数，NULL 则使用默认 lisa_audio 输出
 */
void app_player_adapter_set_pcm_output(app_player_pcm_output_cb_t cb);

/**
 * @brief 幂等确保 audio0 处于共享播放契约 (16kHz/mono/16bit/12×256) 的运行态
 *
 * - 已在运行: 直接返回 0 (不重配, 不改采样率, 不影响正在播放的任何一方);
 * - 空闲: 按契约配置并启动;
 * - 其它状态 (暂停/错误): 返回 -1, 调用方应稍后重试。
 *
 * 供所有往 audio0 写 PCM 的模块调用 (app_player 输出适配、唤醒引擎、NES 端口)。
 *
 * @return 0 表示"现在可以往 audio0 写"
 */
int app_audio0_ensure_play(void);

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
