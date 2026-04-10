/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "PA_MGR"

#include "pa_manager.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_timer.h"

typedef struct {
    pa_ctrl_callback_t ctrl_callback;  /* PA控制回调函数 */
    lisa_timer_t *timer;               /* 延时定时器 */
    lisa_mutex_t *mutex;               /* 互斥锁 */
    int current_state;                 /* 当前PA状态: 1=ON, 0=OFF */
    int pending_state;                 /* 待执行的PA状态 */
    int ref_count;                     /* 引用计数: 记录有多少实例正在使用PA */
} pa_manager_t;

static pa_manager_t *s_pa_mgr = NULL;

/**
 * @brief 定时器超时回调函数
 * @note 定时器回调执行时，定时器已停止，不会与pa_manager_control并发
 */
static void pa_timer_callback(lisa_timer_t *timer)
{
    if (!s_pa_mgr || !timer) {
        return;
    }

    /* 执行待执行的PA状态 */
    if (s_pa_mgr->ctrl_callback) {
        int ret = s_pa_mgr->ctrl_callback(s_pa_mgr->pending_state);
        if (ret == 0) {
            s_pa_mgr->current_state = s_pa_mgr->pending_state;
            LISA_LOGI(TAG, "PA %s (delayed)", s_pa_mgr->current_state ? "ON" : "OFF");
        } else {
            LISA_LOGE(TAG, "PA control failed: %d", ret);
        }
    }
}

/**
 * @brief 初始化 PA 管理器
 */
int pa_manager_init(const pa_manager_config_t *config)
{
    if (!config || !config->ctrl_callback) {
        LISA_LOGE(TAG, "Invalid config or callback is NULL");
        return -1;
    }

    if (s_pa_mgr) {
        LISA_LOGW(TAG, "PA manager already initialized");
        return 0;
    }

    /* 分配内存 */
    s_pa_mgr = (pa_manager_t *)lisa_mem_alloc(sizeof(pa_manager_t));
    if (!s_pa_mgr) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        return -2;
    }

    /* 初始化配置 */
    s_pa_mgr->ctrl_callback = config->ctrl_callback;
    s_pa_mgr->current_state = 0;  /* 初始状态为关闭 */
    s_pa_mgr->pending_state = 0;
    s_pa_mgr->ref_count = 0;      /* 初始引用计数为0 */

    /* 创建互斥锁 */
    s_pa_mgr->mutex = lisa_mutex_create();
    if (!s_pa_mgr->mutex) {
        LISA_LOGE(TAG, "Failed to create mutex");
        lisa_mem_free(s_pa_mgr);
        s_pa_mgr = NULL;
        return -3;
    }

    /* 创建定时器（使用默认周期1000ms，实际使用时会动态修改） */
    s_pa_mgr->timer = lisa_timer_create(1000, pa_timer_callback, s_pa_mgr);
    if (!s_pa_mgr->timer) {
        LISA_LOGE(TAG, "Failed to create timer");
        lisa_mutex_delete(s_pa_mgr->mutex);
        lisa_mem_free(s_pa_mgr);
        s_pa_mgr = NULL;
        return -4;
    }

    LISA_LOGI(TAG, "PA manager initialized successfully");
    return 0;
}

/**
 * @brief 控制 PA 开关（带引用计数）
 * @param onoff 1=开启, 0=关闭
 * @param delay_ms 延时时间（毫秒），0表示立即执行
 * @return 0成功，其他失败
 *
 * @note 引用计数机制：
 * - onoff=1 时增加引用计数，只在首次开启时真正打开PA
 * - onoff=0 时减少引用计数，只在计数为0时真正关闭PA
 * - 这样可以支持多个播放器实例同时使用PA，避免互相干扰
 */
