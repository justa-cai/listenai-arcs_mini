/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_PLAYER_CORE_H
#define APP_PLAYER_CORE_H

#include "app_player.h"
#include "lisa_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief lisa_player 封装层 - 核心接口
 *
 * 本模块专门处理 lisa_player 的各种限制和状态管理：
 * - is_preparing 状态处理
 * - pause_preparing 标志管理
 * - prepare 中断机制
 * - 纯粹的播放器控制，不涉及焦点管理和 PA 控制
 * - 所有接口均已实现线程安全保护（使用 core_lock）
 */

/**
 * @brief 核心层：播放音频
 * @param player 播放器实例
 * @param url 音频 URL
 * @param throw_time 跳转时间（ms）
 * @return 0 成功，负数失败
 * @note 不操作焦点，不操作 PA，线程安全
 */
int app_player_core_play(app_player_t *player, const char *url, int throw_time);

/**
 * @brief 核心层：播放流式音频
 * @param player 播放器实例
 * @param sample_rate 采样率
 * @param channels 声道数
 * @param bits 位宽
 * @return 0 成功，负数失败
 * @note 不操作焦点，不操作 PA，线程安全
 */
int app_player_core_play_stream(app_player_t *player, uint32_t sample_rate, uint8_t channels, uint8_t bits);

/**
 * @brief 核心层：暂停播放
 * @param player 播放器实例
 * @return 0 成功，负数失败
 * @note 不操作焦点，不操作 PA，线程安全
 */
int app_player_core_pause(app_player_t *player);

/**
 * @brief 核心层：恢复播放
 * @param player 播放器实例
 * @return 0 成功，负数失败
 * @note 不操作焦点，不操作 PA，线程安全
 */
int app_player_core_resume(app_player_t *player);

/**
 * @brief 核心层：停止播放
 * @param player 播放器实例
 * @return 0 成功，负数失败
 * @note 不操作焦点，不操作 PA，线程安全
 */
int app_player_core_stop(app_player_t *player);

/**
 * @brief 核心层：同步停止播放
 * @param player 播放器实例
 * @return 0 成功，负数失败
 * @note 不操作焦点，不操作 PA，线程安全
 */
int app_player_core_stop_sync(app_player_t *player);

/**
 * @brief 核心层：写入流式数据
 * @param player 播放器实例
 * @param data 数据缓冲区
 * @param size 数据大小
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际写入的字节数，负数表示错误
 */
int app_player_core_stream_write(app_player_t *player, const uint8_t *data, size_t size, uint32_t timeout_ms);

/**
 * @brief 检查并处理 preparing 状态
 * @param player 播放器实例
 * @return true 可以继续操作，false 已处理 preparing 状态需要 reset
 * @note 内部函数，由 core 层调用
 */
bool app_player_core_prepare_check(app_player_t *player);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLAYER_CORE_H */
