/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file venusa_aon_timer.c
 * @brief AON Timer 适配 LISA硬件定时器驱动框架
 */

#include "lisa_hwtimer.h"
#include "ClockManager.h"
#include "Driver_AON_TIMER.h"
#include <string.h>
#include <lisa_mutex.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "venusa_aon_timer"
#include <lisa_log.h>

#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_lock(priv->mutex, LISA_OS_WAIT_FOREVER);                                                        \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_unlock(priv->mutex);                                                                            \
        }                                                                                                              \
    } while (0)

/* 32K 域典型频率。Venusa 24M_Div32K 路径由硬件分频到 32K 域。 */
#define AON_TIMER_CLK_32K 32768U

/* AON Timer 私有数据结构 */
typedef struct {
    void *aon_timer_handler;
    lisa_hwtimer_callback_t callback;
    void *user_data;
    uint32_t frequency_hz;  /* 当前设置的频率 */
    uint32_t count;         /* 保存的计数值，用于 reset */
    lisa_hwtimer_mode_t mode;  /* 保存的模式，用于 reset */
    bool is_running;
    lisa_mutex_t *mutex;    /* 互斥锁 */
} arcs_aon_timer_priv_data_t;

/* 静态实例数据 */
static arcs_aon_timer_priv_data_t arcs_aon_timer_priv = {
    .is_running = false,
    .frequency_hz = 0,  /* 在初始化时设置 */
};

/* AON Timer 事件回调 */
static void arcs_aon_timer_event_callback(uint32_t event, void *workspace)
{
    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)workspace;

    if (data && data->callback) {
        data->callback(data->user_data);
    }
}

static uint32_t arcs_aon_timer_make_control(lisa_hwtimer_mode_t mode)
{
    uint32_t control = HAL_AON_TIMER_INTERRUPT_Enabled;

    /* 根据模式选择定时器模式 */
    if (mode == LISA_HWTIMER_MODE_ONESHOT) {
        control |= HAL_AON_TIMER_MODE_Normal;
    } else {
        control |= HAL_AON_TIMER_MODE_Repeat;
    }

    /* 根据 Kconfig 配置选择时钟源 */
#ifdef CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_XO24M_DIV32K
    control |= HAL_AON_TIMER_CLK_SEL_XO24M_Div32K;
#elif defined(CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_RC24M_DIV32K)
    control |= HAL_AON_TIMER_CLK_SEL_RC24M_Div32K;
#else
    control |= HAL_AON_TIMER_CLK_SEL_Rc32K;
#endif

    return control;
}

/* 获取硬件能力 */
static int arcs_aon_timer_get_capabilities(lisa_device_t *dev, lisa_hwtimer_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    caps->channel_count = 1;           /* AON Timer 只有1个通道 */
    caps->max_count = 0xFFFFFF;        /* 24位计数器 */
    caps->min_count = 1;
    caps->max_freq_hz = data->frequency_hz;  /* 使用实际的时钟频率 */
    caps->min_freq_hz = data->frequency_hz;
    caps->support_oneshot = true;      /* 支持单次模式 */
    caps->support_periodic = true;     /* 支持周期模式 */

    return LISA_DEVICE_OK;
}

/* 设置频率 */
static int arcs_aon_timer_set_frequency(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    /* AON Timer 只支持 Kconfig 配置的时钟源频率 */
    if (freq_hz != data->frequency_hz) {
        LISA_LOGE(LOG_TAG, "Invalid frequency: %d Hz, only %d Hz is supported", freq_hz, data->frequency_hz);
        return LISA_DEVICE_ERR_RANGE;
    }

    data->frequency_hz = freq_hz;

    return LISA_DEVICE_OK;
}

