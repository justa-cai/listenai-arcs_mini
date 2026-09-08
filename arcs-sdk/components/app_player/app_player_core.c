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

/* 信号量超时时间（毫秒），默认等待 5s */
#define SEMAPHORE_WAIT_TIMEOUT_MS CONFIG_APP_PLAYER_SEMAPHORE_WAIT_TIMEOUT_MS

/**
 * @brief lisa_player 回调处理函数（Core 层）
 * @param evt lisa_player 事件
 * @param arg1 参数1s
 * @param arg2 参数2
 * @param id 播放器ID
 * @return 0成功，负数失败
 *
 * @note Core 层职责：
 * 1. 处理 preparing 状态管理
 * 2. PREPARED 事件的自动播放逻辑
 * 3. pause_preparing 标志处理
 * 4. ERROR 事件的自动 reset
 * 5. 更新 last_evt 状态
 * 6. 调用上层回调通知 PA、焦点等处理
 */
static int __lisa_player_core_callback_handler(PlayerEvt evt, int arg1, int arg2, int id)
{
    // 通过ID查找实例
    app_player_t *player = __find_player_by_id(id);
    if (!player) {
        LISA_LOGE(TAG, "Core callback: player not found for id=%d", id);
        return -1;
    }

    LISA_LOGI(TAG, "Core callback: Player[%d] %s evt=%d", id, player->name, evt);

    // 标志：是否忽略自动播放
    bool ignore_play = false;
    // 标志：是否需要通知用户
    bool should_notify_user = true;

    // === Core 层职责 1: 清除 preparing 标志 ===
    player->is_preparing = false;

    // === Core 层职责 3: 特殊事件处理（纯播放器逻辑）===
    switch (evt) {
        case PLAYER_EVT_PREPARED: {
            PLAYER_MUTEX_LOCK(player->cancel_lock, LISA_OS_WAIT_FOREVER);
            if (player->stop_preparing_requested ||
                __app_player_play_cancelled_locked(player)) {
                LISA_LOGI(TAG, "Core: %s prepared ignored due to stop request", player->name);
                player->prepare_error = true;
                /* 不在这里调 lisa_player_reset：实测无论 pre_close 与否，从 PREPARED
                 * 状态调 reset 都不会真正把状态机带回 IDLE，后续 play_ex 还是看到
                 * state=PREPARED，core_play 再 reset 会卡死。
                 *
                 * 残留的 PREPARED 状态留给下一次 app_player_play_ex 走 play+stop_sync
                 * 排空（PREPARED → PLAYING → STOPPED → reset 成功）。期间用
                 * drain_in_progress 标志抑制上层回调，避免 PA 抖动与虚假 TTS 事件。 */
                lisa_semaphore_give(player->preparing_sem);
                PLAYER_MUTEX_UNLOCK(player->cancel_lock);
                break;
            }
            /* Enable the PA before the decoder can submit its first PCM frame. */
            __app_player_pa_acquire(player);
            PlayerErr play_ret = lisa_player_play(player->hld);
            if (play_ret != PLAYER_OK) {
                LISA_LOGE(TAG, "Core: Auto play failed: %s, ret=%d",
                          player->name, play_ret);
                __app_player_pa_release(player, 0);
                player->prepare_error = true;
                // 播放失败，通知上层处理错误
                if (player->core_upper_callback) {
                    app_player_core_callback_t cb =
                        (app_player_core_callback_t)player->core_upper_callback;
                    cb(player, PLAYER_EVT_ERROR, true);
                }
                PLAYER_MUTEX_UNLOCK(player->cancel_lock);
                return 0;
            }
            lisa_semaphore_give(player->preparing_sem);
            PLAYER_MUTEX_UNLOCK(player->cancel_lock);
            break;
        }

        case PLAYER_EVT_PLAYBACK_COMPLETE:
        case PLAYER_EVT_PAUSED: {
            // === Core 层职责 4: 暂停完成，唤醒同步等待 ===
            LISA_LOGI(TAG, "Core: Player %s paused, releasing pause_sem", player->name);
            lisa_semaphore_give(player->pause_sem);
            break;
        }

        case PLAYER_EVT_ERROR: {
            // === Core 层职责 5: ERROR 事件需要自动 reset ===
            LISA_LOGW(TAG, "Core: Player %s error, auto reset", player->name);
            player->prepare_error = true;
            lisa_semaphore_give(player->preparing_sem);
            lisa_semaphore_give(player->pause_sem);
            lisa_player_reset(player->hld);
            break;
        }

        default:
            break;
    }

    // === Core 层职责 6: 更新内部状态 ===
    player->last_evt = evt;

    // === Core 层职责 7: 调用上层回调（通知 PA、焦点管理等）===
    /* drain_in_progress 期间（play_ex 走 play+stop_sync 排空残留 PREPARED 状态）
     * 跳过上层回调，避免 PA 闪烁、焦点抖动以及虚假的 PLAYING/STOPPED 事件
     * 被业务层当成真实播放收到。 */
    if (player->core_upper_callback && !player->drain_in_progress) {
        app_player_core_callback_t cb =
            (app_player_core_callback_t)player->core_upper_callback;
        cb(player, evt, should_notify_user);
    }

    return 0;
}

