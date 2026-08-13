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

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

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

/**
 * @brief OS 资源初始化（mutex），仅 _init 阶段调用一次，跨 suspend/resume 保留
 */
static int arcs_dual_timer_init_resources(lisa_hwtimer_dual_data_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 Dual Timer HAL 硬件初始化
 *
 * 由 _init 调用；只动 HAL，不分配 mutex / 堆内存。唤醒后经 reinit 重新走 _init
 * 路径时，destroy 阶段已先 DUALTIMERS_PowerControl(OFF) + DUALTIMERS_Uninitialize
 * 清零 HAL 状态；启动期首次调用时 HAL 状态本就为零，重复 Initialize 无副作用。
 */
static int arcs_dual_timer_init_hw(lisa_hwtimer_dual_data_t *priv)
{
    priv->dual_timer_handler = DUALTIMERS0();
    if (!priv->dual_timer_handler) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (DUALTIMERS_Initialize(priv->dual_timer_handler) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    if (DUALTIMERS_PowerControl(priv->dual_timer_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 唤醒路径下硬件已被 PowerControl(OFF) 重置，逻辑通道运行态也必须清零。 */
    for (int i = 0; i < DUAL_TIMER_CHANNEL_COUNT; ++i) {
        priv->channels[i].is_running = false;
    }

    LISA_LOGI(LOG_TAG, "Dual Timer initialized base clock: %d Hz", priv->base_clock_hz);

    return LISA_DEVICE_OK;
}

/* Dual Timer 初始化函数 */
static int arcs_dual_timer_init(void)
{
    int ret = arcs_dual_timer_init_resources(&dual_timer_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return arcs_dual_timer_init_hw(&dual_timer_priv);
}

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放 Dual Timer 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 _init 申请相反：
 *   1) HAL 下电：先 DUALTIMERS_PowerControl(OFF) 再 DUALTIMERS_Uninitialize；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（各通道 is_running / 应用配置
 *      随之清零，强制唤醒后业务侧重新 start()）。
 *
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int arcs_dual_timer_deinit(void)
{
    lisa_hwtimer_dual_data_t *priv = &dual_timer_priv;

    if (priv->dual_timer_handler) {
        DUALTIMERS_PowerControl(priv->dual_timer_handler, CSK_POWER_OFF);
        DUALTIMERS_Uninitialize(priv->dual_timer_handler);
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&dual_timer_priv, 0, sizeof(dual_timer_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(dual_timer) 释放全部软硬件资源（HAL 下电 + mutex），唤醒后经
 * lisa_device_reinit(dual_timer) 重建到 _init 后的状态，并由业务重新 start()。因此
 * prepare_suspend / resume_restore 不再需要（原先它们只做 HAL 拆卸 / 运行态清零，已被
 * destroy/reinit 覆盖，且二者运行于 PM 临界区无法做重活）。
 *
 * 仅保留 check_idle：任一通道 is_running 即代表应用要求 Dual Timer 持续计数，禁止
 * AUTO_LIGHT_SLEEP。只读 priv，不取 mutex / 不读 HAL。
 */
static int32_t arcs_dual_timer_pm_check_idle(void *ctx)
{
    lisa_hwtimer_dual_data_t *priv = (lisa_hwtimer_dual_data_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    for (int i = 0; i < DUAL_TIMER_CHANNEL_COUNT; ++i) {
        if (priv->channels[i].is_running) {
            return 0;
        }
    }
    return 1;
}

static const lisa_pm_system_ops_t arcs_dual_timer_pm_ops = {
    .check_idle      = arcs_dual_timer_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */


LISA_DEVICE_REGISTER_DEINIT(dual_timer, &dual_timer_api, &dual_timer_priv, NULL, arcs_dual_timer_init,
                            arcs_dual_timer_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(dual_timer, &arcs_dual_timer_pm_ops, NULL, &dual_timer_priv);
#endif
