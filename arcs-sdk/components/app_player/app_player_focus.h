/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_PLAYER_FOCUS_H
#define APP_PLAYER_FOCUS_H

#include "app_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 焦点管理层接口
 *
 * 本模块负责处理音频焦点相关的所有逻辑：
 * - 焦点申请和释放
 * - 焦点策略执行（暂停、停止、忽略等）
 * - 焦点变化回调处理
 * - 焦点状态管理
 */

/**
 * @brief 初始化焦点管理器
 * @param focus_configs 焦点配置数组
 * @param focus_config_count 焦点配置数量
 * @return 0 成功，负数失败
 */
int app_player_focus_init(const app_player_focus_channel_config_t *focus_configs, int focus_config_count);

/**
 * @brief 反初始化焦点管理器
 */
void app_player_focus_deinit(void);

/**
 * @brief 为播放器注册焦点通道
 * @param player 播放器实例
 * @param name 播放器名称（用于匹配焦点配置）
 * @return 0 成功，负数失败
 */
int app_player_focus_register(app_player_t *player, const char *name);

/**
 * @brief 注销播放器的焦点通道
 * @param player 播放器实例
 * @return 0 成功，负数失败
 */
int app_player_focus_unregister(app_player_t *player);

/**
 * @brief 申请音频焦点
 * @param player 播放器实例
 * @return 0 成功，负数失败
 */
int app_player_focus_acquire(app_player_t *player);

/**
 * @brief 释放音频焦点
 * @param player 播放器实例
 * @param is_user_initiated 是否是用户主动操作（影响是否清除 user_initiated_stop 标志）
 * @return 0 成功，负数失败
 */
int app_player_focus_release(app_player_t *player, bool is_user_initiated);

/**
 * @brief 注册焦点变化回调
 * @param player 播放器实例
 * @param callback 焦点变化回调函数
 * @param user_data 用户数据
 * @return 0 成功，负数失败
 */
int app_player_focus_register_callback(app_player_t *player,
                                       app_player_focus_change_cb_t callback,
                                       void *user_data);

/**
 * @brief 设置用户主动操作标志
 * @param player 播放器实例
 * @param initiated 是否是用户主动操作
 */
void app_player_focus_set_user_initiated(app_player_t *player, bool initiated);

/**
 * @brief 获取用户主动操作标志
 * @param player 播放器实例
 * @return true 用户主动操作，false 否
 */
bool app_player_focus_is_user_initiated(const app_player_t *player);

/**
 * @brief 设置焦点暂停标志
 * @param player 播放器实例
 * @param paused 是否因焦点而暂停
 */
void app_player_focus_set_paused_by_focus(app_player_t *player, bool paused);

/**
 * @brief 获取焦点暂停标志
 * @param player 播放器实例
 * @return true 因焦点而暂停，false 否
 */
bool app_player_focus_is_paused_by_focus(app_player_t *player);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLAYER_FOCUS_H */
