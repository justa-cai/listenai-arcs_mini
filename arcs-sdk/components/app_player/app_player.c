/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <string.h>
#include "lisa_mem.h"
#include "lisa_log.h"
#include "app_player_internal.h"
#include "app_player_core.h"
#include "pa_manager.h"
#include "lisa_player_adapter.h"

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
#include "app_player_focus.h"
#include "listen_audiomgr.h"
#endif

#define TAG "APP_PLAYER"

#define APP_PLAYER_VOL_MIN (0)
#define APP_PLAYER_VOL_MAX (100)

// 注意：互斥锁宏和数据结构已移到 app_player_internal.h

/**
 * @brief 全局播放器 ID 计数器
 */
static uint32_t s_player_id_counter = 0;

/**
 * @brief 全局 ID 计数器锁
 */
static SemaphoreHandle_t s_id_counter_lock = NULL;

/**
 * @brief 全局模块初始化标志
 */
static bool s_app_player_initialized = false;

/**
 * @brief 全局实例链表节点
 */
typedef struct player_instance_node {
    app_player_t *player;           /**< 播放器实例 */
    struct player_instance_node *next; /**< 下一个节点 */
} player_instance_node_t;

/**
 * @brief 全局实例链表头
 */
static player_instance_node_t *s_player_list_head = NULL;

/**
 * @brief 全局实例链表锁
 */
static SemaphoreHandle_t s_player_list_lock = NULL;

// 注意：callback_event_t, callback_queue_t, callback_map_t, app_player_s 结构体
// 已移到 app_player_internal.h

/**
 * @brief 初始化全局实例链表锁
 */
static void __init_player_list(void)
{
    if (!s_player_list_lock) {
        s_player_list_lock = PLAYER_MUTEX_CREATE();
    }
}

/**
 * @brief 注册播放器实例到全局链表
 * @param player 播放器实例
 * @return 0成功，负数失败
 */
static int __register_player_instance(app_player_t *player)
{
    if (!player) {
        return -1;
    }

    __init_player_list();

    player_instance_node_t *node = lisa_mem_calloc(1, sizeof(player_instance_node_t));
    if (!node) {
        LISA_LOGE(TAG, "Register instance failed: no memory");
        return -1;
    }

    node->player = player;

    PLAYER_MUTEX_LOCK(s_player_list_lock, LISA_OS_WAIT_FOREVER);
    node->next = s_player_list_head;
    s_player_list_head = node;
    PLAYER_MUTEX_UNLOCK(s_player_list_lock);

    LISA_LOGI(TAG, "Instance registered: id=%u", player->id);
    return 0;
}

/**
 * @brief 从全局链表注销播放器实例
 * @param player 播放器实例
 */
static void __unregister_player_instance(app_player_t *player)
{
    if (!player || !s_player_list_lock) {
        return;
    }

    PLAYER_MUTEX_LOCK(s_player_list_lock, LISA_OS_WAIT_FOREVER);

    player_instance_node_t **curr = &s_player_list_head;
    while (*curr) {
        if ((*curr)->player == player) {
            player_instance_node_t *to_free = *curr;
            *curr = (*curr)->next;
            lisa_mem_free(to_free);
            LISA_LOGI(TAG, "Instance unregistered: id=%u", player->id);
            break;
        }
        curr = &((*curr)->next);
    }

    PLAYER_MUTEX_UNLOCK(s_player_list_lock);
}

/**
 * @brief 根据ID查找播放器实例（导出给 core 层使用）
 * @param id 播放器ID
 * @return 播放器实例指针，未找到返回NULL
 */
app_player_t *__find_player_by_id(uint32_t id)
{
    if (!s_player_list_lock) {
        return NULL;
    }

    app_player_t *result = NULL;

    PLAYER_MUTEX_LOCK(s_player_list_lock, LISA_OS_WAIT_FOREVER);

    player_instance_node_t *curr = s_player_list_head;
    while (curr) {
        if (curr->player && curr->player->id == id) {
            result = curr->player;
            break;
        }
        curr = curr->next;
    }

    PLAYER_MUTEX_UNLOCK(s_player_list_lock);

    return result;
}

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
/**
 * @brief 通过焦点通道ID查找播放器实例（导出给 focus 模块使用）
 * @param focus_channel_id 焦点通道ID
 * @return 播放器实例指针，未找到返回 NULL
 */
app_player_t *__find_player_by_focus_channel_id(int focus_channel_id)
{
    if (!s_player_list_lock || focus_channel_id < 0) {
        return NULL;
    }

    app_player_t *result = NULL;

    PLAYER_MUTEX_LOCK(s_player_list_lock, LISA_OS_WAIT_FOREVER);

    player_instance_node_t *curr = s_player_list_head;
    while (curr) {
        if (curr->player && curr->player->focus_channel_id == focus_channel_id) {
            result = curr->player;
            break;
        }
        curr = curr->next;
    }

    PLAYER_MUTEX_UNLOCK(s_player_list_lock);

    return result;
}
#endif // CONFIG_APP_PLAYER_AUDIO_FOCUS

/**
 * @brief   初始化 app_player 模块
 * @param   config 初始化配置参数
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    必须在创建播放器实例前调用此函数
 */
