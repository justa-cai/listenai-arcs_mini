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

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
#include "app_player_focus.h"
#include "listen_audiomgr.h"
#endif

#define TAG "APP_PLAYER"

#define APP_PLAYER_VOL_MIN (1)
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

    LISA_LOGD(TAG, "Instance registered: id=%u", player->id);
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
            LISA_LOGD(TAG, "Instance unregistered: id=%u", player->id);
            break;
        }
        curr = &((*curr)->next);
    }

    PLAYER_MUTEX_UNLOCK(s_player_list_lock);
}

/**
 * @brief 根据ID查找播放器实例
 * @param id 播放器ID
 * @return 播放器实例指针，未找到返回NULL
 */
static app_player_t *__find_player_by_id(uint32_t id)
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
 * @brief lisa_player回调处理函数（中间层，快速返回）
 * @param evt lisa_player事件
 * @param arg1 参数1
 * @param arg2 参数2（保留）
 * @param id 播放器ID
 * @return 0成功，负数失败
 *
 * @note 关键逻辑：
 * 1. PREPARED事件：根据标志决定是否自动播放
 * 2. 播放结束事件：关闭PA
 * 3. ERROR事件：自动reset播放器
 * 4. 中断标志处理：唤醒等待的preparing信号量
 */
static int __lisa_player_callback_handler(PlayerEvt evt, int arg1, int arg2, int id)
{
    // 通过ID查找实例
    app_player_t *player = __find_player_by_id(id);
    if (!player) {
        LISA_LOGE(TAG, "Callback: player not found for id=%d", id);
        return -1;
    }

    LISA_LOGI(TAG, "Player[%d] %s evt=%d", id, player->name, evt);

    // 标志：是否忽略自动播放
    bool ignore_play = false;

    // 清除preparing标志
    player->is_preparing = false;

    // 如果正在等待中断，唤醒信号量
    if (player->wait_prepare_intercepted) {
        ignore_play = true;
        lisa_semaphore_give(player->preparing_sem);
    }

    // 特殊事件处理（同步执行，快速返回）
    switch (evt) {
        case PLAYER_EVT_PREPARED: {
            if (!ignore_play) {
                if (!player->pause_preparing) {
                    // 自动开始播放
                    if (lisa_player_play(player->hld) == PLAYER_OP_FAIL) {
                        LISA_LOGE(TAG, "Auto play failed: %s", player->name);
                        // 播放失败，通知错误事件
                        __enqueue_callback_event(player, APP_PLAYER_EVENT_ERROR);
                        return 0;
                    }
                } else {
                    // 准备时被暂停，直接通知暂停事件
                    LISA_LOGD(TAG, "Player %s has pause when preparing", player->name);
                    __enqueue_callback_event(player, APP_PLAYER_EVENT_PAUSED);
                    return 0;
                }
            }
            // PREPARED事件不通知用户（因为会自动触发PLAYING）
            return 0;
        }

        case PLAYER_EVT_PAUSED:
        case PLAYER_EVT_STOPED:
        case PLAYER_EVT_PLAYBACK_COMPLETE:
        case PLAYER_EVT_ERROR: {
            // 特殊情况：Stop后立即来Error，不需要再关闭PA
            if (player->last_evt == PLAYER_EVT_STOPED && evt == PLAYER_EVT_ERROR) {
                LISA_LOGD(TAG, "Player %s: skip PA off after stop+error", player->name);
            } else {
                // 播放结束，延迟关闭PA
                pa_manager_control(0, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);
            }

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
            // 播放完成、停止、错误或用户主动暂停时释放焦点
            // 注意：被焦点管理器暂停时(user_initiated_stop=false)不释放焦点，以便后续自动恢复
            bool user_initiated = app_player_focus_is_user_initiated(player);
            if (evt == PLAYER_EVT_STOPED || evt == PLAYER_EVT_PLAYBACK_COMPLETE ||
                evt == PLAYER_EVT_ERROR || (evt == PLAYER_EVT_PAUSED && user_initiated)) {
                LISA_LOGD(TAG, "Releasing audio focus for player %s (evt=%d, user_initiated=%d)",
                         player->name, evt, user_initiated);
                bool is_user_initiated = user_initiated;
                app_player_focus_release(player, is_user_initiated);
                // 释放后清除用户主动标志
                app_player_focus_set_user_initiated(player, false);
            }
#endif

            // ERROR事件需要自动reset
            if (evt == PLAYER_EVT_ERROR) {
                LISA_LOGW(TAG, "Player %s error, auto reset", player->name);
                lisa_player_reset(player->hld);
            }

            // fall through，继续处理
            break;
        }

        case PLAYER_EVT_PLAYING: {
            // 播放中，确保PA开启
            pa_manager_control(1, 0);
            break;
        }

        default:
            break;
    }

    // 更新内部状态
    player->last_evt = evt;

    // 转换事件并加入队列（异步通知用户）
    app_player_event_t app_evt = __convert_player_evt_to_app_event(evt);
    __enqueue_callback_event(player, app_evt);

    return 0;
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

    // 创建准备信号量
    player->preparing_sem = lisa_semaphore_create(1);
    if (!player->preparing_sem) {
        LISA_LOGE(TAG, "Create failed: semaphore create failed");
        goto _err;
    }

    // 初始化状态
    player->state = APP_PLAYER_STATE_IDLE;
    player->last_evt = PLAYER_EVT_INIT;
    player->is_preparing = false;
    player->wait_prepare_intercepted = false;
    player->pause_preparing = false;
    player->is_stream_mode = false;

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

    // 注册lisa_player回调
    if (lisa_player_set_callback(player->hld, __lisa_player_callback_handler) != PLAYER_OK) {
        LISA_LOGE(TAG, "Create failed: set lisa_player callback failed");
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

    LISA_LOGD(TAG, "Callback registered successfully: %s (cb=%p, user_data=%p)",
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

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 清除焦点暂停标志（用户主动播放）
    app_player_focus_set_paused_by_focus(player, false);

    // 申请音频焦点（如果已注册焦点通道）
    app_player_focus_acquire(player);
#endif

    // 开启 PA
    pa_manager_control(1, 0);

    // 调用 core 层播放（纯播放控制）
    int ret = app_player_core_play(player, opt->url, opt->throw_time_ms);
    if (ret != 0) {
        LISA_LOGE(TAG, "Play failed: app_player_core_play error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_IO;
    }

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

/**
 * @brief   停止播放（异步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    异步停止，立即返回，停止完成后会收到STOPPED事件
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

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动stop，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    // 关闭PA
    pa_manager_control(0, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);

    // 调用 core 层停止（纯播放控制）
    int ret = app_player_core_stop(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "Stop failed: app_player_core_stop error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 用户主动停止，释放焦点（在锁外执行）
    app_player_focus_release(player, true);
#endif

    return APP_PLAYER_OK;
}

/**
 * @brief   停止播放（同步，等待停止完成）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    同步停止，等待停止完成后返回
 */
int app_player_stop_sync(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Stop sync failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 流式播放模式下不支持 stop 操作
    if (player->is_stream_mode) {
        LISA_LOGE(TAG, "Stop sync failed: not supported in stream mode");
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    LISA_LOGI(TAG, "Stop sync: %s", player->name);

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动stop，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    // 关闭PA
    pa_manager_control(0, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);

    // 调用 core 层同步停止（纯播放控制）
    int ret = app_player_core_stop_sync(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "%s Stop sync failed: app_player_core_stop_sync error %d", player->name, ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 用户主动停止，释放焦点（在锁外执行）
    app_player_focus_release(player, true);
#endif

    return APP_PLAYER_OK;
}

/**
 * @brief   暂停播放
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
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

    // 检查当前状态，如果已经是PAUSED，无需重复pause
    PlayerState state = lisa_player_get_state(player->hld);
    if (state == PLAYER_ST_PAUSED) {
        LISA_LOGI(TAG, "Pause: %s already paused, skip", player->name);
        return APP_PLAYER_OK;
    }

    LISA_LOGI(TAG, "Pause: %s", player->name);

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 标记为用户主动pause，后续焦点释放时跳过策略执行
    app_player_focus_set_user_initiated(player, true);
#endif

    // 关闭PA
    pa_manager_control(0, CONFIG_APP_PLAYER_PA_OFF_DELAY_MS);

    // 调用 core 层暂停（纯播放控制）
    int ret = app_player_core_pause(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "Pause failed: app_player_core_pause error %d", ret);
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
 * @brief   恢复播放（异步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
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
    app_player_focus_set_paused_by_focus(player, false);
#endif

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 开启PA
    pa_manager_control(1, 0);

    // 调用 core 层恢复（纯播放控制）
    int ret = app_player_core_resume(player);
    if (ret != 0) {
        LISA_LOGE(TAG, "Resume failed: app_player_core_resume error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   恢复播放（同步，等待恢复完成）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    同步版本，等待恢复完成后返回
 */
int app_player_resume_sync(app_player_t *player)
{
    if (!player) {
        LISA_LOGE(TAG, "Resume sync failed: player is NULL");
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    // 流式播放模式下不支持 resume 操作
    if (player->is_stream_mode) {
        LISA_LOGE(TAG, "Resume sync failed: not supported in stream mode");
        return APP_PLAYER_ERR_NOT_SUPPORTED;
    }

    LISA_LOGI(TAG, "Resume sync: %s", player->name);

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    // 清除焦点暂停标志（用户主动恢复）
    player->paused_by_focus = false;
#endif

    // 开启PA
    pa_manager_control(1, 0);

    // 如果是在准备阶段被暂停的，清除pause_preparing标志并开始播放
    if (player->pause_preparing) {
        LISA_LOGD(TAG, "Player %s was paused during preparing, start playing", player->name);
        player->pause_preparing = false;
        PlayerErr ret = lisa_player_play(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Resume sync failed: lisa_player_play error %d", ret);
            PLAYER_MUTEX_UNLOCK(player->operation_lock);
            return APP_PLAYER_ERR_INVALID_STATE;
        }
    } else {
        // 否则调用底层的同步恢复接口
        PlayerErr ret = lisa_player_resume_sync(player->hld);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Resume sync failed: lisa_player_resume_sync error %d", ret);
            PLAYER_MUTEX_UNLOCK(player->operation_lock);
            return APP_PLAYER_ERR_INVALID_STATE;
        }
    }

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
    return APP_PLAYER_OK;
}

/**
 * @brief   重置播放器到初始状态
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
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

    // 清除准备中标志
    player->is_preparing = false;
    player->wait_prepare_intercepted = false;
    player->pause_preparing = false;

    // 调用底层reset
    PlayerErr ret = lisa_player_reset(player->hld);
    if (ret != PLAYER_OK) {
        LISA_LOGE(TAG, "Reset failed: lisa_player_reset error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_INVALID_STATE;
    }

    // 更新状态为IDLE
    player->state = APP_PLAYER_STATE_IDLE;
    player->last_evt = PLAYER_EVT_INIT;

    LISA_LOGD(TAG, "Player %s reset to IDLE state", player->name);

    PLAYER_MUTEX_UNLOCK(player->operation_lock);
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
#endif

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 开启PA
    pa_manager_control(1, 0);

    // 调用 core 层流式播放（纯播放控制）
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

    PLAYER_MUTEX_LOCK(player->operation_lock, LISA_OS_WAIT_FOREVER);

    // 调用 core 层写入流数据（纯播放控制）
    int ret = app_player_core_stream_write(player, data, size, timeout_ms);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write stream failed: app_player_core_stream_write error %d", ret);
        PLAYER_MUTEX_UNLOCK(player->operation_lock);
        return APP_PLAYER_ERR_IO;
    }

    LISA_LOGD(TAG, "Stream data written: %d bytes", ret);
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

    LISA_LOGD(TAG, "Focus callback registered successfully: %s (cb=%p, user_data=%p)",
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

    // 销毁回调管理锁
    if (player->cb_map.lock) {
        PLAYER_MUTEX_DELETE(player->cb_map.lock);
        player->cb_map.lock = NULL;
    }

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
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