/**
 * @brief 初始化 core 层，设置回调链
 */
int app_player_core_init(app_player_t *player, app_player_core_callback_t upper_callback)
{
    if (!player || !upper_callback) {
        LISA_LOGE(TAG, "Core init failed: invalid parameters");
        return -1;
    }

    // 保存上层回调
    player->core_upper_callback = (void *)upper_callback;

    // 设置 lisa_player 的回调
    PlayerErr ret = lisa_player_set_callback(player->hld, __lisa_player_core_callback_handler);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core init failed: set lisa_player callback error %d", ret);
        return -1;
    }

    LISA_LOGI(TAG, "Core layer initialized for player: %s", player->name);
    return 0;
}

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
        LISA_LOGI(TAG, "Wait %s prepare complete...", player->name);
        lisa_player_pre_close(player->hld);

        // 回调线程执行后，is_preparing会被置为false
        // 因此此处信号量可能会一直等待
        if (player->is_preparing) {
            player->wait_prepare_intercepted = true;
            lisa_semaphore_take(player->preparing_sem, LISA_OS_WAIT_FOREVER);
            player->wait_prepare_intercepted = false;
        }

        LISA_LOGI(TAG, "Pre %s prepare complete", player->name);
        lisa_player_stop_sync(player->hld);
        lisa_player_reset(player->hld);
        LISA_LOGI(TAG, "Pre %s status check end", player->name);
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

    LISA_LOGI(TAG, "Core play: %s, url=%s, throw_time=%d", player->name, url, throw_time);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    lisa_player_reset(player->hld);


    // 设置流式模式标志为false（URL播放模式）
    player->is_stream_mode = false;

    // 设置跳过开始指定时长内的低能量段
    if (throw_time > 0) {
        lisa_player_throw_low_energy(player->hld, throw_time);
    }

    // 设置preparing标志
    player->is_preparing = true;
    player->pause_preparing = false;
    player->prepare_error = false;

    lisa_semaphore_reset(player->preparing_sem);

    // 设置URL并准备（seturl 会自动触发准备过程）
    PlayerErr ret = lisa_player_seturl(player->hld, url);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core play failed: seturl error %d", ret);
        player->is_preparing = false;
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    ret = lisa_semaphore_take(player->preparing_sem, SEMAPHORE_WAIT_TIMEOUT_MS);

    // 如果信号量超时，执行 reset 并返回 error 事件
    if (ret != LISA_OK) {
        LISA_LOGE(TAG, "Core play failed: wait prepare timeout");
        player->is_preparing = false;
        lisa_player_reset(player->hld);
        __enqueue_callback_event(player, APP_PLAYER_EVENT_ERROR);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    // 检查是否因为ERROR事件被唤醒
    if (player->prepare_error) {
        LISA_LOGE(TAG, "Core play failed: prepare error occurred");
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

    LISA_LOGI(TAG, "Core play stream: %s, rate=%u, ch=%u, bits=%u",
              player->name, sample_rate, channels, bits);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    lisa_player_reset(player->hld);

    // 设置流式模式标志
    player->is_stream_mode = true;

    // 设置preparing标志
    player->is_preparing = true;
    player->pause_preparing = false;
    player->prepare_error = false;

    lisa_semaphore_reset(player->preparing_sem);

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
 * @brief 核心层：同步暂停播放
 * @note 同步暂停，阻塞等待操作完成
 */
int app_player_core_pause(app_player_t *player)
{
    if (!player || player->is_stream_mode) {
        return -1;
    }

    LISA_LOGI(TAG, "Core pause sync: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 检查当前状态，如果已经是PAUSED，无需重复pause
    PlayerState state = lisa_player_get_state(player->hld);
    if (state == PLAYER_ST_PAUSED) {
        LISA_LOGI(TAG, "Core pause sync: %s already paused, skip", player->name);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return 0;
    }

    // 如果正在准备中，设置pause_preparing标志
    if (player->is_preparing) {
        player->pause_preparing = true;
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        LISA_LOGI(TAG, "Core pause sync: %s preparing, set pause_preparing flag", player->name);
        return 0;
    }

    // 确保信号量处于清空状态，避免之前的 give 导致立即返回
    lisa_semaphore_reset(player->pause_sem);

    // 调用底层异步暂停
    PlayerErr ret = lisa_player_pause(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core pause sync failed: lisa_player_pause error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    // 等待暂停完成（同步）
    LISA_LOGI(TAG, "Core pause sync: waiting for PAUSED event...");
    ret = lisa_semaphore_take(player->pause_sem, SEMAPHORE_WAIT_TIMEOUT_MS);

    // 如果信号量超时，执行 reset 并返回 error 事件
    if (ret != LISA_OK) {
        LISA_LOGE(TAG, "Core pause sync failed: wait pause timeout");
        lisa_player_reset(player->hld);
        __enqueue_callback_event(player, APP_PLAYER_EVENT_ERROR);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    LISA_LOGI(TAG, "Core pause sync: PAUSED event received");

    PLAYER_MUTEX_UNLOCK(player->core_lock);

    return 0;
}

/**
 * @brief 核心层：恢复播放（异步）
 */
int app_player_core_resume(app_player_t *player)
{
    if (!player || player->is_stream_mode) {
        return -1;
    }

    LISA_LOGI(TAG, "Core resume: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 如果是在准备阶段被暂停的，清除pause_preparing标志并开始播放
    if (player->pause_preparing) {
        player->pause_preparing = false;
        __app_player_pa_acquire(player);
        PlayerErr ret = lisa_player_play(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Core resume failed: lisa_player_play error %d", ret);
            __app_player_pa_release(player, 0);
            PLAYER_MUTEX_UNLOCK(player->core_lock);
            return -1;
        }
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return 0;
    }

    // 调用底层的恢复接口
    __app_player_pa_acquire(player);
    PlayerErr ret = lisa_player_resume(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core resume failed: lisa_player_resume error %d", ret);
        __app_player_pa_release(player, 0);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：同步恢复播放
 */
int app_player_core_resume_sync(app_player_t *player)
{
    if (!player || player->is_stream_mode) {
        return -1;
    }

    LISA_LOGI(TAG, "Core resume sync: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    // 如果是在准备阶段被暂停的，清除pause_preparing标志并开始播放
    if (player->pause_preparing) {
        player->pause_preparing = false;
        __app_player_pa_acquire(player);
        PlayerErr ret = lisa_player_play(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Core resume sync failed: lisa_player_play error %d", ret);
            __app_player_pa_release(player, 0);
            PLAYER_MUTEX_UNLOCK(player->core_lock);
            return -1;
        }
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return 0;
    }

    // 调用底层的同步恢复接口
    __app_player_pa_acquire(player);
    PlayerErr ret = lisa_player_resume_sync(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Core resume sync failed: lisa_player_resume_sync error %d", ret);
        __app_player_pa_release(player, 0);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：停止播放（异步）
 */
int app_player_core_stop(app_player_t *player)
{
    if (!player) {
        return -1;
    }

    LISA_LOGI(TAG, "Core stop: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    int ret = lisa_player_stop(player->hld);
    if (ret != PLAYER_OK) {
        // 异步stop失败，reset并手动发送事件（异常情况）
        LISA_LOGW(TAG, "Core stop failed: %d, resetting player", ret);

        // 无效状态下执行该函数会卡住
        //lisa_player_reset(player->hld);
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

    LISA_LOGI(TAG, "Core stop sync: %s", player->name);

    PLAYER_MUTEX_LOCK(player->core_lock, LISA_OS_WAIT_FOREVER);

    int ret = lisa_player_stop_sync(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGW(TAG, "Core stop sync failed: %d, resetting player", ret);

        // 无效状态下执行该函数会卡住
        // lisa_player_reset(player->hld);
        __enqueue_callback_event(player, APP_PLAYER_EVENT_STOPPED);

        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return 0;
}

/**
 * @brief 核心层：写入流式数据
 * @note 严格按照原 app_player_write_stream 的实现方式（使用timeout_ms参数）
 * @note 首次写入数据时会等待 prepare 完成
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

    // 记录是否是首次写入（用于后续等待 prepare）
    bool is_first_write = (player->is_preparing && data != NULL && size > 0);

    LISA_LOGI(TAG, "Core stream write: data=%p, size=%zu, timeout=%u, is_first_write=%d", data, size, timeout_ms, is_first_write);

    // 调用底层写入接口
    int ret = lisa_player_put_stream_data(player->hld, (uint8_t *)data, size, timeout_ms);
    if (ret < 0) {
        LISA_LOGE(TAG, "Core stream write failed: %d", ret);
        PLAYER_MUTEX_UNLOCK(player->core_lock);
        return -1;
    }

    // 首次写入数据后，需要等待 prepare 完成并开始播放
    if (is_first_write) {
        LISA_LOGI(TAG, "Core stream write: first write completed, waiting for prepare...");

        PlayerErr wait_ret = lisa_semaphore_take(player->preparing_sem, SEMAPHORE_WAIT_TIMEOUT_MS);

        // 如果信号量超时，执行 reset 并返回 error 事件
        if (wait_ret != LISA_OK) {
            LISA_LOGE(TAG, "Core stream write failed: wait prepare timeout");
            player->is_preparing = false;
            player->is_stream_mode = false;
            lisa_player_reset(player->hld);
            __enqueue_callback_event(player, APP_PLAYER_EVENT_ERROR);
            PLAYER_MUTEX_UNLOCK(player->core_lock);
            return -1;
        }

        // 检查是否因为ERROR事件被唤醒
        if (player->prepare_error) {
            LISA_LOGE(TAG, "Core stream write failed: prepare error occurred");
            player->is_stream_mode = false;
            PLAYER_MUTEX_UNLOCK(player->core_lock);
            return -1;
        }

        LISA_LOGI(TAG, "Core stream write: prepare completed, starting playback...");
    }

    PLAYER_MUTEX_UNLOCK(player->core_lock);
    return ret;
}
