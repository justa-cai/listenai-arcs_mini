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
#include <stdio.h>

#define TAG "APP_PLAYER_CORE"

/**
 * @brief 检查并处理 preparing 状态
 * @param player 播放器实例
 * @return true 可以继续操作，false 已处理 preparing 状态需要 reset
 */
bool app_player_core_prepare_check(app_player_t *player)
{
    if (!player) {
        return false;
    }

    if (player->is_preparing) {
        // is_preparing判断完后打印后，可能引起时间片由player回调线程调度
        LISA_LOGD(TAG, "Wait %s prepare complete...", player->name);
        lisa_player_pre_close(player->hld);

        // 回调线程执行后，is_preparing会被置为false
        // 因此此处信号量可能会一直等待
        if (player->is_preparing) {
            player->wait_prepare_intercepted = true;
            lisa_semaphore_take(player->preparing_sem, LISA_OS_WAIT_FOREVER);
            player->wait_prepare_intercepted = false;
        }

        LISA_LOGD(TAG, "Pre %s prepare complete", player->name);
        lisa_player_stop_sync(player->hld);
        lisa_player_reset(player->hld);
        LISA_LOGD(TAG, "Pre %s status check end", player->name);
        return false;
    }

    return true;
}

/**
 * @brief 核心层：播放音频
 * @note 严格按照原 app_player_play_ex 的实现方式
 */
int app_player_core_play(app_player_t *player, const char *url, int throw_time)
{
    if (!player || !url) {
        return -1;
    }

    LISA_LOGD(TAG, "Core play: %s, url=%s, throw_time=%d", player->name, url, throw_time);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 检查并处理preparing状态
    if (app_player_core_prepare_check(player)) {
        // 如果返回true，需要reset
        lisa_player_reset(player->hld);
    }

    // 设置流式模式标志为false（URL播放模式）
    player->is_stream_mode = false;

    // 设置跳过开始指定时长内的低能量段
    if (throw_time > 0) {
        lisa_player_throw_low_energy(player->hld, throw_time);
    }

    // 设置preparing标志
    player->is_preparing = true;
    player->pause_preparing = false;

    // 设置URL并准备（seturl 会自动触发准备过程）
    PlayerErr ret = lisa_player_seturl(player->hld, url);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core play failed: seturl error %d", ret);
        player->is_preparing = false;
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：播放流式音频
 * @note 严格按照原 app_player_play_stream 的实现方式
 */
int app_player_core_play_stream(app_player_t *player, uint32_t sample_rate, uint8_t channels, uint8_t bits)
{
    if (!player) {
        return -1;
    }

    LISA_LOGD(TAG, "Core play stream: %s, rate=%u, ch=%u, bits=%u",
              player->name, sample_rate, channels, bits);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 检查并处理preparing状态
    if (app_player_core_prepare_check(player)) {
        // 如果返回true，需要reset
        lisa_player_reset(player->hld);
    }

    // 设置流式模式标志
    player->is_stream_mode = true;

    // 设置preparing标志
    player->is_preparing = true;
    player->pause_preparing = false;

    // 构造流式播放URL（PCM格式）
    char stream_url[128];
    snprintf(stream_url, sizeof(stream_url),
             "stream://type=pcm&rate=%u&channel=%u&bits=%u",
             sample_rate, channels, bits);

    // 设置URL并准备（seturl 会自动触发准备过程）
    PlayerErr ret = lisa_player_seturl(player->hld, stream_url);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core play stream failed: seturl error %d", ret);
        player->is_preparing = false;
        player->is_stream_mode = false;
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：暂停播放
 */
int app_player_core_pause(app_player_t *player)
{
    if (!player || player->is_stream_mode) {
        return -1;
    }

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 检查当前状态，如果已经是PAUSED，无需重复pause
    PlayerState state = lisa_player_get_state(player->hld);
    if (state == PLAYER_ST_PAUSED) {
        LISA_LOGD(TAG, "Core pause: %s already paused, skip", player->name);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return 0;
    }

    LISA_LOGD(TAG, "Core pause: %s", player->name);

    // 如果正在准备中，设置pause_preparing标志
    if (player->is_preparing) {
        player->pause_preparing = true;
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return 0;
    }

    // 调用底层暂停
    PlayerErr ret = lisa_player_pause(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core pause failed: lisa_player_pause error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：恢复播放
 */
int app_player_core_resume(app_player_t *player)
{
    if (!player || player->is_stream_mode) {
        return -1;
    }

    LISA_LOGD(TAG, "Core resume: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 如果是在准备阶段被暂停的，清除pause_preparing标志并开始播放
    if (player->pause_preparing) {
        player->pause_preparing = false;
        PlayerErr ret = lisa_player_play(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Core resume failed: lisa_player_play error %d", ret);
            PLAYER_MUTEX_UNLOCK(player->core_lock);
            return -1;
        }
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return 0;
    }

    // 调用底层的恢复接口
    PlayerErr ret = lisa_player_resume(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core resume failed: lisa_player_resume error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：停止播放
 */
int app_player_core_stop(app_player_t *player)
{
    if (!player) {
        return -1;
    }

    LISA_LOGD(TAG, "Core stop: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 检查并处理preparing状态
    if (app_player_core_prepare_check(player)) {
        // 不在preparing状态，调用同步stop
        int ret = lisa_player_stop_sync(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGW(TAG, "Core stop sync failed: %d, resetting player", ret);
            lisa_player_reset(player->hld);
            __enqueue_callback_event(player, APP_PLAYER_EVENT_STOPPED);
        }
    } else {
        // 在preparing状态，__player_prepare_check已经处理完毕
        // 手动发送STOPPED事件（因为已经reset，不会有回调）
        __enqueue_callback_event(player, APP_PLAYER_EVENT_STOPPED);
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：同步停止播放
 */
int app_player_core_stop_sync(app_player_t *player)
{
    if (!player) {
        return -1;
    }

    LISA_LOGD(TAG, "Core stop sync: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 检查并处理preparing状态
    if (app_player_core_prepare_check(player)) {
        // 不在preparing状态，调用同步stop
        int ret = lisa_player_stop_sync(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGW(TAG, "Core stop sync failed: %d, resetting player", ret);
            lisa_player_reset(player->hld);
            __enqueue_callback_event(player, APP_PLAYER_EVENT_STOPPED);
        }
    } else {
        // 在preparing状态，__player_prepare_check已经处理完毕
        // 手动发送STOPPED事件（因为已经reset，不会有回调）
        __enqueue_callback_event(player, APP_PLAYER_EVENT_STOPPED);
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：写入流式数据
 * @note 严格按照原 app_player_write_stream 的实现方式（使用timeout_ms参数）
 */
int app_player_core_stream_write(app_player_t *player, const uint8_t *data, size_t size, uint32_t timeout_ms)
{
    if (!player) {
        return -1;
    }

    // data 可以为 NULL（用于结束流）
    if (!data && size > 0) {
        LISA_LOGE(TAG, "Core stream write failed: data is NULL but size > 0");
        return -1;
    }

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    if (!player->is_stream_mode) {
        LISA_LOGE(TAG, "Core stream write failed: not in stream mode");
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    // 调用底层写入接口
    int ret = lisa_player_put_stream_data(player->hld, (uint8_t *)data, size, timeout_ms);
    if (ret < 0) {
        LISA_LOGE(TAG, "Core stream write failed: %d", ret);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return ret;
}
