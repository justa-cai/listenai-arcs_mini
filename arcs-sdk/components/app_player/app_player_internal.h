/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_PLAYER_INTERNAL_H
#define APP_PLAYER_INTERNAL_H

#include <FreeRTOS.h>
#include <semphr.h>
#include "lisa_semaphore.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "lisa_player.h"
#include "app_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内部共享定义
 *
 * 本头文件包含 app_player 各模块之间共享的数据结构和函数声明
 */

// 互斥锁操作宏
#define PLAYER_MUTEX_CREATE() xSemaphoreCreateRecursiveMutex()

#define PLAYER_MUTEX_LOCK(mutex, timeout) \
    do { \
        if (mutex) { \
            TickType_t wait_tick = ((timeout) == LISA_OS_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout); \
            xSemaphoreTakeRecursive((mutex), wait_tick); \
        } \
    } while(0)

#define PLAYER_MUTEX_UNLOCK(mutex) \
    do { \
        if (mutex) { \
            xSemaphoreGiveRecursive((mutex)); \
        } \
    } while(0)

#define PLAYER_MUTEX_DELETE(mutex) \
    do { \
        if (mutex) { \
            vSemaphoreDelete((mutex)); \
            (mutex) = NULL; \
        } \
    } while(0)

/**
 * @brief 回调事件项
 */
typedef struct callback_event {
    app_player_event_t event;       /**< 事件类型 */
    struct callback_event *next;    /**< 链表指针 */
    uint32_t request_tag;
} callback_event_t;

/**
 * @brief 回调事件队列
 */
typedef struct {
    callback_event_t *head;         /**< 队列头 */
    callback_event_t *tail;         /**< 队列尾 */
    SemaphoreHandle_t lock;         /**< 队列锁 */
    lisa_semaphore_t *sem;          /**< 队列信号量 */
    bool running;                   /**< 线程运行标志 */
} callback_queue_t;

/**
 * @brief 回调映射管理
 */
typedef struct {
    SemaphoreHandle_t lock;        /**< 回调锁 */
    app_player_event_cb_t callback; /**< 事件回调函数 */
    void *user_data;                /**< 用户数据 */
} callback_map_t;

/**
 * @brief 播放器实例内部结构
 */
struct app_player_s {
    char *name;                     /**< 播放器名称 */
    uint32_t id;                    /**< 播放器 ID */
    PLAYER_HANDLE hld;              /**< lisa_player 句柄 */

    SemaphoreHandle_t operation_lock;   /**< 实例操作互斥锁(保护状态和操作) */
    SemaphoreHandle_t core_lock;        /**< 核心层操作互斥锁(保护 core 层函数) */

    callback_map_t cb_map;          /**< 回调管理 */
    void *core_upper_callback;      /**< Core 层的上层回调 (app_player_core_callback_t) */

    app_player_state_t state;       /**< 当前状态 */
    PlayerEvt last_evt;             /**< 最后一次事件 */

    bool is_preparing;              /**< 准备中标志 */
    bool stop_preparing_requested;  /**< preparing 阶段收到 stop，请求抑制自动播放 */
    bool wait_prepare_intercepted;  /**< 等待准备中断标志 */
    bool pause_preparing;           /**< 准备时暂停标志 */
    bool prepare_error;             /**< 准备过程中发生错误标志 */
    bool drain_in_progress;         /**< 排空残留 PREPARED 状态中，抑制上层回调 */
    lisa_semaphore_t *preparing_sem; /**< 准备同步信号量 */
    lisa_semaphore_t *pause_sem;    /**< 暂停同步信号量 */

    uint8_t vol_min;                /**< 音量最小值 */
    uint8_t vol_max;                /**< 音量最大值 */

    bool is_stream_mode;            /**< 流式播放模式标志 */
    bool pa_is_on;                  /**< PA使用标志：true=此实例正在使用PA */

    callback_queue_t *cb_queue;     /**< 回调事件队列 */
    lisa_thread_t *cb_thread;       /**< 回调处理线程 */

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    int focus_channel_id;           /**< 焦点通道 ID（-1 表示未注册） */
    app_player_focus_change_cb_t focus_cb; /**< 焦点变化回调函数 */
    void *focus_user_data;          /**< 焦点回调用户数据 */
    SemaphoreHandle_t focus_cb_lock;    /**< 焦点回调锁 */
    app_player_focus_behavior_t behavior; /**< 焦点行为配置 */
    bool paused_by_focus;           /**< 是否因焦点策略而暂停 */
    bool user_initiated_stop;       /**< 用户主动调用stop/pause标志(用于跳过焦点策略) */
    app_player_focus_state_t last_focus_state; /**< 上一次焦点状态 */
    char *pending_url;              /**< 待播放URL（焦点被抢占时保存） */
    uint32_t pending_throw_time;    /**< 待播放的throw_time参数 */
#endif
    const volatile uint32_t *play_cancel_token;
    uint32_t play_request_tag;
    uint32_t callback_request_tag;
    SemaphoreHandle_t cancel_lock;
    bool cancel_requested;
};

/**
 * @brief 向回调队列添加事件（内部函数，由 core 层调用）
 * @param player 播放器实例
 * @param event 事件类型
 */
void __enqueue_callback_event(app_player_t *player, app_player_event_t event);

bool __app_player_play_cancelled(const app_player_t *player);
bool __app_player_play_cancelled_locked(const app_player_t *player);

/**
 * @brief 通过播放器ID查找播放器实例（内部函数，由 core 层调用）
 * @param id 播放器ID
 * @return 播放器实例指针，未找到返回 NULL
 */
app_player_t *__find_player_by_id(uint32_t id);

/**
 * @brief 通过焦点通道ID查找播放器实例（内部函数，由 focus 层调用）
 * @param focus_channel_id 焦点通道ID
 * @return 播放器实例指针，未找到返回 NULL
 */
app_player_t *__find_player_by_focus_channel_id(int focus_channel_id);

/** Acquire/release this player's PA reference around actual audio output. */
int __app_player_pa_acquire(app_player_t *player);
void __app_player_pa_release(app_player_t *player, uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLAYER_INTERNAL_H */