/* 启动定时器 */
static int arcs_aon_timer_start(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    /* 检查是否设置了回调函数，如果没有设置则不允许启动定时器
     * 避免 HAL 库中断处理时的空指针访问 */
    if (!data->callback) {
        LISA_LOGE(LOG_TAG, "Callback not set for channel %d, cannot start timer", channel);
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查count是否满足24bit有效范围 */
    if (count < 1 || count > 0xFFFFFF) {
        LISA_LOGE(LOG_TAG, "Count for channel %d is out of range, cannot start timer", channel);
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_RANGE;
    }

    uint32_t control = arcs_aon_timer_make_control(mode);
    if (AON_TIMER_Control(data->aon_timer_handler, control) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }

    /* 设置计数周期 */
    if (AON_TIMER_SetTimerPeriodByCount(data->aon_timer_handler, count) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }

    /* 启动定时器 */
    if (AON_TIMER_StartTimer(data->aon_timer_handler) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }

    data->is_running = true;
    data->count = count;
    data->mode = mode;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

/* 停止定时器 */
static int arcs_aon_timer_stop(lisa_device_t *dev, uint8_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    if (!data->is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    AON_TIMER_StopTimer(data->aon_timer_handler);
    data->is_running = false;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

/* 重置定时器 */
static int arcs_aon_timer_reset(lisa_device_t *dev, uint8_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    if (!data->is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* AON Timer 没有独立的reset功能，通过重新装载保存的周期实现。 */
    if (AON_TIMER_StopTimer(data->aon_timer_handler) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }
    data->is_running = false;

    uint32_t control = arcs_aon_timer_make_control(data->mode);
    if (AON_TIMER_Control(data->aon_timer_handler, control) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }

    if (AON_TIMER_SetTimerPeriodByCount(data->aon_timer_handler, data->count) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }

    if (AON_TIMER_StartTimer(data->aon_timer_handler) != CSK_DRIVER_OK) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_IO;
    }
    data->is_running = true;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

/* 获取当前计数值 */
static int arcs_aon_timer_get_value(lisa_device_t *dev, uint8_t channel, uint32_t *count)
{
    if (!lisa_device_is_initialized(dev) || !count) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    AON_TIMER_ReadTimerCount(data->aon_timer_handler, count);

    return LISA_DEVICE_OK;
}

/* 设置回调函数 */
static int arcs_aon_timer_set_callback(lisa_device_t *dev, uint8_t channel,
                                        lisa_hwtimer_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_aon_timer_priv_data_t *data = (arcs_aon_timer_priv_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    data->callback = callback;
    data->user_data = user_data;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

/* AON Timer API 实现 */
static const lisa_hwtimer_api_t arcs_aon_timer_api = {
    .get_capabilities = arcs_aon_timer_get_capabilities,
    .set_frequency = arcs_aon_timer_set_frequency,
    .start = arcs_aon_timer_start,
    .stop = arcs_aon_timer_stop,
    .reset = arcs_aon_timer_reset,
    .get_value = arcs_aon_timer_get_value,
    .set_callback = arcs_aon_timer_set_callback,
};

/**
 * @brief OS 资源初始化（mutex），仅 _init 阶段调用一次，跨 suspend/resume 保留
 */
static int arcs_aon_timer_init_resources(arcs_aon_timer_priv_data_t *priv)
{
    /* 根据配置选择时钟源 */
#ifdef CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_XO24M_DIV32K
    priv->frequency_hz = AON_TIMER_CLK_32K;     /* 使用外部24M晶振分频为32K，固定频率 */
#elif defined(CONFIG_LISA_HWTIMER_VENUSA_AON_TIMER_CLK_RC24M_DIV32K)
    priv->frequency_hz = AON_TIMER_CLK_32K;     /* 使用内部RC 24M分频为32K，固定频率 */
#else
    priv->frequency_hz = CRM_GetSrcFreq(CRM_IpSrcRC032K); /* 使用内部RC 32K*/
    if (priv->frequency_hz == 0) {
        priv->frequency_hz = AON_TIMER_CLK_32K;
    }
#endif

    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 AON Timer HAL 硬件初始化
 *
 * always-on 域硬件 retention，启动期 _init 与（如有）唤醒后 resume_restore 共用。
 * 这里仍按 Setup S.2 拆分，保持与其他驱动一致的形态。
 */
static int arcs_aon_timer_init_hw(arcs_aon_timer_priv_data_t *priv)
{
    priv->aon_timer_handler = AON_TIMER();
    if (!priv->aon_timer_handler) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (AON_TIMER_Initialize(priv->aon_timer_handler, arcs_aon_timer_event_callback, priv) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (AON_TIMER_PowerControl(priv->aon_timer_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    return LISA_DEVICE_OK;
}

/* AON Timer 初始化函数 */
static int arcs_aon_timer_init(void)
{
    int ret = arcs_aon_timer_init_resources(&arcs_aon_timer_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return arcs_aon_timer_init_hw(&arcs_aon_timer_priv);
}

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放 AON Timer 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 _init 申请相反：
 *   1) HAL 下电：先 AON_TIMER_PowerControl(OFF) 再 AON_TIMER_Uninitialize；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值。
 *
 * 注意：AON Timer 位于 always-on 域，PM 自动轻睡眠路径下 check_idle 恒返 1、绝不销毁
 * 本设备（否则会破坏唤醒源 / 心跳语义）；本 deinit 仅服务于业务显式 lisa_device_destroy()。
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int arcs_aon_timer_deinit(void)
{
    arcs_aon_timer_priv_data_t *priv = &arcs_aon_timer_priv;

    if (priv->aon_timer_handler) {
        AON_TIMER_PowerControl(priv->aon_timer_handler, CSK_POWER_OFF);
        AON_TIMER_Uninitialize(priv->aon_timer_handler);
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&arcs_aon_timer_priv, 0, sizeof(arcs_aon_timer_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：业务可显式调
 * lisa_device_destroy(aon_timer) 释放资源、唤醒后 lisa_device_reinit(aon_timer) 重建。
 * 因此 prepare_suspend / resume_restore 不再需要（原为空占位，已被 destroy/reinit 覆盖）。
 *
 * 仅保留 check_idle：AON Timer 位于 always-on 域、硬件 retention，且常作为唤醒源 /
 * 长周期心跳，故恒返 1（始终允许自动轻睡眠、绝不因其阻塞整机，也不在 PM 路径销毁）。
 */
static int32_t arcs_aon_timer_pm_check_idle(void *ctx)
{
    (void)ctx;
    return 1;
}

static const lisa_pm_system_ops_t arcs_aon_timer_pm_ops = {
    .check_idle      = arcs_aon_timer_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */


LISA_DEVICE_REGISTER_DEINIT(aon_timer, &arcs_aon_timer_api, &arcs_aon_timer_priv, NULL, arcs_aon_timer_init,
                            arcs_aon_timer_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(aon_timer, &arcs_aon_timer_pm_ops, NULL, &arcs_aon_timer_priv);
#endif
