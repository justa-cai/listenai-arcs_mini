/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file arcs_aon_timer.c
 * @brief AON Timer 适配 LISA硬件定时器驱动框架
 */

#include "lisa_hwtimer.h"
#include "ClockManager.h"
#include "Driver_AON_TIMER.h"
#include <string.h>
#include <lisa_mutex.h>

#define LOG_TAG "arcs_aon_timer"
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

/* XO32K 是外部32K晶振，固定频率 */
#define AON_TIMER_CLK_XO32K 32768

/* AON Timer 私有数据结构 */
typedef struct {
    void *aon_timer_handler;
    lisa_hwtimer_callback_t callback;
    void *user_data;
    uint32_t frequency_hz;  /* 当前设置的频率 */
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

    /* 配置定时器模式和时钟源 */
    uint32_t control = HAL_AON_TIMER_INTERRUPT_Enabled;

    /* 根据模式选择定时器模式 */
    if (mode == LISA_HWTIMER_MODE_ONESHOT) {
        control |= HAL_AON_TIMER_MODE_Normal;
    } else {
        control |= HAL_AON_TIMER_MODE_Repeat;
    }

    /* 根据 Kconfig 配置选择时钟源 */
#ifdef CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_XO32K
    control |= HAL_AON_TIMER_CLK_SEL_Xo32k;
#else
    control |= HAL_AON_TIMER_CLK_SEL_Rc32k;
#endif

    AON_TIMER_Control(data->aon_timer_handler, control);

    /* 设置计数周期 */
    AON_TIMER_SetTimerPeriodByCount(data->aon_timer_handler, count);

    /* 启动定时器 */
    AON_TIMER_StartTimer(data->aon_timer_handler);

    data->is_running = true;

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

    /* AON Timer 没有独立的reset功能，通过停止再启动实现 */
    AON_TIMER_StopTimer(data->aon_timer_handler);
    AON_TIMER_StartTimer(data->aon_timer_handler);

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

/* AON Timer 初始化函数 */
static int arcs_aon_timer_init(void)
{
    /* 根据配置选择时钟源 */
#ifdef CONFIG_LISA_HWTIMER_ARCS_AON_TIMER_CLK_XO32K
    /* 使用外部32K晶振，固定频率 */
    arcs_aon_timer_priv.frequency_hz = AON_TIMER_CLK_XO32K;
#else
    /* 使用内部RC32K，通过CRM动态获取频率 */
    arcs_aon_timer_priv.frequency_hz = CRM_GetSrcFreq(CRM_IpSrcAon32kClk);
#endif

    /* 创建互斥锁 */
    arcs_aon_timer_priv.mutex = lisa_mutex_create();
    if (!arcs_aon_timer_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 AON Timer 硬件 */
    arcs_aon_timer_priv.aon_timer_handler = AON_TIMER();
    AON_TIMER_Initialize(arcs_aon_timer_priv.aon_timer_handler, arcs_aon_timer_event_callback,
                         &arcs_aon_timer_priv);
    AON_TIMER_PowerControl(arcs_aon_timer_priv.aon_timer_handler, CSK_POWER_FULL);

    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(aon_timer, &arcs_aon_timer_api, &arcs_aon_timer_priv, NULL, arcs_aon_timer_init,
                     LISA_DEVICE_PRIORITY_NORMAL);
