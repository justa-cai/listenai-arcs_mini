/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file arcs_dual_timer.c
 * @brief Dual Timer 适配 LISA硬件定时器驱动框架
 */

#include "lisa_hwtimer.h"
#include "Driver_DUAL_TIMER.h"
#include "ClockManager.h"
#include <string.h>
#include <lisa_mutex.h>

#define LOG_TAG "arcs_dual_timer"
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

#define DUAL_TIMER_SOURCE_CLK 16000
#define DUAL_TIMER_CHANNEL_COUNT 2

/* Dual Timer 支持的分频系数 */
#define DUAL_TIMER_PRESCALE_DIV_1   1
#define DUAL_TIMER_PRESCALE_DIV_16  16
#define DUAL_TIMER_PRESCALE_DIV_256 256

/* Dual Timer 支持的定时器频率 */
#define DUAL_TIMER_FREQ_DIV_1   (DUAL_TIMER_SOURCE_CLK / DUAL_TIMER_PRESCALE_DIV_1)    /* 16000 Hz */
#define DUAL_TIMER_FREQ_DIV_16  (DUAL_TIMER_SOURCE_CLK / DUAL_TIMER_PRESCALE_DIV_16)   /* 1000 Hz */
#define DUAL_TIMER_FREQ_DIV_256 (DUAL_TIMER_SOURCE_CLK / DUAL_TIMER_PRESCALE_DIV_256)  /* 62 Hz */

/* Dual Timer 通道私有数据结构 */
typedef struct {
    lisa_hwtimer_callback_t callback;
    void *user_data;
    uint32_t frequency_hz;  /* 当前设置的频率 */
    bool is_running;
    uint32_t count;         /* 保存的计数值，用于重置 */
    lisa_hwtimer_mode_t mode;  /* 保存的模式，用于重置 */
} dual_timer_channel_data_t;

/* Dual Timer 设备私有数据结构 */
typedef struct {
    void *dual_timer_handler;
    dual_timer_channel_data_t channels[DUAL_TIMER_CHANNEL_COUNT];
    uint32_t base_clock_hz;  /* 基准时钟频率 */
    lisa_mutex_t *mutex;     /* 互斥锁 */
} lisa_hwtimer_dual_data_t;

/* 静态实例数据 */
static lisa_hwtimer_dual_data_t dual_timer_priv = {
    .base_clock_hz = DUAL_TIMER_SOURCE_CLK
};

/* Dual Timer 事件回调 */
static void dual_timer_event_callback(uint32_t event, void *workspace)
{
    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)workspace;

    if (!data) {
        return;
    }

    /* 根据事件判断是哪个通道触发 */
    uint8_t channel = 0;
    if (event & CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH0) {
        channel = 0;
    } else if (event & CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH1) {
        channel = 1;
    } else {
        return;
    }

    dual_timer_channel_data_t *ch_data = &data->channels[channel];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

/* 获取硬件能力 */
static int dual_timer_get_capabilities(lisa_device_t *dev, lisa_hwtimer_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    caps->channel_count = DUAL_TIMER_CHANNEL_COUNT;       /* Dual Timer 有2个通道 */
    caps->max_count = 0xFFFFFFFF;                         /* 32位计数器 */
    caps->min_count = 1;
    caps->max_freq_hz = DUAL_TIMER_FREQ_DIV_1;            /* 最大频率: 16000 Hz (分频/1) */
    caps->min_freq_hz = DUAL_TIMER_FREQ_DIV_256;          /* 最小频率: 62 Hz (分频/256) */
    caps->support_oneshot = true;                         /* 支持单次模式 */
    caps->support_periodic = true;                        /* 支持周期模式 */

    return 0;
}