int pa_manager_control(int onoff, uint32_t delay_ms)
{
    if (!s_pa_mgr) {
        LISA_LOGE(TAG, "PA manager not initialized");
        return -1;
    }

    lisa_mutex_lock(s_pa_mgr->mutex, LISA_OS_WAIT_FOREVER);

    if (onoff) {
        /* === PA开启：增加引用计数 === */
        s_pa_mgr->ref_count++;

        LISA_LOGI(TAG, "PA ON request (ref_count: %d->%d, caller:0x%x)",
                  s_pa_mgr->ref_count - 1, s_pa_mgr->ref_count, __builtin_return_address(0));

        /* 停止之前的关闭定时器（如果有） */
        lisa_timer_stop(s_pa_mgr->timer);

        /* 只在首次开启时真正打开PA */
        if (s_pa_mgr->ref_count == 1 && s_pa_mgr->current_state != 1) {
            if (s_pa_mgr->ctrl_callback) {
                int ret = s_pa_mgr->ctrl_callback(1);
                if (ret == 0) {
                    s_pa_mgr->current_state = 1;
                    LISA_LOGI(TAG, "PA turned ON (first instance)");
                } else {
                    LISA_LOGE(TAG, "PA control failed: %d", ret);
                    s_pa_mgr->ref_count--;  /* 失败时回退引用计数 */
                    lisa_mutex_unlock(s_pa_mgr->mutex);
                    return ret;
                }
            }
        } else {
            LISA_LOGD(TAG, "PA already ON, skip hardware control");
        }
    } else {
        /* === PA关闭：减少引用计数 === */
        if (s_pa_mgr->ref_count > 0) {
            s_pa_mgr->ref_count--;
        } else {
            LISA_LOGW(TAG, "PA OFF request but ref_count already 0 (caller:0x%x)",
                      __builtin_return_address(0));
        }

        LISA_LOGI(TAG, "PA OFF request (ref_count: %d->%d, caller:0x%x)",
                  s_pa_mgr->ref_count + 1, s_pa_mgr->ref_count, __builtin_return_address(0));

        /* 只有当引用计数为0时才真正关闭PA */
        if (s_pa_mgr->ref_count == 0) {
            /* 停止之前的定时器 */
            lisa_timer_stop(s_pa_mgr->timer);

            if (delay_ms == 0) {
                /* 立即关闭 */
                if (s_pa_mgr->ctrl_callback && s_pa_mgr->current_state != 0) {
                    int ret = s_pa_mgr->ctrl_callback(0);
                    if (ret == 0) {
                        s_pa_mgr->current_state = 0;
                        LISA_LOGI(TAG, "PA turned OFF (last instance, immediate)");
                    } else {
                        LISA_LOGE(TAG, "PA control failed: %d", ret);
                        lisa_mutex_unlock(s_pa_mgr->mutex);
                        return ret;
                    }
                }
            } else {
                /* 延时关闭 */
                s_pa_mgr->pending_state = 0;
                lisa_timer_change_period(s_pa_mgr->timer, delay_ms);
                lisa_timer_start(s_pa_mgr->timer);
                LISA_LOGI(TAG, "PA will turn OFF after %u ms (last instance)", delay_ms);
            }
        } else {
            LISA_LOGD(TAG, "PA still used by %d instance(s), skip hardware control",
                      s_pa_mgr->ref_count);
        }
    }

    lisa_mutex_unlock(s_pa_mgr->mutex);
    return 0;
}

/**
 * @brief 获取当前 PA 状态
 */
int pa_manager_get_state(void)
{
    if (!s_pa_mgr) {
        return 0;
    }

    int state;
    lisa_mutex_lock(s_pa_mgr->mutex, LISA_OS_WAIT_FOREVER);
    state = s_pa_mgr->current_state;
    lisa_mutex_unlock(s_pa_mgr->mutex);

    return state;
}

/**
 * @brief 获取当前 PA 引用计数
 * @return 引用计数值
 */
int pa_manager_get_ref_count(void)
{
    if (!s_pa_mgr) {
        return 0;
    }

    int count;
    lisa_mutex_lock(s_pa_mgr->mutex, LISA_OS_WAIT_FOREVER);
    count = s_pa_mgr->ref_count;
    lisa_mutex_unlock(s_pa_mgr->mutex);

    return count;
}

/**
 * @brief 反初始化 PA 管理器
 */
void pa_manager_deinit(void)
{
    if (!s_pa_mgr) {
        return;
    }

    /* 停止并销毁定时器 */
    if (s_pa_mgr->timer) {
        lisa_timer_stop(s_pa_mgr->timer);
        lisa_timer_destroy(s_pa_mgr->timer);
    }

    /* 销毁互斥锁 */
    if (s_pa_mgr->mutex) {
        lisa_mutex_delete(s_pa_mgr->mutex);
    }

    /* 释放内存 */
    lisa_mem_free(s_pa_mgr);
    s_pa_mgr = NULL;

    LISA_LOGI(TAG, "PA manager deinitialized");
}
