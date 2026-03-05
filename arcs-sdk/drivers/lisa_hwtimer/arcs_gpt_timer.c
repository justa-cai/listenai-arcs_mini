/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file arcs_gpt_timer.c
 * @brief GPT Timer 适配 LISA硬件定时器驱动框架
 */

#include "lisa_hwtimer.h"
#include "Driver_GPT_TIMER.h"
#include "Driver_GPT_Common.h"
#include "ClockManager.h"
#include <string.h>
#include <lisa_mutex.h>

#define LOG_TAG "arcs_gpt_timer"
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

#define GPT_TIMER_SOURCE_CLK 100000000  /* 默认基准时钟 100MHz */
#define GPT_TIMER_CHANNEL_COUNT 8

/* GPT Timer 支持的分频系数 */
#define GPT_TIMER_PRESCALE_DIV_1   1
#define GPT_TIMER_PRESCALE_DIV_2   2
#define GPT_TIMER_PRESCALE_DIV_4   4
#define GPT_TIMER_PRESCALE_DIV_8   8
#define GPT_TIMER_PRESCALE_DIV_16  16
#define GPT_TIMER_PRESCALE_DIV_32  32
#define GPT_TIMER_PRESCALE_DIV_64  64
#define GPT_TIMER_PRESCALE_DIV_128 128

/* GPT Timer 支持的定时器频率 (基于默认100MHz时钟源) */
#define GPT_TIMER_FREQ_DIV_1   (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_1)    /* 100000000 Hz */
#define GPT_TIMER_FREQ_DIV_2   (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_2)    /* 50000000 Hz */
#define GPT_TIMER_FREQ_DIV_4   (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_4)    /* 25000000 Hz */
#define GPT_TIMER_FREQ_DIV_8   (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_8)    /* 12500000 Hz */
#define GPT_TIMER_FREQ_DIV_16  (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_16)   /* 6250000 Hz */
#define GPT_TIMER_FREQ_DIV_32  (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_32)   /* 3125000 Hz */
#define GPT_TIMER_FREQ_DIV_64  (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_64)   /* 1562500 Hz */
#define GPT_TIMER_FREQ_DIV_128 (GPT_TIMER_SOURCE_CLK / GPT_TIMER_PRESCALE_DIV_128)  /* 781250 Hz */

/* GPT Timer 通道私有数据结构 */
typedef struct {
    lisa_hwtimer_callback_t callback;
    void *user_data;
    uint32_t frequency_hz;  /* 当前设置的频率 */
    uint32_t count;         /* 保存的计数值 */
    lisa_hwtimer_mode_t mode; /* 保存的定时器模式 */
    bool is_running;
} gpt_timer_channel_data_t;

/* GPT Timer 设备私有数据结构 */
typedef struct {
    void *gpt_timer_handler;
    gpt_timer_channel_data_t channels[GPT_TIMER_CHANNEL_COUNT];
    uint32_t base_clock_hz;  /* 基准时钟频率 */
    lisa_mutex_t *mutex;     /* 互斥锁 */
} lisa_hwtimer_gpt_data_t;

/* 静态实例数据 */
static lisa_hwtimer_gpt_data_t gpt_timer_priv = {
    .base_clock_hz = GPT_TIMER_SOURCE_CLK,
};