/* 设置频率 */
static int dual_timer_set_frequency(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= DUAL_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    /*
     * Dual Timer 内部时钟源固定为 16000Hz
     * 分频系数只支持: /1, /16, /256
     * 因此支持的频率为: 16000Hz, 1000Hz, 62.5Hz
     */
    if (freq_hz != DUAL_TIMER_FREQ_DIV_1 &&
        freq_hz != DUAL_TIMER_FREQ_DIV_16 &&
        freq_hz != DUAL_TIMER_FREQ_DIV_256) {
        LISA_LOGE(LOG_TAG, "Unsupported frequency %d Hz. Only support %d, %d, %d Hz",
              freq_hz, DUAL_TIMER_FREQ_DIV_1, DUAL_TIMER_FREQ_DIV_16, DUAL_TIMER_FREQ_DIV_256);
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    data->channels[channel].frequency_hz = freq_hz;

    DEVICE_UNLOCK(data);

    return 0;
}

/* 根据目标频率计算分频系数 */
static uint32_t calculate_prescale(uint32_t target_freq)
{
    /*
     * Dual Timer 内部时钟源固定为 DUAL_TIMER_SOURCE_CLK (16000Hz)
     * 只支持三种分频: /1, /16, /256
     */
    if (target_freq == DUAL_TIMER_FREQ_DIV_1) {
        return CSK_TIMER_PRESCALE_Divide_1;
    } else if (target_freq == DUAL_TIMER_FREQ_DIV_16) {
        return CSK_TIMER_PRESCALE_Divide_16;
    } else if (target_freq == DUAL_TIMER_FREQ_DIV_256) {
        return CSK_TIMER_PRESCALE_Divide_256;
    }

    /* 不应该到达这里，因为 set_frequency 已经做了检查 */
    LISA_LOGW(LOG_TAG, "Unsupported frequency %d Hz. Using default frequency %d Hz",
              target_freq, DUAL_TIMER_FREQ_DIV_1);
    return CSK_TIMER_PRESCALE_Divide_1;
}

/* 内部启动定时器函数（假设已持有锁） */
static int dual_timer_start_internal(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode)
{
    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    dual_timer_channel_data_t *ch_data = &data->channels[channel];

    /* 检查是否设置了回调函数，如果没有设置则不允许启动定时器
     * 避免 HAL 库中断处理时的空指针访问 */
    if (!ch_data->callback) {
        LISA_LOGE(LOG_TAG, "Callback not set for channel %d, cannot start timer", channel);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 计算分频系数 */
    uint32_t prescale = calculate_prescale(ch_data->frequency_hz);

    /* 配置定时器模式 */
    uint32_t control = prescale | CSK_TIMER_SIZE_32Bit | CSK_TIMER_INTERRUPT_Enabled;

    if (mode == LISA_HWTIMER_MODE_ONESHOT) {
        control |= CSK_TIMER_MODE_OneShot;
    } else {
        control |= CSK_TIMER_MODE_Periodic;
    }

    DUALTIMERS_Control(data->dual_timer_handler, control, channel);

    /* 设置回调（每个通道独立设置） */
    DUALTIMERS_SetTimerCallback(data->dual_timer_handler, channel, dual_timer_event_callback, data);

    /* 设置计数周期 */
    DUALTIMERS_SetTimerPeriodByCount(data->dual_timer_handler, channel, count);

    /* 启动定时器 */
    DUALTIMERS_StartTimer(data->dual_timer_handler, channel);

    ch_data->is_running = true;
    ch_data->count = count;  /* 保存计数值，用于重置 */
    ch_data->mode = mode;    /* 保存模式，用于重置 */

    return 0;
}

/* 启动定时器 */
static int dual_timer_start(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= DUAL_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    int ret = dual_timer_start_internal(dev, channel, count, mode);

    DEVICE_UNLOCK(data);

    return ret;
}

/* 停止定时器 */
static int dual_timer_stop(lisa_device_t *dev, uint8_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= DUAL_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    if (!data->channels[channel].is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    DUALTIMERS_StopTimer(data->dual_timer_handler, channel);
    data->channels[channel].is_running = false;

    DEVICE_UNLOCK(data);

    return 0;
}

/* 重置定时器 */
static int dual_timer_reset(lisa_device_t *dev, uint8_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= DUAL_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    dual_timer_channel_data_t *ch_data = &data->channels[channel];

    /* 如果定时器未运行，无需重置 */
    if (!ch_data->is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 停止定时器 */
    DUALTIMERS_StopTimer(data->dual_timer_handler, channel);

    /* 使用保存的配置参数重新启动定时器 */
    int ret = dual_timer_start_internal(dev, channel, ch_data->count, ch_data->mode);

    DEVICE_UNLOCK(data);

    return ret;
}

/* 获取当前计数值 */
static int dual_timer_get_value(lisa_device_t *dev, uint8_t channel, uint32_t *count)
{
    if (!lisa_device_is_initialized(dev) || !count) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= DUAL_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DUALTIMERS_ReadTimerCount(data->dual_timer_handler, channel, count);

    return 0;
}

/* 设置回调函数 */
static int dual_timer_set_callback(lisa_device_t *dev, uint8_t channel,
                                    lisa_hwtimer_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_dual_data_t *data = (lisa_hwtimer_dual_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= DUAL_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    data->channels[channel].callback = callback;
    data->channels[channel].user_data = user_data;

    DEVICE_UNLOCK(data);

    return 0;
}

/* Dual Timer API 实现 */
static const lisa_hwtimer_api_t dual_timer_api = {
    .get_capabilities = dual_timer_get_capabilities,
    .set_frequency = dual_timer_set_frequency,
    .start = dual_timer_start,
    .stop = dual_timer_stop,
    .reset = dual_timer_reset,
    .get_value = dual_timer_get_value,
    .set_callback = dual_timer_set_callback,
};

/* Dual Timer 初始化函数 */
static int arcs_dual_timer_init(void)
{
    /* 创建互斥锁 */
    dual_timer_priv.mutex = lisa_mutex_create();
    if (!dual_timer_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    dual_timer_priv.dual_timer_handler = DUALTIMERS0();
    DUALTIMERS_Initialize(dual_timer_priv.dual_timer_handler);
    DUALTIMERS_PowerControl(dual_timer_priv.dual_timer_handler, CSK_POWER_FULL);

    LISA_LOGI(LOG_TAG, "Dual Timer initialized base clock: %d Hz", dual_timer_priv.base_clock_hz);

    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(dual_timer, &dual_timer_api, &dual_timer_priv, NULL, arcs_dual_timer_init,
                     LISA_DEVICE_PRIORITY_NORMAL);
