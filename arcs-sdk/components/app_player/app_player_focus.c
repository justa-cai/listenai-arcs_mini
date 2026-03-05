/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "lisa_log.h"
#include "lisa_mem.h"
#include "app_player_internal.h"
#include "app_player_core.h"
#include "app_player_focus.h"
#include "listen_audiomgr.h"
#include "pa_manager.h"

#define TAG "APP_PLAYER_FOCUS"

/**
 * @brief 全局焦点管理器实例
 */
static listen_audiomgr_t *s_audio_focus_mgr = NULL;

/**
 * @brief 焦点配置缓存
 */
static const app_player_focus_channel_config_t *s_focus_configs = NULL;
static int s_focus_config_count = 0;

// 前向声明
static void __audio_focus_change_bridge(focus_state_e state, int by_which_id, void *user_data);
extern app_player_t *__find_player_by_focus_channel_id(int focus_channel_id);

static int __find_focus_config_index_by_name(const char *name)
{
    if (!name || !s_focus_configs || s_focus_config_count <= 0) {
        return -1;
    }

    for (int i = 0; i < s_focus_config_count; i++) {
        if (s_focus_configs[i].name && strcmp(s_focus_configs[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/**
 * @brief 执行焦点丢失策略（内部函数）
 * @param player 播放器实例
 * @param policy 焦点丢失策略
 * @return 0成功，负数失败
 */
static int __execute_focus_loss_policy(app_player_t *player, app_player_focus_loss_policy_t policy)
{
    if (!player) {
        return -1;
    }

    switch (policy) {
        case APP_PLAYER_FOCUS_LOSS_PAUSE:
            LISA_LOGI(TAG, "Player %s: executing PAUSE policy", player->name);
            player->paused_by_focus = true;

            // 使用核心层暂停，不操作焦点
            pa_manager_control(0, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);
            app_player_core_pause(player);
            break;

        case APP_PLAYER_FOCUS_LOSS_STOP:
            LISA_LOGI(TAG, "Player %s: executing STOP policy", player->name);
            player->paused_by_focus = false;  // STOP后不可恢复

            // 使用核心层停止，不操作焦点
            pa_manager_control(0, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);
            app_player_core_stop(player);
            break;

        case APP_PLAYER_FOCUS_LOSS_DUCK:
            LISA_LOGW(TAG, "Player %s: DUCK policy not supported yet", player->name);
            // TODO: 实现音量降低逻辑
            break;

        case APP_PLAYER_FOCUS_LOSS_IGNORE:
        default:
            LISA_LOGD(TAG, "Player %s: ignoring focus loss", player->name);
            break;
    }

    return 0;
}

/**
 * @brief 焦点变化回调桥接函数
 */
static void __audio_focus_change_bridge(focus_state_e state, int by_which_id, void *user_data)
{
    app_player_t *player = (app_player_t *)user_data;
    if (!player) {
        LISA_LOGE(TAG, "Focus callback: player is NULL");
        return;
    }

    // 转换焦点状态
    app_player_focus_state_t app_state;
    switch (state) {
        case FOREGROUND:
            app_state = APP_PLAYER_FOCUS_FOREGROUND;
            break;
        case BACKGROUND:
            app_state = APP_PLAYER_FOCUS_BACKGROUND;
            break;
        case FOCUS_NONE:
        default:
            app_state = APP_PLAYER_FOCUS_NONE;
            break;
    }

    // 通过通道ID查找触发焦点变化的播放器
    app_player_t *by_which_player = __find_player_by_focus_channel_id(by_which_id);
    const char *by_which_name = by_which_player ? by_which_player->name :
                                 listen_audiomgr_get_channel_name(s_audio_focus_mgr, by_which_id);

    LISA_LOGI(TAG, "Player[%d] %s focus change: %s -> %s (by %s)",
              player->id, player->name,
              listen_audiomgr_get_state_name(player->last_focus_state),
              listen_audiomgr_get_state_name(state),
              by_which_name);

    // 读取用户注册的焦点回调
    PLAYER_MUTEX_LOCK(player->focus_cb_lock, LISA_OS_WAIT_FOREVER);
    app_player_focus_change_cb_t user_cb = player->focus_cb;
    void *focus_user_data = player->focus_user_data;
    PLAYER_MUTEX_UNLOCK(player->focus_cb_lock);

    // 调用用户回调
    bool handled = false;
    if (user_cb) {
        handled = user_cb(player, app_state, by_which_player, focus_user_data);
    }

    // 如果用户回调返回 false，执行默认策略
    if (!handled) {
        // 检查是否是用户主动调用stop/pause导致的焦点释放
        if (player->user_initiated_stop &&
            (app_state == APP_PLAYER_FOCUS_BACKGROUND || app_state == APP_PLAYER_FOCUS_NONE)) {
            LISA_LOGI(TAG, "Player %s: skip focus policy (user-initiated stop/pause)", player->name);
            // 用户主动操作，不执行焦点策略
        } else if ((app_state == APP_PLAYER_FOCUS_BACKGROUND || app_state == APP_PLAYER_FOCUS_NONE) &&
                   by_which_player == player) {
            LISA_LOGI(TAG, "Player %s: skip focus policy (self-triggered focus change)", player->name);
            // 播放器自身触发的焦点变化，说明已完成相应控制
        } else if (app_state == APP_PLAYER_FOCUS_FOREGROUND) {
            // 获得焦点：检查是否需要自动恢复
            if (player->paused_by_focus) {
                LISA_LOGI(TAG, "Player %s: auto-resuming from focus pause", player->name);
                player->paused_by_focus = false;

                // 使用核心层恢复，不操作焦点
                pa_manager_control(1, 0);
                app_player_core_resume(player);
            }
        } else if (app_state == APP_PLAYER_FOCUS_BACKGROUND) {
            // 变为后景：执行 on_background 策略
            __execute_focus_loss_policy(player, player->behavior.on_background);
        } else if (app_state == APP_PLAYER_FOCUS_NONE) {
            // 失去焦点：执行 on_focus_lost 策略
            __execute_focus_loss_policy(player, player->behavior.on_focus_lost);
        }
    } else {
        LISA_LOGD(TAG, "Player %s: focus change handled by user callback", player->name);
    }

    // 更新上一次焦点状态
    player->last_focus_state = app_state;
}

/**
 * @brief 初始化焦点管理器
 */
int app_player_focus_init(const app_player_focus_channel_config_t *focus_configs, int focus_config_count)
{
    if (s_audio_focus_mgr) {
        LISA_LOGW(TAG, "Focus manager already initialized");
        return 0;
    }

    if (!focus_configs || focus_config_count <= 0) {
        LISA_LOGI(TAG, "No focus config provided, skip focus manager init");
        return 0;
    }

    LISA_LOGI(TAG, "Initializing audio focus manager with %d channels", focus_config_count);

    s_audio_focus_mgr = listen_audiomgr_create();
    if (!s_audio_focus_mgr) {
        LISA_LOGE(TAG, "Focus init failed: audio focus manager create failed");
        return -1;
    }

    // 保存焦点配置
    s_focus_configs = focus_configs;
    s_focus_config_count = focus_config_count;

    LISA_LOGI(TAG, "Focus manager initialized successfully");
    return 0;
}

/**
 * @brief 反初始化焦点管理器
 */
void app_player_focus_deinit(void)
{
    if (s_audio_focus_mgr) {
        listen_audiomgr_destroy(s_audio_focus_mgr);
        s_audio_focus_mgr = NULL;
    }
    s_focus_configs = NULL;
    s_focus_config_count = 0;
    LISA_LOGI(TAG, "Focus manager deinitialized");
}

/**
 * @brief 为播放器注册焦点通道
 */
int app_player_focus_register(app_player_t *player, const char *name)
{
    if (!player || !name) {
        return -1;
    }

    if (!s_audio_focus_mgr || !s_focus_configs || s_focus_config_count <= 0) {
        LISA_LOGD(TAG, "Focus manager not initialized, skip registration for %s", name);
        return 0;
    }

    // 查找与播放器名称匹配的焦点配置
    int focus_index = __find_focus_config_index_by_name(name);
    if (focus_index < 0) {
        LISA_LOGD(TAG, "No focus config found for player %s", name);
        return 0;
    }

    const app_player_focus_channel_config_t *focus_config = &s_focus_configs[focus_index];
    int channel_id = focus_index;

    // 转换 capture_names 为 capture_ids
    int *capture_ids = NULL;
    int capture_count = 0;
    if (focus_config->capture_names && focus_config->capture_count > 0) {
        capture_ids = (int *)lisa_mem_alloc(sizeof(int) * focus_config->capture_count);
        if (!capture_ids) {
            return -1;
        }

        for (int i = 0; i < focus_config->capture_count; i++) {
            // 查找 capture_name 对应的通道ID
            int target_id = __find_focus_config_index_by_name(focus_config->capture_names[i]);
            if (target_id >= 0) {
                capture_ids[capture_count++] = target_id;
            }
        }
    }

    // 注册通道
    int ret = listen_audiomgr_register_channel(
        s_audio_focus_mgr,
        channel_id,
        name,
        focus_config->priority,
        capture_ids,
        capture_count,
        __audio_focus_change_bridge,
        player  // 传递 player 指针作为 user_data
    );

    if (capture_ids) {
        lisa_mem_free(capture_ids);
    }

    if (ret == 0) {
        player->focus_channel_id = channel_id;
        // 复制焦点行为配置到播放器实例
        player->behavior = focus_config->behavior;
        LISA_LOGI(TAG, "Registered focus channel for player %s (id=%d, priority=%d)",
                  name, channel_id, focus_config->priority);
    } else {
        LISA_LOGE(TAG, "Register focus channel failed for %s", name);
        return -1;
    }

    return 0;
}

/**
 * @brief 注销播放器的焦点通道
 */
int app_player_focus_unregister(app_player_t *player)
{
    if (!player) {
        return -1;
    }

    if (s_audio_focus_mgr && player->focus_channel_id >= 0) {
        LISA_LOGD(TAG, "Releasing focus for player %s before destroy", player->name);
        listen_audiomgr_release_channel(s_audio_focus_mgr, player->focus_channel_id);
        player->focus_channel_id = -1;
    }

    return 0;
}

/**
 * @brief 申请音频焦点
 */
int app_player_focus_acquire(app_player_t *player)
{
    if (!player) {
        return -1;
    }

    if (s_audio_focus_mgr && player->focus_channel_id >= 0) {
        LISA_LOGD(TAG, "Acquiring audio focus for player %s", player->name);
        listen_audiomgr_acquire_channel(s_audio_focus_mgr, player->focus_channel_id);
        return 0;
    }

    return -1;
}

/**
 * @brief 释放音频焦点
 */
int app_player_focus_release(app_player_t *player, bool is_user_initiated)
{
    if (!player) {
        return -1;
    }

    if (s_audio_focus_mgr && player->focus_channel_id >= 0) {
        LISA_LOGD(TAG, "Releasing audio focus for player %s (user_initiated=%d)",
                 player->name, is_user_initiated);
        listen_audiomgr_release_channel(s_audio_focus_mgr, player->focus_channel_id);

        // 如果是用户主动操作，清除标志
        if (is_user_initiated) {
            player->user_initiated_stop = false;
        }

        return 0;
    }

    return -1;
}

/**
 * @brief 注册焦点变化回调
 */
int app_player_focus_register_callback(app_player_t *player,
                                       app_player_focus_change_cb_t callback,
                                       void *user_data)
{
    if (!player) {
        return -1;
    }

    if (player->focus_channel_id < 0) {
        LISA_LOGW(TAG, "Register focus callback failed: player %s not registered to focus manager",
                  player->name);
        return -1;
    }

    PLAYER_MUTEX_LOCK(player->focus_cb_lock, LISA_OS_WAIT_FOREVER);
    player->focus_cb = callback;
    player->focus_user_data = user_data;
    PLAYER_MUTEX_UNLOCK(player->focus_cb_lock);

    LISA_LOGI(TAG, "Registered focus callback for player %s", player->name);
    return 0;
}

/**
 * @brief 设置用户主动操作标志
 */
void app_player_focus_set_user_initiated(app_player_t *player, bool initiated)
{
    if (player) {
        player->user_initiated_stop = initiated;
    }
}

bool app_player_focus_is_user_initiated(const app_player_t *player)
{
    if (!player) {
        return false;
    }

    return player->user_initiated_stop;
}

/**
 * @brief 设置焦点暂停标志
 */
void app_player_focus_set_paused_by_focus(app_player_t *player, bool paused)
{
    if (player) {
        player->paused_by_focus = paused;
    }
}

/**
 * @brief 获取焦点暂停标志
 */
bool app_player_focus_is_paused_by_focus(app_player_t *player)
{
    return player ? player->paused_by_focus : false;
}

/**
 * @brief 设置播放器的焦点行为策略（运行时动态修改）
 */
int app_player_set_focus_behavior(app_player_t *player,
                                   const app_player_focus_behavior_t *behavior)
{
    if (!player || !behavior) {
        LISA_LOGE(TAG, "Set focus behavior failed: invalid parameter");
        return -1;
    }

    if (player->focus_channel_id < 0) {
        LISA_LOGW(TAG, "Player %s has no focus channel registered", player->name);
        return -1;
    }

    // 更新行为配置（使用锁保护）
    PLAYER_MUTEX_LOCK(player->focus_cb_lock, LISA_OS_WAIT_FOREVER);
    player->behavior = *behavior;
    PLAYER_MUTEX_UNLOCK(player->focus_cb_lock);

    LISA_LOGI(TAG, "Updated focus behavior for player %s: on_background=%d, on_focus_lost=%d",
              player->name, behavior->on_background, behavior->on_focus_lost);

    return 0;
}

/**
 * @brief 获取播放器当前的焦点行为策略
 */
int app_player_get_focus_behavior(app_player_t *player,
                                   app_player_focus_behavior_t *behavior)
{
    if (!player || !behavior) {
        LISA_LOGE(TAG, "Get focus behavior failed: invalid parameter");
        return -1;
    }

    if (player->focus_channel_id < 0) {
        LISA_LOGW(TAG, "Player %s has no focus channel registered", player->name);
        return -1;
    }

    // 读取行为配置（使用锁保护）
    PLAYER_MUTEX_LOCK(player->focus_cb_lock, LISA_OS_WAIT_FOREVER);
    *behavior = player->behavior;
    PLAYER_MUTEX_UNLOCK(player->focus_cb_lock);

    return 0;
}