/* GPT Timer 事件回调包装 */
static void gpt_timer_event_callback_ch0(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[0];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch1(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[1];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch2(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[2];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch3(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[3];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch4(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[4];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch5(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[5];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch6(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[6];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

static void gpt_timer_event_callback_ch7(uint32_t event, void *param)
{
    gpt_timer_channel_data_t *ch_data = &gpt_timer_priv.channels[7];
    if (ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

/* 回调函数数组 */
static CSK_GPT_SignalEvent_t gpt_callbacks[GPT_TIMER_CHANNEL_COUNT] = {
    gpt_timer_event_callback_ch0,
    gpt_timer_event_callback_ch1,
    gpt_timer_event_callback_ch2,
    gpt_timer_event_callback_ch3,
    gpt_timer_event_callback_ch4,
    gpt_timer_event_callback_ch5,
    gpt_timer_event_callback_ch6,
    gpt_timer_event_callback_ch7,
};

/* 获取硬件能力 */
static int gpt_timer_get_capabilities(lisa_device_t *dev, lisa_hwtimer_capabilities_t *caps)
{
    if (!dev || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    caps->channel_count = GPT_TIMER_CHANNEL_COUNT;                     /* GPT Timer 有8个通道 */
    caps->max_count = 0xFFFFFFFF;                                      /* 32位计数器 */
    caps->min_count = 1;
    caps->max_freq_hz = data->base_clock_hz;                           /* 最大频率等于基准时钟 (分频/1) */
    caps->min_freq_hz = data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_128;  /* 最小频率 (分频/128) */
    caps->support_oneshot = true;                                      /* 支持单次模式 */
    caps->support_periodic = true;                                     /* 支持周期模式 */

    return 0;
}

/* 设置频率 */
static int gpt_timer_set_frequency(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    /*
     * GPT Timer 内部时钟源默认为 100MHz
     * 分频系数支持: /1, /2, /4, /8, /16, /32, /64, /128
     * 因此支持的频率为: 100MHz, 50MHz, 25MHz, 12.5MHz, 6.25MHz, 3.125MHz, 1.5625MHz, 781.25kHz
     * 注意: 如果实际时钟频率与默认值不同，这些频率值也会相应变化
     */
    if (freq_hz != data->base_clock_hz &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_2 &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_4 &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_8 &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_16 &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_32 &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_64 &&
        freq_hz != data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_128) {
        LISA_LOGE(LOG_TAG, "Unsupported frequency %u Hz. Only support %u, %u, %u, %u, %u, %u, %u, %u Hz",
              freq_hz, data->base_clock_hz, data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_2,
              data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_4, data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_8,
              data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_16, data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_32,
              data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_64, data->base_clock_hz / GPT_TIMER_PRESCALE_DIV_128);
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    /* 保存频率设置 */
    data->channels[channel].frequency_hz = freq_hz;

    DEVICE_UNLOCK(data);

    return 0;
}

/* 根据目标频率计算分频系数 */
static uint32_t calculate_clock_divider(uint32_t base_clock, uint32_t target_freq)
{
    uint32_t ratio = base_clock / target_freq;

    if (ratio <= 1) {
        return CSK_GPT_TIMER_CLKDIV_1;
    } else if (ratio <= 2) {
        return CSK_GPT_TIMER_CLKDIV_2;
    } else if (ratio <= 4) {
        return CSK_GPT_TIMER_CLKDIV_4;
    } else if (ratio <= 8) {
        return CSK_GPT_TIMER_CLKDIV_8;
    } else if (ratio <= 16) {
        return CSK_GPT_TIMER_CLKDIV_16;
    } else if (ratio <= 32) {
        return CSK_GPT_TIMER_CLKDIV_32;
    } else if (ratio <= 64) {
        return CSK_GPT_TIMER_CLKDIV_64;
    } else {
        return CSK_GPT_TIMER_CLKDIV_128;
    }
}

/* 内部启动定时器函数（假设已持有锁） */
static int gpt_timer_start_internal(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode)
{
    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    gpt_timer_channel_data_t *ch_data = &data->channels[channel];

    /* 检查是否设置了回调函数，如果没有设置则不允许启动定时器
     * 避免 HAL 库中断处理时的空指针访问 */
    if (!ch_data->callback) {
        LISA_LOGE(LOG_TAG, "Callback not set for channel %d, cannot start timer", channel);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 保存配置参数 */
    ch_data->count = count;
    ch_data->mode = mode;

    /* 计算分频系数 */
    uint32_t clk_div = calculate_clock_divider(data->base_clock_hz, ch_data->frequency_hz);

    /* 配置定时器模式 */
    uint32_t control = CSK_GPT_TIMER_32_BIT_TIMER |       /* 32位定时器 */
                       CSK_GPT_TIMER_CLKSRC_PCLK |        /* 使用PCLK时钟源 */
                       clk_div |                          /* 时钟分频 */
                       CSK_GPT_TIMER_COUNTER_DOWN;        /* 向下计数 */

    if (mode == LISA_HWTIMER_MODE_ONESHOT) {
        control |= CSK_GPT_TIMER_RUNMODE_SINGLE;          /* 单次模式 */
    } else {
        control |= CSK_GPT_TIMER_RUNMODE_REPEAT;          /* 周期模式 */
    }

    HAL_GPT_TimerControl(data->gpt_timer_handler, control, (GPT_CHANNEL_TYPE)channel);

    /* 注册回调 */
    HAL_GPT_RegisterTimerCallback(data->gpt_timer_handler, (GPT_CHANNEL_TYPE)channel,
                                   gpt_callbacks[channel]);

    /* 设置计数周期 */
    HAL_GPT_SetTimerPeriodByCount(data->gpt_timer_handler, (GPT_CHANNEL_TYPE)channel, count);

    /* 启动定时器 */
    HAL_GPT_StartTimer(data->gpt_timer_handler, (GPT_CHANNEL_TYPE)channel);

    ch_data->is_running = true;

    return 0;
}

/* 启动定时器 */
static int gpt_timer_start(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    int ret = gpt_timer_start_internal(dev, channel, count, mode);

    DEVICE_UNLOCK(data);

    return ret;
}

/* 停止定时器 */
static int gpt_timer_stop(lisa_device_t *dev, uint8_t channel)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    if (!data->channels[channel].is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    HAL_GPT_StopTimer(data->gpt_timer_handler, (GPT_CHANNEL_TYPE)channel);
    data->channels[channel].is_running = false;

    DEVICE_UNLOCK(data);

    return 0;
}

/* 重置定时器 */
static int gpt_timer_reset(lisa_device_t *dev, uint8_t channel)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    gpt_timer_channel_data_t *ch_data = &data->channels[channel];

    /* 如果定时器未运行，无需重置 */
    if (!ch_data->is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 停止定时器 */
    HAL_GPT_StopTimer(data->gpt_timer_handler, (GPT_CHANNEL_TYPE)channel);

    /* 使用保存的配置参数重新启动定时器 */
    int ret = gpt_timer_start_internal(dev, channel, ch_data->count, ch_data->mode);

    DEVICE_UNLOCK(data);

    return ret;
}

/* 获取当前计数值 */
static int gpt_timer_get_value(lisa_device_t *dev, uint8_t channel, uint32_t *count)
{
    if (!dev || !count) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    HAL_GPT_ReadTimerCount(data->gpt_timer_handler, (GPT_CHANNEL_TYPE)channel, count);

    return 0;
}

/* 设置回调函数 */
static int gpt_timer_set_callback(lisa_device_t *dev, uint8_t channel,
                                   lisa_hwtimer_callback_t callback, void *user_data)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_hwtimer_gpt_data_t *data = (lisa_hwtimer_gpt_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查通道号 */
    if (channel >= GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    data->channels[channel].callback = callback;
    data->channels[channel].user_data = user_data;

    DEVICE_UNLOCK(data);

    return 0;
}

/* GPT Timer API 实现 */
static const lisa_hwtimer_api_t gpt_timer_api = {
    .get_capabilities = gpt_timer_get_capabilities,
    .set_frequency = gpt_timer_set_frequency,
    .start = gpt_timer_start,
    .stop = gpt_timer_stop,
    .reset = gpt_timer_reset,
    .get_value = gpt_timer_get_value,
    .set_callback = gpt_timer_set_callback,
};

/* GPT Timer 初始化函数 */
static int arcs_gpt_timer_init(void)
{
    /* 创建互斥锁 */
    gpt_timer_priv.mutex = lisa_mutex_create();
    if (!gpt_timer_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 GPT Timer 硬件 */
    gpt_timer_priv.gpt_timer_handler = GPT0_TIMER();
    if (!gpt_timer_priv.gpt_timer_handler) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    int32_t ret = HAL_GPT_TimerInitialize(gpt_timer_priv.gpt_timer_handler, NULL);
    if (ret != 0) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = HAL_GPT_TimerPowerControl(gpt_timer_priv.gpt_timer_handler, CSK_POWER_FULL);
    if (ret != 0) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    return LISA_DEVICE_OK;
}


LISA_DEVICE_REGISTER(gpt_timer, &gpt_timer_api, &gpt_timer_priv, NULL, arcs_gpt_timer_init,
                     LISA_DEVICE_PRIORITY_NORMAL);