int app_player_init(const app_player_config_t *config)
{
    if (!config) {
        LISA_LOGE(TAG, "Init failed: config is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!config->pa_ctrl_callback) {
        LISA_LOGE(TAG, "Init failed: pa_ctrl_callback is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (s_app_player_initialized) {
        LISA_LOGW(TAG, "App player already initialized");
        return APP_PLAYER_OK;
    }

    LISA_LOGI(TAG, "Initializing app_player module");

    // 设置 PCM 输出回调（如果提供）
    app_player_adapter_set_pcm_output(config->pcm_output_callback);

    // 初始化 PA 管理器
    pa_manager_config_t pa_config = {
        .ctrl_callback = config->pa_ctrl_callback
    };

    int ret = pa_manager_init(&pa_config);
    if (ret != 0) {
        LISA_LOGE(TAG, "Init failed: pa_manager_init error %d", ret);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    lisa_player_set_loglev(2);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 初始化焦点管理器（如果配置了焦点通道）
    if (config->focus_configs && config->focus_config_count > 0) {
        ret = app_player_focus_init(config->focus_configs, config->focus_config_count);
        if (ret != 0) {
            LISA_LOGE(TAG, "Init failed: app_player_focus_init error %d", ret);
            return APP_PLAYER_ERR_INVALID_STATE;
        }
    } else {
        LISA_LOGI(TAG, "Audio focus disabled (no focus configs)");
    }
#endif

    s_app_player_initialized = true;

    LISA_LOGI(TAG, "App player module initialized successfully");
    return APP_PLAYER_OK;
}

/**
 * @brief   分配唯一的播放器 ID（线程安全）
 * @return  分配的 ID
 */
static uint32_t __alloc_player_id(void)
{
    // 延迟初始化全局锁（首次调用时创建）
    if (!s_id_counter_lock) {
        s_id_counter_lock = PLAYER_MUTEX_CREATE();
        if (!s_id_counter_lock) {
            LISA_LOGE(TAG, "Failed to create ID counter lock");
            return 0;  // 返回 0 表示失败
        }
    }

    PLAYER_MUTEX_LOCK(s_id_counter_lock, LISA_OS_WAIT_FOREVER);
    uint32_t id = s_player_id_counter++;
    PLAYER_MUTEX_UNLOCK(s_id_counter_lock);

    return id;
}

/**
 * @brief 将lisa_player事件转换为app_player事件
 * @param evt lisa_player事件
 * @return app_player事件
 */
static app_player_event_t __convert_player_evt_to_app_event(PlayerEvt evt)
{
    switch (evt) {
        case PLAYER_EVT_PREPARED:
            return APP_PLAYER_EVENT_PREPARED;
        case PLAYER_EVT_PLAYING:
            return APP_PLAYER_EVENT_PLAYING;
        case PLAYER_EVT_PAUSED:
            return APP_PLAYER_EVENT_PAUSED;
        case PLAYER_EVT_STOPED:
            return APP_PLAYER_EVENT_STOPPED;
        case PLAYER_EVT_PLAYBACK_COMPLETE:
            return APP_PLAYER_EVENT_COMPLETED;
        case PLAYER_EVT_SEEK_COMPLETE:
            return APP_PLAYER_EVENT_SEEK_COMPLETE;
        case PLAYER_EVT_ERROR:
        default:
            return APP_PLAYER_EVENT_ERROR;
    }
}

/**
 * @brief 将事件加入回调队列（导出给 core 模块使用）
 * @param player 播放器实例
 * @param event 事件类型
 */
void __enqueue_callback_event(app_player_t *player, app_player_event_t event)
{
    if (!player || !player->cb_queue) {
        return;
    }

    // 创建事件
    callback_event_t *evt = lisa_mem_calloc(1, sizeof(callback_event_t));
    if (!evt) {
        LISA_LOGE(TAG, "Enqueue event failed: no memory");
        return;
    }

    evt->event = event;
    evt->request_tag = player->play_request_tag;
    evt->next = NULL;

    // 加入队列
    callback_queue_t *queue = player->cb_queue;
    PLAYER_MUTEX_LOCK(queue->lock, LISA_OS_WAIT_FOREVER);

    if (queue->tail) {
        queue->tail->next = evt;
    } else {
        queue->head = evt;
    }
    queue->tail = evt;

    PLAYER_MUTEX_UNLOCK(queue->lock);

    // 通知回调线程
    lisa_semaphore_give(queue->sem);
}

/**
 * @brief 回调处理线程（每个实例独立）
 * @param arg 播放器实例指针
 */
static void __callback_thread_func(void *arg)
{
    app_player_t *player = (app_player_t *)arg;
    if (!player || !player->cb_queue) {
        return;
    }

    callback_queue_t *queue = player->cb_queue;

    LISA_LOGI(TAG, "Callback thread started for player[%d] %s", player->id, player->name);

    while (queue->running) {
        // 等待事件
        if (lisa_semaphore_take(queue->sem, LISA_OS_WAIT_FOREVER) != 0) {
            continue;
        }

        while (1) {
            // 从队列取出事件
            PLAYER_MUTEX_LOCK(queue->lock, LISA_OS_WAIT_FOREVER);
            callback_event_t *evt = queue->head;
            if (evt) {
                queue->head = evt->next;
                if (!queue->head) {
                    queue->tail = NULL;
                }
            }
            PLAYER_MUTEX_UNLOCK(queue->lock);

            if (!evt) {
                break; // 队列为空
            }

            // 读取回调信息
            PLAYER_MUTEX_LOCK(player->cb_map.lock, LISA_OS_WAIT_FOREVER);
            app_player_event_cb_t cb = player->cb_map.callback;
            void *user_data = player->cb_map.user_data;
            PLAYER_MUTEX_UNLOCK(player->cb_map.lock);

            // 调用用户回调（不持锁）
            if (cb) {
                player->callback_request_tag = evt->request_tag;
                cb(player, evt->event, user_data);
            }

            // 释放事件内存
            lisa_mem_free(evt);
        }
    }

    LISA_LOGI(TAG, "Callback thread stopped for player[%d] %s", player->id, player->name);
}

// 注意：__inner_player_pause/resume/stop, __execute_focus_loss_policy,
// __audio_focus_change_bridge 已移到 app_player_focus.c

/**
 * @brief App 层回调处理函数（从 Core 层回调而来）
 * @param player 播放器实例
 * @param evt lisa_player 原始事件
 * @param should_notify_user 是否需要通知用户
 *
 * @note App 层职责：
 * 1. PA 管理（功放控制）- 基于引用计数机制
 * 2. 音频焦点管理
 * 3. 事件转换和通知用户
 *
 * @note PA控制策略（方案3）：
 * - 只在事件回调中控制PA，不在play/pause/stop接口中控制
 * - PLAYING事件：开PA（引用计数+1）
 * - PAUSED/STOPPED/COMPLETE/ERROR事件：关PA（引用计数-1）
 * - 配合PA管理器的引用计数，避免多实例互相干扰
 */
int __app_player_pa_acquire(app_player_t *player)
{
    if (!player || player->pa_is_on) {
        return 0;
    }

    int ret = pa_manager_control(1, 0);
    if (ret != 0) {
        LISA_LOGE(TAG, "Player[%d] %s: PA ON failed: %d",
                  player->id, player->name, ret);
        return ret;
    }

    player->pa_is_on = true;
    LISA_LOGI(TAG, "Player[%d] %s: PA acquired before audio output",
              player->id, player->name);
    return 0;
}

void __app_player_pa_release(app_player_t *player, uint32_t delay_ms)
{
    if (!player || !player->pa_is_on) {
        return;
    }

    int ret = pa_manager_control(0, delay_ms);
    /* The manager consumes the reference before applying the hardware state. */
    player->pa_is_on = false;
    if (ret != 0) {
        LISA_LOGE(TAG, "Player[%d] %s: PA OFF failed: %d",
                  player->id, player->name, ret);
        return;
    }
}

static void __app_player_upper_callback_handler(app_player_t *player, PlayerEvt evt, bool should_notify_user)
{
    if (!player) {
        return;
    }

    LISA_LOGI(TAG, "App callback: Player[%d] %s evt=%d, notify_user=%d",
              player->id, player->name, evt, should_notify_user);

    // === App 层职责 1: PA 管理（基于实际播放状态，使用标志避免重复控制）===
    switch (evt) {
        case PLAYER_EVT_PLAYING: {
            // 真正开始播放时才开启PA
            if (!player->pa_is_on) {
                LISA_LOGI(TAG, "Player[%d] %s: PLAYING -> PA ON", player->id, player->name);
                __app_player_pa_acquire(player);
            } else {
                LISA_LOGI(TAG, "Player[%d] %s: PLAYING but PA already on, skip", player->id, player->name);
            }
            break;
        }

        case PLAYER_EVT_PAUSED:
        case PLAYER_EVT_STOPED:
        case PLAYER_EVT_PLAYBACK_COMPLETE:
        case PLAYER_EVT_ERROR: {
            // 播放结束、暂停、错误时关闭PA（延迟关闭，避免频繁开关）
            // 只有当PA确实被此实例打开时才关闭
            if (player->pa_is_on) {
                // 特殊情况：Stop后立即来Error，不需要再关闭PA
                if (player->last_evt == PLAYER_EVT_STOPED && evt == PLAYER_EVT_ERROR) {
                    LISA_LOGI(TAG, "Player[%d] %s: skip PA off after stop+error", player->id, player->name);
                } else {
                    LISA_LOGI(TAG, "Player[%d] %s: evt=%d -> PA OFF (delayed %dms)",
                              player->id, player->name, evt, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);
                    __app_player_pa_release(player, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);
                }
            } else {
                LISA_LOGI(TAG, "Player[%d] %s: evt=%d but PA not on, skip PA off",
                          player->id, player->name, evt);
            }
            break;
        }

        default:
            break;
    }

    // === App 层职责 2: 音频焦点管理 ===
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    switch (evt) {
        case PLAYER_EVT_PAUSED:
        case PLAYER_EVT_STOPED:
        case PLAYER_EVT_PLAYBACK_COMPLETE:
        case PLAYER_EVT_ERROR: {
            // 播放完成、停止、错误或用户主动暂停/停止时释放焦点
            // 注意：URL切换场景下的STOP(user_initiated_stop=false)不释放焦点，避免其他播放器短暂恢复
            bool user_initiated = app_player_focus_is_user_initiated(player);
            if (evt == PLAYER_EVT_PLAYBACK_COMPLETE || evt == PLAYER_EVT_ERROR ||
                ((evt == PLAYER_EVT_PAUSED || evt == PLAYER_EVT_STOPED) && user_initiated)) {
                LISA_LOGI(TAG, "Releasing audio focus for player %s (evt=%d, user_initiated=%d)",
                         player->name, evt, user_initiated);
                app_player_focus_release(player, user_initiated);
                // 释放后清除用户主动标志
                app_player_focus_set_user_initiated(player, false);
            } else if (evt == PLAYER_EVT_STOPED && !user_initiated) {
                LISA_LOGI(TAG, "Player %s stopped for URL switch, keeping focus", player->name);
            }
            break;
        }

        default:
            break;
    }
#endif

    // === App 层职责 3: 事件转换和通知用户 ===
    if (should_notify_user) {
        app_player_event_t app_evt = __convert_player_evt_to_app_event(evt);
        __enqueue_callback_event(player, app_evt);
    }
}

/**
 * @brief   创建播放器实例
 * @param   name 播放器名称
 * @return  播放器实例指针，失败返回 NULL
 */
app_player_t *app_player_create(const char *name)
{
    if (!name) {
        LISA_LOGE(TAG, "Create failed: name is NULL");
        return NULL;
    }

    if (!s_app_player_initialized) {
        LISA_LOGE(TAG, "Create failed: app_player not initialized, call app_player_init first");
        return NULL;
    }

    LISA_LOGI(TAG, "Creating player: %s", name);

    // 分配播放器结构体
    app_player_t *player = (app_player_t *)lisa_mem_calloc(1, sizeof(app_player_t));
    if (!player) {
        LISA_LOGE(TAG, "Create failed: no memory for player");
        return NULL;
    }

    // 复制名称
    player->name = lisa_mem_alloc(strlen(name) + 1);
    if (!player->name) {
        LISA_LOGE(TAG, "Create failed: no memory for name");
        goto _err;
    }
    strcpy(player->name, name);

    // 分配唯一 ID（线程安全）
    player->id = __alloc_player_id();

    // 创建实例操作互斥锁
    player->operation_lock = PLAYER_MUTEX_CREATE();
    if (!player->operation_lock) {
        LISA_LOGE(TAG, "Create failed: operation_lock create failed");
        goto _err;
    }

    // 创建核心层操作互斥锁
    player->core_lock = PLAYER_MUTEX_CREATE();
    if (!player->core_lock) {
        LISA_LOGE(TAG, "Create failed: core_lock create failed");
        goto _err;
    }

    // 创建底层播放器句柄
    player->hld = lisa_player_create(name, player->id);
    if (!player->hld) {
        LISA_LOGE(TAG, "Create failed: lisa_player_create failed");
        goto _err;
    }

    // 初始化回调管理
    player->cb_map.lock = PLAYER_MUTEX_CREATE();
    if (!player->cb_map.lock) {
        LISA_LOGE(TAG, "Create failed: mutex create failed");
        goto _err;
    }
    player->cb_map.callback = NULL;
    player->cb_map.user_data = NULL;
    player->core_upper_callback = NULL;

    // 创建准备信号量
    player->preparing_sem = lisa_semaphore_create(1);
    if (!player->preparing_sem) {
        LISA_LOGE(TAG, "Create failed: preparing semaphore create failed");
        goto _err;
    }

    // 创建暂停信号量
    player->pause_sem = lisa_semaphore_create(1);
    if (!player->pause_sem) {
        LISA_LOGE(TAG, "Create failed: pause semaphore create failed");
        goto _err;
    }

    // 初始化状态
    player->state = APP_PLAYER_STATE_IDLE;
    player->last_evt = PLAYER_EVT_INIT;
    player->play_cancel_token = NULL;
    player->play_request_tag = 0U;
    player->callback_request_tag = 0U;
    player->cancel_lock = PLAYER_MUTEX_CREATE();
    if (!player->cancel_lock) {
        LISA_LOGE(TAG, "Create failed: cancel_lock create failed");
        goto _err;
    }
    player->cancel_requested = false;
    player->is_preparing = false;
    player->stop_preparing_requested = false;
    player->wait_prepare_intercepted = false;
    player->pause_preparing = false;
    player->is_stream_mode = false;
    player->pa_is_on = false;  // 初始状态：未使用PA

    // 初始化音量范围
    player->vol_min = APP_PLAYER_VOL_MIN;
    player->vol_max = APP_PLAYER_VOL_MAX;

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 初始化焦点相关字段
    player->focus_channel_id = -1;
    player->focus_cb = NULL;
    player->focus_user_data = NULL;
    player->focus_cb_lock = PLAYER_MUTEX_CREATE();
    if (!player->focus_cb_lock) {
        LISA_LOGE(TAG, "Create failed: focus_cb_lock create failed");
        goto _err;
    }
    player->paused_by_focus = false;
    app_player_focus_set_user_initiated(player, false);
    player->last_focus_state = APP_PLAYER_FOCUS_NONE;
    player->pending_url = NULL;
    player->pending_throw_time = 0;
    // behavior 将在注册焦点通道时从配置中复制
#endif

    // 创建回调事件队列
    player->cb_queue = lisa_mem_calloc(1, sizeof(callback_queue_t));
    if (!player->cb_queue) {
        LISA_LOGE(TAG, "Create failed: no memory for callback queue");
        goto _err;
    }

    player->cb_queue->lock = PLAYER_MUTEX_CREATE();
    if (!player->cb_queue->lock) {
        LISA_LOGE(TAG, "Create failed: callback queue mutex create failed");
        goto _err;
    }

    player->cb_queue->sem = lisa_semaphore_create(1);
    if (!player->cb_queue->sem) {
        LISA_LOGE(TAG, "Create failed: callback queue semaphore create failed");
        goto _err;
    }

    player->cb_queue->head = NULL;
    player->cb_queue->tail = NULL;
    player->cb_queue->running = true;

    // 创建回调处理线程
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "cb_%s", name);
    lisa_thread_attr_t thread_attr = {
        .name = (uint8_t *)thread_name,
        .stack_size = CONFIG_APP_PLAYER_CALLBACK_THREAD_STACK_SIZE,
        .priority = CONFIG_APP_PLAYER_CALLBACK_THREAD_PRIORITY
    };
    player->cb_thread = lisa_thread_create(&thread_attr,
                                           __callback_thread_func,
                                           player);
    if (!player->cb_thread) {
        LISA_LOGE(TAG, "Create failed: callback thread create failed");
        player->cb_queue->running = false;
        goto _err;
    }

    // 初始化 core 层，设置回调链
    if (app_player_core_init(player, __app_player_upper_callback_handler) != 0) {
        LISA_LOGE(TAG, "Create failed: app_player_core_init failed");
        player->cb_queue->running = false;
        lisa_semaphore_give(player->cb_queue->sem); // 唤醒线程退出
        player->cb_thread = NULL;
        goto _err;
    }

    // 注册实例到全局链表
    if (__register_player_instance(player) != 0) {
        LISA_LOGE(TAG, "Create failed: register instance failed");
        player->cb_queue->running = false;
        lisa_semaphore_give(player->cb_queue->sem); // 唤醒线程退出
        player->cb_thread = NULL;
        goto _err;
    }

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 注册焦点通道（如果焦点管理器已初始化且配置了对应通道）
    int ret = app_player_focus_register(player, name);
    if (ret != 0 && ret != -2) {  // -2 表示没有找到配置，可以忽略
        LISA_LOGW(TAG, "Failed to register player %s to focus manager: %d", name, ret);
    }
#endif

    LISA_LOGI(TAG, "Player created successfully: %s (id=%u, handle=%p)", name, player->id, player);

    return player;

_err:
    // 统一错误处理：按相反顺序清理资源
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    if (player->focus_cb_lock) {
        PLAYER_MUTEX_DELETE(player->focus_cb_lock);
    }
#endif

    if (player->cb_queue) {
        if (player->cb_queue->sem) {
            lisa_semaphore_delete(player->cb_queue->sem);
        }
        if (player->cb_queue->lock) {
            PLAYER_MUTEX_DELETE(player->cb_queue->lock);
        }
        lisa_mem_free(player->cb_queue);
    }

    if (player->preparing_sem) {
        lisa_semaphore_delete(player->preparing_sem);
    }

    if (player->pause_sem) {
        lisa_semaphore_delete(player->pause_sem);
    }

    if (player->cb_map.lock) {
        PLAYER_MUTEX_DELETE(player->cb_map.lock);
    }

    if (player->hld) {
        lisa_player_destory(player->hld);
    }

    if (player->operation_lock) {
        PLAYER_MUTEX_DELETE(player->operation_lock);
    }

    if (player->core_lock) {
        PLAYER_MUTEX_DELETE(player->core_lock);
    }

    if (player->cancel_lock) {
        PLAYER_MUTEX_DELETE(player->cancel_lock);
    }

    if (player->name) {
        lisa_mem_free(player->name);
    }

    lisa_mem_free(player);

    return NULL;
}

/**
 * @brief   注册事件回调
 * @param   player 播放器实例
 * @param   event_cb 事件回调函数
 * @param   user_data 用户数据
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    建议在首次播放前调用
 */
int app_player_register_callback(app_player_t *player, app_player_event_cb_t event_cb, void *user_data)
{
    if (!player) {
        LISA_LOGE(TAG, "Register callback failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!event_cb) {
        LISA_LOGE(TAG, "Register callback failed: event_cb is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Registering callback for player: %s (id=%u)", player->name, player->id);

    // 加锁保护回调注册
    PLAYER_MUTEX_LOCK(player->cb_map.lock, LISA_OS_WAIT_FOREVER);

    player->cb_map.callback = event_cb;
    player->cb_map.user_data = user_data;

    PLAYER_MUTEX_UNLOCK(player->cb_map.lock);

    LISA_LOGI(TAG, "Callback registered successfully: %s (cb=%p, user_data=%p)",
              player->name, event_cb, user_data);

    return APP_PLAYER_OK;
}

// 注意：__player_prepare_check 已移到 app_player_core.c 作为 app_player_core_prepare_check

/**
 * @brief   播放音频（扩展版本，支持播放选项）
 * @param   player 播放器实例
 * @param   opt 播放选项
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    支持URL播放和流式播放模式
 */
static bool __app_player_play_opt_cancelled(const app_player_play_opt_t *opt)
{
    return opt != NULL &&
           opt->cancel_token != NULL &&
           *opt->cancel_token != opt->request_tag;
}

bool __app_player_play_cancelled_locked(const app_player_t *player)
{
    return player != NULL &&
           (player->cancel_requested ||
            (player->play_cancel_token != NULL &&
             *player->play_cancel_token != player->play_request_tag));
}

bool __app_player_play_cancelled(const app_player_t *player)
{
    bool cancelled;

    if (player == NULL) {
        return false;
    }

    PLAYER_MUTEX_LOCK(player->cancel_lock, LISA_OS_WAIT_FOREVER);
    cancelled = __app_player_play_cancelled_locked(player);
    PLAYER_MUTEX_UNLOCK(player->cancel_lock);
    return cancelled;
}

uint32_t app_player_get_callback_request_tag(app_player_t *player)
{
    return player != NULL ? player->callback_request_tag : 0U;
}

int app_player_cancel_pending(app_player_t *player)
{
    if (player == NULL) {
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PLAYER_MUTEX_LOCK(player->cancel_lock, LISA_OS_WAIT_FOREVER);
    player->cancel_requested = true;
    PLAYER_MUTEX_UNLOCK(player->cancel_lock);
    return APP_PLAYER_OK;
}

int app_player_play_ex(app_player_t *player, const app_player_play_opt_t *opt)
{
    if (!player) {
        LISA_LOGE(TAG, "Play failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!opt) {
        LISA_LOGE(TAG, "Play failed: opt is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Play ex: %s, url=%s, throw_time=%d",
              player->name,
              opt->url ? opt->url : "NULL",
              opt->throw_time_ms);

    if (!opt->url) {
        LISA_LOGE(TAG, "Play failed: url is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (__app_player_play_opt_cancelled(opt)) {
        LISA_LOGI(TAG, "Play cancelled at entry: %s tag=%u",
                  player->name, opt->request_tag);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);
    PLAYER_MUTEX_LOCK(player->cancel_lock, LISA_OS_WAIT_FOREVER);
    player->play_cancel_token = opt->cancel_token;
    player->play_request_tag = opt->request_tag;
    player->cancel_requested = false;
    bool cancelled = __app_player_play_cancelled_locked(player);
    PLAYER_MUTEX_UNLOCK(player->cancel_lock);
    if (cancelled) {
        LISA_LOGI(TAG, "Play cancelled before start: %s tag=%u",
                  player->name, opt->request_tag);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }
    /* 新一次播放开始，清除 preparing-stop 请求标志 */
    player->stop_preparing_requested = false;

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 检查当前播放状态，如果已经在播放/准备中，先停止
    PlayerState lisa_state = lisa_player_get_state(player->hld);
    if (lisa_state == PLAYER_ST_PLAYING || lisa_state == PLAYER_ST_PAUSED) {
        LISA_LOGI(TAG, "Player %s already active (state=%d), stopping before new play",
                  player->name, lisa_state);

        // 同步停止播放器（PA控制由事件回调处理）
        int stop_ret = app_player_core_stop_sync(player);
        if (stop_ret != 0) {
            LISA_LOGW(TAG, "Player %s stop before replay failed: %d, continuing anyway",
                      player->name, stop_ret);
        }
    } else if (lisa_state == PLAYER_ST_PREPARED) {
        /* PREPARED 状态：lisa_player_reset 从 PREPARED 不会真正让状态机回到 IDLE
         * （pre_close+reset 也不行），后续 core_play 的 reset 会卡死。
         *
         * 唯一可靠的排空路径是 play → stop_sync：PREPARED → PLAYING（lisa_player_play
         * 支持，老的非 stop_preparing 分支就是这么用的）；PLAYING → STOPPED
         * （stop_sync 从 PLAYING 工作正常）。
         *
         * 期间用 drain_in_progress 标志压制上层回调，避免：
         *   - PA 闪一下（app_player_upper_callback_handler 在 PLAYING 事件里开 PA）
         *   - 焦点抖动（PLAYING/STOPPED 事件会触发焦点策略）
         *   - 虚假的 PLAYER_EVT_PLAYING / STOPED 被 voice_player 当成真实 TTS 事件 */
        LISA_LOGI(TAG, "Player %s in PREPARED state (%d), drain via play+stop_sync",
                  player->name, lisa_state);
        player->drain_in_progress = true;
        if (lisa_player_play(player->hld) == PLAYER_OK) {
            PlayerErr stop_ret = lisa_player_stop_sync(player->hld);
            if (stop_ret != PLAYER_OK) {
                LISA_LOGW(TAG, "Drain stop_sync failed: %d", stop_ret);
            }
        } else {
            LISA_LOGW(TAG, "Drain play failed for %s", player->name);
        }
        player->drain_in_progress = false;
        player->is_preparing = false;
    } else if (lisa_state == PLAYER_ST_READY_TO_PLAY) {
        /* READY_TO_PLAY（准备中）状态下不动 player：__app_player_try_interrupt_prepare_stop
         * 这条路径专门处理 stop 期间 preparing，让 prepare 完成回调把状态收尾。
         * 这里跳过 stop_sync，避免 core_stop_sync 兜底入队虚假 STOPPED 事件被
         * voice_cloud_msg 当作"上一段 TTS 播完"误触发 continuous session restart。 */
        LISA_LOGI(TAG, "Player %s in preparing state (%d), skip stop_sync before new play",
                  player->name, lisa_state);
        player->is_preparing = false;
    }

    // 清除焦点暂停标志（用户主动播放）
    app_player_focus_set_paused_by_focus(player, false);

    // 申请音频焦点（如果已注册焦点通道）
    app_player_focus_acquire(player);

    // 检查焦点申请结果：如果不是前景焦点，则不继续播放
    // 焦点回调是同步执行的，所以这里可以立即检查结果
    app_player_focus_state_t focus_state = app_player_focus_get_state(player);
    if (focus_state != APP_PLAYER_FOCUS_FOREGROUND) {
        LISA_LOGI(TAG, "Play cancelled: %s not in FOREGROUND (current state: %d)",
                  player->name, focus_state);
        // 保存待播放URL，等待焦点恢复时自动播放
        if (player->pending_url) {
            lisa_mem_free(player->pending_url);
        }
        player->pending_url = lisa_mem_alloc(strlen(opt->url) + 1);
        if (player->pending_url) {
            strcpy(player->pending_url, opt->url);
            player->pending_throw_time = opt->throw_time_ms;
            LISA_LOGI(TAG, "Saved pending URL for %s: %s", player->name, opt->url);
        }
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_OK;
    }
#endif

    // 调用 core 层播放（PA控制由PLAYING事件回调处理）
    if (__app_player_play_cancelled(player)) {
        LISA_LOGI(TAG, "Play cancelled before core start: %s tag=%u",
                  player->name, opt->request_tag);
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
        app_player_focus_release(player, true);
#endif
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    int ret = app_player_core_play(player, opt->url, opt->throw_time_ms);
    if (ret != 0) {
        LISA_LOGE(TAG, "Play failed: app_player_core_play error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_IO;
    }

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 播放成功，清除待播放URL（如果有）
    if (player->pending_url) {
        lisa_mem_free(player->pending_url);
        player->pending_url = NULL;
        player->pending_throw_time = 0;
    }
#endif

    LISA_LOGI(TAG, "Play started: %s", player->name);
    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   播放音频（简化版本）
 * @param   player 播放器实例
 * @param   url 音频资源URL
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_play(app_player_t *player, const char *url)
{
    if (!player) {
        LISA_LOGE(TAG, "Play failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!url) {
        LISA_LOGE(TAG, "Play failed: url is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 构造播放选项
    app_player_play_opt_t opt = {
        .url = url,
        .throw_time_ms = 0,
    };

    return app_player_play_ex(player, &opt);
}

static int __app_player_try_interrupt_prepare_stop(app_player_t *player)
{
    PlayerState state;
    bool prepare_active;
    PlayerErr pre_close_ret;
    PlayerErr reset_ret;

    if (!player) {
        return -1;
    }

    state = lisa_player_get_state(player->hld);
    prepare_active = player->is_preparing || (state == PLAYER_ST_READY_TO_PLAY);
    if (!prepare_active) {
        return 1;
    }

    LISA_LOGI(TAG, "Stop %s while preparing, interrupt prepare first (state=%d)",
              player->name, state);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动stop，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    /* preparing 快速中断路径：不等待 prepare 完成 */
    player->prepare_error = true;
    player->pause_preparing = false;
    player->wait_prepare_intercepted = true;

    pre_close_ret = lisa_player_pre_close(player->hld);
    if (pre_close_ret < 0) {
        LISA_LOGW(TAG, "Interrupt prepare stop pre_close failed for %s: %d",
                  player->name, pre_close_ret);
    }

    reset_ret = lisa_player_reset(player->hld);
    if (reset_ret != PLAYER_OK) {
        LISA_LOGW(TAG, "Interrupt prepare stop reset failed for %s: %d",
                  player->name, reset_ret);
    }

    player->is_preparing = false;
    player->wait_prepare_intercepted = false;
    lisa_semaphore_give(player->preparing_sem);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // prepare 被中断后，主动释放焦点
    app_player_focus_release(player, true);
#endif

    return 0;
}

/**
 * @brief   停止播放（同步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    同步停止，等待停止完成后返回
 */
int app_player_stop(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Stop failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 流式播放模式下不支持 stop 操作
    if (player->is_stream_mode) {
        LISA_LOGE(TAG, "Stop failed: not supported in stream mode");
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    LISA_LOGI(TAG, "Stop: %s", player->name);
    (void)app_player_cancel_pending(player);
    /* stop 优先，抑制 preparing 完成后的自动播放 */
    player->stop_preparing_requested = true;
    
    // 优先处理中断prepare场景，避免被 operation_lock 长时间阻塞导致 stop 失效
    if (__app_player_try_interrupt_prepare_stop(player) == 0) {
        return APP_PLAYER_OK;
    }
    
    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动stop，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    // 调用 core 层同步停止（纯播放控制）
    int ret = app_player_core_stop_sync(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "%s Stop failed: app_player_core_stop_sync error %d", player->name, ret);
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
        // 失败时清除标志，避免残留
        app_player_focus_set_user_initiated(player, false);
#endif
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 用户主动停止，释放焦点（在锁外执行）
    app_player_focus_release(player, true);
#endif

    // PA控制由STOPPED事件回调处理

    return APP_PLAYER_OK;
}

/**
 * @brief   暂停播放（同步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    同步接口，等待暂停完成后返回
 * @note    如果正在准备阶段，则设置pause_preparing标志，准备完成后不会自动播放
 */
int app_player_pause(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Pause failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 流式播放模式下不支持 pause 操作
    if (player->is_stream_mode) {
        LISA_LOGE(TAG, "Pause failed: not supported in stream mode");
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    // 检查当前状态
    PlayerState state = lisa_player_get_state(player->hld);
    if (state == PLAYER_ST_PAUSED) {
        LISA_LOGI(TAG, "Pause: %s already paused", player->name);
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
        // 即使已经暂停，也要标记用户主动操作，避免焦点恢复时自动播放
        app_player_focus_set_user_initiated(player, true);
        // 用户主动暂停，释放焦点
        app_player_focus_release(player, true);
#endif
        return APP_PLAYER_OK;
    }

    LISA_LOGI(TAG, "Pause: %s", player->name);

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动pause，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    // 调用 core 层暂停（PA控制由PAUSED事件回调处理）
    int ret = app_player_core_pause(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "Pause failed: app_player_core_pause error %d", ret);
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
        // 失败时清除标志，避免残留
        app_player_focus_set_user_initiated(player, false);
#endif
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 用户主动暂停，释放焦点（在锁外执行）
    app_player_focus_release(player, true);
#endif

    return APP_PLAYER_OK;
}

/**
 * @brief   恢复播放（同步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    同步接口，等待恢复完成后返回
 * @note    如果在准备阶段被暂停，则清除pause_preparing标志并开始播放
 */
int app_player_resume(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Resume failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 流式播放模式下不支持 resume 操作
    if (player->is_stream_mode) {
        LISA_LOGE(TAG, "Resume failed: not supported in stream mode");
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    LISA_LOGI(TAG, "Resume: %s", player->name);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 用户主动恢复，申请焦点
    app_player_focus_acquire(player);

    // 检查焦点申请结果：如果不是前景焦点，则不继续恢复
    app_player_focus_state_t focus_state = app_player_focus_get_state(player);
    if (focus_state != APP_PLAYER_FOCUS_FOREGROUND) {
        LISA_LOGI(TAG, "Resume cancelled: %s not in FOREGROUND (current state: %d)",
                  player->name, focus_state);
        // 设置 paused_by_focus 标志，等待焦点恢复时自动播放
        app_player_focus_set_paused_by_focus(player, true);
        return APP_PLAYER_OK;
    }

    // 获得焦点，清除标志
    app_player_focus_set_paused_by_focus(player, false);
#endif

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 调用 core 层接口，由 core 层统一处理 pause_preparing 逻辑和 lisa_player 调用
    // PA控制由PLAYING事件回调处理
    int ret = app_player_core_resume_sync(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "Resume failed: app_player_core_resume_sync error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   重置播放器到初始状态（同步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    同步接口，等待重置完成后返回
 * @note    重置后播放器回到IDLE状态，清除所有播放相关的标志和状态
 */
int app_player_reset(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Reset failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    LISA_LOGI(TAG, "Reset: %s", player->name);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动reset，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    // 清除准备中标志
    player->is_preparing = false;
    player->stop_preparing_requested = false;
    player->wait_prepare_intercepted = false;
    player->pause_preparing = false;

    // 调用底层reset
    PlayerErr ret = lisa_player_reset(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Reset failed: lisa_player_reset error %d", ret);
#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
        // 失败时清除标志，避免残留
        app_player_focus_set_user_initiated(player, false);
#endif
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    // 更新状态为IDLE
    player->state = APP_PLAYER_STATE_IDLE;
    player->last_evt = PLAYER_EVT_INIT;

    LISA_LOGI(TAG, "Player %s reset to IDLE state", player->name);

    PLAYER_MUTEX_UNLOCK(player->operation_lock);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 用户主动reset，释放焦点（在锁外执行）
    app_player_focus_release(player, true);
#endif

    return APP_PLAYER_OK;
}

int app_player_set_decode_prepared_count(app_player_t *player, uint32_t count)
{
    if (!player || count == 0) {
        LISA_LOGE(TAG, "Set prepared count failed: invalid param");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PlayerErr ret = lisa_player_set_decode_prepared_count(player->hld, count);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Set prepared count failed: %d", ret);
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Set prepared count: %s, count=%u", player->name, count);
    return APP_PLAYER_OK;
}

int app_player_set_readstream_buf_size(app_player_t *player, uint32_t size)
{
    if (!player) {
        LISA_LOGE(TAG, "Set readstream buf size failed: invalid param");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PlayerErr ret = lisa_player_set_readstream_buf_size(player->hld, size);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Set readstream buf size failed: %d", ret);
        if (ret == PLAYER_INVALID_STATE) {
            return APP_PLAYER_ERR_INVALID_STATE;
        }
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Set readstream buf size: %s, size=%u", player->name, size);
    return APP_PLAYER_OK;
}

/**
 * @brief   跳转到指定位置
 * @param   player 播放器实例
 * @param   seek_ms 跳转位置（毫秒）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    异步操作，跳转完成后会收到 SEEK_COMPLETE 事件
 */
int app_player_seek(app_player_t *player, uint32_t seek_ms)
{
    if (!player) {
        LISA_LOGE(TAG, "Seek failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 流式播放模式下不支持 seek 操作
    if (player->is_stream_mode) {
        LISA_LOGE(TAG, "Seek failed: not supported in stream mode");
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    LISA_LOGI(TAG, "Seek: %s to %u ms", player->name, seek_ms);

    // 调用底层seek
    PlayerErr ret = lisa_player_seek(player->hld, seek_ms);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Seek failed: lisa_player_seek error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   获取播放器状态
 * @param   player 播放器实例
 * @return  播放器状态
 */
app_player_state_t app_player_get_state(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Get state failed: player is NULL");
        return APP_PLAYER_STATE_ERROR;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 获取底层播放器状态
    PlayerState state = lisa_player_get_state(player->hld);

    // 转换为app_player状态
    app_player_state_t app_state;
    switch (state) {
        case PLAYER_ST_NONE:
            app_state = APP_PLAYER_STATE_IDLE;
            break;
        case PLAYER_ST_READY_TO_PLAY:
            app_state = APP_PLAYER_STATE_PREPARING;
            break;
        case PLAYER_ST_PREPARED:
            app_state = APP_PLAYER_STATE_PREPARED;
            break;
        case PLAYER_ST_PLAYING:
            app_state = APP_PLAYER_STATE_PLAYING;
            break;
        case PLAYER_ST_PAUSED:
            app_state = APP_PLAYER_STATE_PAUSED;
            break;
        case PLAYER_ST_STOPED:
            app_state = APP_PLAYER_STATE_STOPPED;
            break;
        case PLAYER_ST_PLAYBACK_COMPLETE:
            app_state = APP_PLAYER_STATE_STOPPED;
            break;
        case PLAYER_ST_ERROR:
        default:
            app_state = APP_PLAYER_STATE_ERROR;
            break;
    }

    // 更新内部状态缓存
    player->state = app_state;

    PLAYER_MUTEX_UNLOCK(player->operation_lock);

    return app_state;
}

/**
 * @brief   获取当前播放位置
 * @param   player 播放器实例
 * @param   position 输出参数，当前位置（毫秒）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_get_position(app_player_t *player, uint32_t *position)
{
    if (!player) {
        LISA_LOGE(TAG, "Get position failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!position) {
        LISA_LOGE(TAG, "Get position failed: position is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 获取底层播放位置
    int32_t pos = lisa_player_get_pos(player->hld);
    if (pos < 0) {
        LISA_LOGE(TAG, "Get position failed: lisa_player_get_pos error %d", pos);
        *position = 0;
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    *position = (uint32_t)pos;

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   获取总时长
 * @param   player 播放器实例
 * @param   duration 输出参数，总时长（毫秒）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_get_duration(app_player_t *player, uint32_t *duration)
{
    if (!player) {
        LISA_LOGE(TAG, "Get duration failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!duration) {
        LISA_LOGE(TAG, "Get duration failed: duration is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 获取底层播放时长
    int32_t dur = lisa_player_get_duration(player->hld);
    if (dur < 0) {
        LISA_LOGE(TAG, "Get duration failed: lisa_player_get_duration error %d", dur);
        *duration = 0;
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    *duration = (uint32_t)dur;

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   设置音量
 * @param   player 播放器实例
 * @param   volume 音量值（1-100）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    音量会根据播放器的音量范围进行映射转换
 */
int app_player_set_volume(app_player_t *player, uint8_t volume)
{
    if (!player) {
        LISA_LOGE(TAG, "Set volume failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 检查音量范围
    if (volume < APP_PLAYER_VOL_MIN || volume > APP_PLAYER_VOL_MAX) {
        LISA_LOGE(TAG, "Set volume failed: volume %d out of range [%d, %d]",
                  volume, APP_PLAYER_VOL_MIN, APP_PLAYER_VOL_MAX);
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Set volume: %d", volume);

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 调用底层设置音量
    PlayerErr ret = lisa_player_set_vol(player->hld, volume);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Set volume failed: lisa_player_set_vol error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   开始流式播放
 * @param   player 播放器实例
 * @param   sample_rate 采样率（Hz，如 8000, 16000, 48000）
 * @param   channels 声道数（1=单声道, 2=立体声）
 * @param   bits 位深度（8, 16, 24, 32）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    调用此函数后，使用 app_player_write_stream 写入 PCM 数据
 */
int app_player_play_stream(app_player_t *player, uint32_t sample_rate, uint8_t channels, uint8_t bits)
{
    if (!player) {
        LISA_LOGE(TAG, "Play stream failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 参数验证
    if (sample_rate == 0) {
        LISA_LOGE(TAG, "Play stream failed: invalid sample rate 0");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (channels != 1) {
        LISA_LOGE(TAG, "Play stream failed: only mono channel supported (channels=%u)", channels);
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    if (bits != 16) {
        LISA_LOGE(TAG, "Play stream failed: only 16-bit supported (bits=%u)", bits);
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    LISA_LOGI(TAG, "Play stream: %s, rate=%u, ch=%u, bits=%u",
              player->name, sample_rate, channels, bits);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 清除焦点暂停标志（用户主动播放）
    app_player_focus_set_paused_by_focus(player, false);

    // 申请音频焦点（如果已注册焦点通道）
    app_player_focus_acquire(player);

    // 检查焦点申请结果：如果不是前景焦点，则不继续播放
    app_player_focus_state_t focus_state = app_player_focus_get_state(player);
    if (focus_state != APP_PLAYER_FOCUS_FOREGROUND) {
        LISA_LOGW(TAG, "Play stream cancelled: %s not in FOREGROUND (current state: %d)",
                  player->name, focus_state);
        /* 如实返回错误：调用方需要区分"被焦点拒绝"与"成功"来决策重试 */
        return APP_PLAYER_ERR_INVALID_STATE;
    }
#endif

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 调用 core 层流式播放（PA控制由PLAYING事件回调处理）
    int ret = app_player_core_play_stream(player, sample_rate, channels, bits);
    if (ret != 0) {
        LISA_LOGE(TAG, "Play stream failed: app_player_core_play_stream error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_IO;
    }

    LISA_LOGI(TAG, "Stream playback started: %s", player->name);
    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   写入流数据
 * @param   player 播放器实例
 * @param   data 数据指针
 * @param   size 数据大小（字节）
 * @param   timeout_ms 超时时间（毫秒）
 * @return  实际写入的字节数，<0 表示错误
 * @note    必须先调用 app_player_play_stream 开启流式播放
 */
int app_player_write_stream(app_player_t *player, const uint8_t *data, uint32_t size, uint32_t timeout_ms)
{
    if (!player) {
        LISA_LOGE(TAG, "Write stream failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    if (!data && size > 0) {
        LISA_LOGE(TAG, "Write stream failed: data is NULL but size > 0");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Write stream: %s, size=%u, timeout=%u", player->name, size, timeout_ms);

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 调用 core 层写入流数据（纯播放控制）
    int ret = app_player_core_stream_write(player, data, size, timeout_ms);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write stream failed: app_player_core_stream_write error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_IO;
    }

    LISA_LOGI(TAG, "Stream data written: %d bytes", ret);
    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return ret;
}

/**
 * @brief   结束流式播放
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    通知播放器所有流数据已写入完毕，等待播放完成
 */
int app_player_finish_stream(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Finish stream failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 检查是否处于流式播放模式
    if (!player->is_stream_mode) {
        LISA_LOGE(TAG, "Finish stream failed: not in stream mode");
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    LISA_LOGI(TAG, "Finish stream: %s", player->name);

    // 发送结束信号（data=NULL, size=0）
    int ret = lisa_player_put_stream_data(player->hld, NULL, 0, 0);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish stream failed: lisa_player_put_stream_data error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_IO;
    }

    // 清除流式模式标志
    player->is_stream_mode = false;

    LISA_LOGI(TAG, "Stream playback finished: %s", player->name);
    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
/**
 * @brief   注册播放器焦点变化回调
 * @param   player 播放器实例
 * @param   on_focus_change 焦点状态变化回调函数（可为 NULL 清除回调）
 * @param   user_data 用户自定义数据（传递给回调）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_register_focus_cb(app_player_t *player,
                                  app_player_focus_change_cb_t on_focus_change,
                                  void *user_data)
{
    if (!player) {
        LISA_LOGE(TAG, "Register focus callback failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 检查播放器是否已注册焦点通道
    if (player->focus_channel_id < 0) {
        LISA_LOGW(TAG, "Register focus callback failed: player %s not registered to focus manager",
                  player->name);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    LISA_LOGI(TAG, "Registering focus callback for player: %s (id=%u)", player->name, player->id);

    // 加锁保护焦点回调注册
    PLAYER_MUTEX_LOCK(player->focus_cb_lock, LISA_OS_WAIT_FOREVER);
    player->focus_cb = on_focus_change;
    player->focus_user_data = user_data;
    PLAYER_MUTEX_UNLOCK(player->focus_cb_lock);

    LISA_LOGI(TAG, "Focus callback registered successfully: %s (cb=%p, user_data=%p)",
              player->name, on_focus_change, user_data);

    return APP_PLAYER_OK;
}
#endif

/**
 * @brief   销毁播放器实例
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_destroy(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Destroy failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Destroying player: %s (id=%u)", player->name, player->id);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 注销焦点通道
    app_player_focus_unregister(player);
#endif

    // 从全局链表注销实例
    __unregister_player_instance(player);

    // 停止回调线程
    if (player->cb_queue) {
        player->cb_queue->running = false;
        lisa_semaphore_give(player->cb_queue->sem); // 唤醒线程退出
        player->cb_thread = NULL;
    }

    // 清空并销毁回调队列
    if (player->cb_queue) {
        // 清空队列中的所有事件
        PLAYER_MUTEX_LOCK(player->cb_queue->lock, LISA_OS_WAIT_FOREVER);
        callback_event_t *evt = player->cb_queue->head;
        while (evt) {
            callback_event_t *next = evt->next;
            lisa_mem_free(evt);
            evt = next;
        }
        player->cb_queue->head = NULL;
        player->cb_queue->tail = NULL;
        PLAYER_MUTEX_UNLOCK(player->cb_queue->lock);

        // 销毁队列资源
        if (player->cb_queue->sem) {
            lisa_semaphore_delete(player->cb_queue->sem);
        }
        if (player->cb_queue->lock) {
            PLAYER_MUTEX_DELETE(player->cb_queue->lock);
        }
        lisa_mem_free(player->cb_queue);
        player->cb_queue = NULL;
    }

    // 销毁底层播放器
    if (player->hld) {
        lisa_player_destory(player->hld);
        player->hld = NULL;
    }

    // 销毁准备信号量
    if (player->preparing_sem) {
        lisa_semaphore_delete(player->preparing_sem);
        player->preparing_sem = NULL;
    }

    // 销毁暂停信号量
    if (player->pause_sem) {
        lisa_semaphore_delete(player->pause_sem);
        player->pause_sem = NULL;
    }

    // 销毁回调管理锁
    if (player->cb_map.lock) {
        PLAYER_MUTEX_DELETE(player->cb_map.lock);
        player->cb_map.lock = NULL;
    }

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 释放待播放URL
    if (player->pending_url) {
        lisa_mem_free(player->pending_url);
        player->pending_url = NULL;
    }

    // 销毁焦点回调锁
    if (player->focus_cb_lock) {
        PLAYER_MUTEX_DELETE(player->focus_cb_lock);
        player->focus_cb_lock = NULL;
    }
#endif

    // 销毁实例操作互斥锁
    if (player->operation_lock) {
        PLAYER_MUTEX_DELETE(player->operation_lock);
        player->operation_lock = NULL;
    }

    // 销毁核心层操作互斥锁
    if (player->core_lock) {
        PLAYER_MUTEX_DELETE(player->core_lock);
        player->core_lock = NULL;
    }

    if (player->cancel_lock) {
        PLAYER_MUTEX_DELETE(player->cancel_lock);
        player->cancel_lock = NULL;
    }

    // 释放名称内存
    if (player->name) {
        LISA_LOGI(TAG, "Player destroyed: %s (id=%u)", player->name, player->id);
        lisa_mem_free(player->name);
        player->name = NULL;
    }

    // 释放播放器结构体
    lisa_mem_free(player);

    return APP_PLAYER_OK;
}
