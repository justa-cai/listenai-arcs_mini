/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file venusa_gpt_timer.c
 * @brief Venusa GPT Timer 适配 LISA 硬件定时器驱动框架
 */

#include "lisa_hwtimer.h"

#include "ClockManager.h"
#include "Driver_GPT_Common.h"
#include "Driver_GPT_TIMER.h"
#include "lisa_mutex.h"

#include <stdbool.h>
#include <stdint.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "venusa_gpt_timer"
#include <lisa_log.h>

#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if ((priv)->mutex) {                                                                                           \
            lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER);                                                      \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if ((priv)->mutex) {                                                                                           \
            lisa_mutex_unlock((priv)->mutex);                                                                          \
        }                                                                                                              \
    } while (0)

#define VENUSA_GPT_TIMER_CHANNEL_COUNT GPT_NUMBER_OF_CHANNELS
#define VENUSA_GPT_TIMER_MODE          HAL_GPT_TIMER_16BITS_INDEX_0
#define VENUSA_GPT_TIMER_MAX_COUNT     UINT16_MAX
#define VENUSA_GPT_TIMER_MIN_COUNT     1U
#define VENUSA_GPT_TIMER_MAX_PREDIV    65536U

/* ===== GPT Timer 时钟分频定义 ===== */
typedef struct {
    GPT_Clk_Div_t hal_div; /* HAL 分频枚举 */
    uint32_t factor;       /* 实际分频系数 */
} venusa_gpt_clk_div_t;

static const venusa_gpt_clk_div_t venusa_gpt_clk_divs[] = {
    {GPT_CLK_DIV_1, 1U},   {GPT_CLK_DIV_2, 2U},   {GPT_CLK_DIV_4, 4U},   {GPT_CLK_DIV_8, 8U},
    {GPT_CLK_DIV_16, 16U}, {GPT_CLK_DIV_32, 32U}, {GPT_CLK_DIV_64, 64U}, {GPT_CLK_DIV_128, 128U},
};

/* 只使用芯片内部稳定时钟源；EXCLK 需要外部输入，当前驱动不自动选择。 */
static const GPT_Clk_Src_t venusa_gpt_clk_sources[] = {
    GPT_CLK_SRC_PCLK,
    GPT_CLK_SRC_T0,
};

/* ===== GPT Timer 通道私有数据 ===== */
typedef struct {
    lisa_hwtimer_callback_t callback;
    void *user_data;
    uint32_t frequency_hz;     /* 当前设置的计数频率 */
    uint32_t count;            /* 保存的计数值，用于 reset */
    lisa_hwtimer_mode_t mode;  /* 保存的模式，用于 reset */
    GPT_Config_Para_t clk_cfg; /* 当前通道 GPT clock 配置 */
    bool clk_configured;       /* 是否已完成 frequency -> clock 配置计算 */
    bool is_running;
} venusa_gpt_channel_data_t;

/* ===== GPT Timer 设备私有数据 ===== */
typedef struct {
    void *gpt_handler;
    venusa_gpt_channel_data_t channels[VENUSA_GPT_TIMER_CHANNEL_COUNT];
    lisa_mutex_t *mutex;
} venusa_gpt_timer_data_t;

static venusa_gpt_timer_data_t venusa_gpt_timer_priv;

/**
 * @brief 获取 GPT 时钟源当前频率
 *
 * 从 ClockManager 读取运行时 PCLK/T0 频率，避免假设系统时钟固定。EXCLK 需要
 * 外部输入，当前驱动不自动选择，返回 0。
 *
 * @param clk_src GPT 时钟源
 * @return uint32_t 时钟源频率，单位 Hz；0 表示不可用
 */
static uint32_t venusa_gpt_source_frequency_hz(GPT_Clk_Src_t clk_src)
{
    switch (clk_src) {
    case GPT_CLK_SRC_T0:
        return CRM_GetGptFreq();
    case GPT_CLK_SRC_PCLK:
        return CRM_GetCmn_pclkFreq();
    case GPT_CLK_SRC_EXCLK:
    default:
        return 0;
    }
}

/**
 * @brief 根据目标计数频率选择 GPT clock/prediv/divider
 *
 * Venusa GPT timer 的计数频率为 source_hz / divider / (prediv + 1)。LISA
 * hwtimer API 把 count 和 frequency 直接暴露给应用，因此这里只接受可精确表示
 * 的目标频率，避免回调周期与上层计算不一致。
 *
 * @param freq_hz 目标计数频率
 * @param cfg 输出参数，返回 HAL clock 配置
 * @return LISA_DEVICE_OK 找到精确配置
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 目标频率不支持
 */
static int venusa_gpt_select_clock(uint32_t freq_hz, GPT_Config_Para_t *cfg)
{
    if (!cfg || freq_hz == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    for (uint32_t src_idx = 0; src_idx < sizeof(venusa_gpt_clk_sources) / sizeof(venusa_gpt_clk_sources[0]);
         src_idx++) {
        GPT_Clk_Src_t clk_src = venusa_gpt_clk_sources[src_idx];
        uint32_t src_hz = venusa_gpt_source_frequency_hz(clk_src);
        if (src_hz == 0) {
            continue;
        }

        for (uint32_t div_idx = 0; div_idx < sizeof(venusa_gpt_clk_divs) / sizeof(venusa_gpt_clk_divs[0]); div_idx++) {
            /*
                只接受可精确表示的目标频率
                目标频率计算方式: freq_hz = src_hz / divider / (prediv + 1)
                对应代码中:
                prediv_factor = prediv + 1
                所以要让目标频率精确成立,需要: freq_hz = src_hz / divider / prediv_factor
                变形后: prediv_factor = src_hz / (freq_hz * divider)
                也就是: prediv_factor = src_hz / denom
                但 prediv_factor 必须是整数, 因为硬件 prediv 寄存器只能配置整数分频
            */
            uint32_t divider = venusa_gpt_clk_divs[div_idx].factor;
            uint64_t denom = (uint64_t)freq_hz * divider;
            if (denom == 0 || src_hz % denom != 0) {
                continue;
            }

            uint32_t prediv_factor = (uint32_t)(src_hz / denom);
            if (prediv_factor == 0 || prediv_factor > VENUSA_GPT_TIMER_MAX_PREDIV) {
                continue;
            }

            cfg->clk_src = clk_src;
            cfg->prediv = (uint16_t)(prediv_factor - 1U);
            cfg->clk_div = venusa_gpt_clk_divs[div_idx].hal_div;
            LISA_LOGD(LOG_TAG, "Selected GPT timing: freq=%lu Hz, clk_src=%d, prediv=%u, clk_div=%d", freq_hz,
                      (int)cfg->clk_src, cfg->prediv, (int)cfg->clk_div);
            return LISA_DEVICE_OK;
        }
    }

    LISA_LOGE(LOG_TAG, "Unsupported GPT timer frequency %lu Hz", freq_hz);
    return LISA_DEVICE_ERR_RANGE;
}

/**
 * @brief 计算当前平台可用 GPT 计数频率范围
 *
 * 遍历当前可用的 PCLK/T0 时钟源，最大值为最高源频率，最小值为最低源频率经过
 * 128 分频和 16-bit prediv 后的频率。
 *
 * @param min_freq_hz 输出最小频率
 * @param max_freq_hz 输出最大频率
 */
static void venusa_gpt_get_freq_range(uint32_t *min_freq_hz, uint32_t *max_freq_hz)
{
    uint32_t min_freq = UINT32_MAX;
    uint32_t max_freq = 0;

    for (uint32_t i = 0; i < sizeof(venusa_gpt_clk_sources) / sizeof(venusa_gpt_clk_sources[0]); i++) {
        uint32_t src_hz = venusa_gpt_source_frequency_hz(venusa_gpt_clk_sources[i]);
        if (src_hz == 0) {
            continue;
        }

        if (src_hz > max_freq) {
            max_freq = src_hz;
        }

        uint32_t candidate_min = src_hz / (128U * VENUSA_GPT_TIMER_MAX_PREDIV);
        if (candidate_min == 0) {
            candidate_min = 1;
        }
        if (candidate_min < min_freq) {
            min_freq = candidate_min;
        }
    }

    if (min_freq == UINT32_MAX) {
        min_freq = 0;
    }

    *min_freq_hz = min_freq;
    *max_freq_hz = max_freq;
}

/**
 * @brief GPT Timer 事件回调
 *
 * HAL 中断回调通过 workspace 传入通道私有数据，避免按 event 反推通道号。
 *
 * @param event HAL 事件标志
 * @param param 通道私有数据
 */
static void venusa_gpt_timer_event_callback(uint32_t event, void *param)
{
    (void)event;

    venusa_gpt_channel_data_t *ch_data = (venusa_gpt_channel_data_t *)param;
    if (ch_data && ch_data->callback) {
        ch_data->callback(ch_data->user_data);
    }
}

/**
 * @brief 获取硬件能力
 *
 * @param dev LISA 设备对象
 * @param caps 输出硬件能力
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 */
static int venusa_gpt_timer_get_capabilities(lisa_device_t *dev, lisa_hwtimer_capabilities_t *caps)
{
    if (!dev || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    uint32_t min_freq = 0;
    uint32_t max_freq = 0;
    venusa_gpt_get_freq_range(&min_freq, &max_freq);

    caps->channel_count = VENUSA_GPT_TIMER_CHANNEL_COUNT;
    caps->max_count = VENUSA_GPT_TIMER_MAX_COUNT;
    caps->min_count = VENUSA_GPT_TIMER_MIN_COUNT;
    caps->max_freq_hz = max_freq;
    caps->min_freq_hz = min_freq;
    caps->support_oneshot = true;
    caps->support_periodic = true;

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置 GPT timer 计数频率
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @param freq_hz 目标计数频率，必须可由 GPT clock/prediv/divider 精确表示
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_RANGE 通道号或频率无效
 */
static int venusa_gpt_timer_set_frequency(lisa_device_t *dev, uint8_t channel, uint32_t freq_hz)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (channel >= VENUSA_GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    GPT_Config_Para_t cfg = {0};
    int ret = venusa_gpt_select_clock(freq_hz, &cfg);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    DEVICE_LOCK(data);

    data->channels[channel].frequency_hz = freq_hz;
    data->channels[channel].clk_cfg = cfg;
    data->channels[channel].clk_configured = true;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

/**
 * @brief 内部启动 GPT timer（调用方已持有锁）
 *
 * 配置 GPT channel clock、16-bit timer 模式、回调和 reload count，然后启动计数。
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @param count 计数值，Venusa 16-bit timer 上限为 0xffff
 * @param mode LISA timer 运行模式
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 回调或频率未配置
 * @return LISA_DEVICE_ERR_RANGE count 越界
 * @return LISA_DEVICE_ERR_IO HAL 操作失败
 */
static int venusa_gpt_timer_start_internal(lisa_device_t *dev, uint8_t channel, uint32_t count,
                                           lisa_hwtimer_mode_t mode)
{
    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    venusa_gpt_channel_data_t *ch_data = &data->channels[channel];
    GPT_Channel_Num_t hal_channel = (GPT_Channel_Num_t)channel;

    if (!ch_data->callback) {
        LISA_LOGE(LOG_TAG, "Callback not set for channel %d, cannot start timer", channel);
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!ch_data->clk_configured || ch_data->frequency_hz == 0) {
        LISA_LOGE(LOG_TAG, "Frequency not set for channel %d, cannot start timer", channel);
        return LISA_DEVICE_ERR_INVALID;
    }

    if (count < VENUSA_GPT_TIMER_MIN_COUNT || count > VENUSA_GPT_TIMER_MAX_COUNT) {
        LISA_LOGE(LOG_TAG, "Invalid count %lu for channel %d", count, channel);
        return LISA_DEVICE_ERR_RANGE;
    }

    GPT_TIMER_Config_Para_t timer_cfg = {
        .index = VENUSA_GPT_TIMER_MODE,
        .run_mode = (mode == LISA_HWTIMER_MODE_ONESHOT) ? GPT_TIMER_RUNMODE_SINGLE : GPT_TIMER_RUNMODE_REPEAT,
        .cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
    };

    if (HAL_GPT_Control(data->gpt_handler, hal_channel, &ch_data->clk_cfg) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    if (HAL_GPT_TimerControl(data->gpt_handler, hal_channel, &timer_cfg) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    if (HAL_GPT_RegisterTimerCallback(data->gpt_handler, hal_channel, venusa_gpt_timer_event_callback, ch_data) !=
        CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    if (HAL_GPT_SetTimerPeriodByCount(data->gpt_handler, hal_channel, VENUSA_GPT_TIMER_MODE, (uint16_t)count) !=
        CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    if (HAL_GPT_StartTimer(data->gpt_handler, hal_channel, VENUSA_GPT_TIMER_MODE) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    ch_data->count = count;
    ch_data->mode = mode;
    ch_data->is_running = true;

    return LISA_DEVICE_OK;
}

/**
 * @brief 启动 GPT timer
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @param count 计数值
 * @param mode LISA timer 运行模式
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_RANGE 通道号或 count 无效
 */
static int venusa_gpt_timer_start(lisa_device_t *dev, uint8_t channel, uint32_t count, lisa_hwtimer_mode_t mode)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (channel >= VENUSA_GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    int ret = venusa_gpt_timer_start_internal(dev, channel, count, mode);

    DEVICE_UNLOCK(data);

    return ret;
}

/**
 * @brief 停止 GPT timer
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效或通道未运行
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 */
static int venusa_gpt_timer_stop(lisa_device_t *dev, uint8_t channel)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (channel >= VENUSA_GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    if (!data->channels[channel].is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    HAL_GPT_DisableChannel(data->gpt_handler, (GPT_Channel_Num_t)channel);
    data->channels[channel].is_running = false;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

/**
 * @brief 重置 GPT timer
 *
 * 通过停止当前 GPT channel 后复用保存的 count/mode 重新启动实现。
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效或通道未运行
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 */
static int venusa_gpt_timer_reset(lisa_device_t *dev, uint8_t channel)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (channel >= VENUSA_GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    venusa_gpt_channel_data_t *ch_data = &data->channels[channel];
    if (!ch_data->is_running) {
        DEVICE_UNLOCK(data);
        return LISA_DEVICE_ERR_INVALID;
    }

    HAL_GPT_DisableChannel(data->gpt_handler, (GPT_Channel_Num_t)channel);
    int ret = venusa_gpt_timer_start_internal(dev, channel, ch_data->count, ch_data->mode);

    DEVICE_UNLOCK(data);

    return ret;
}

/**
 * @brief 读取当前 GPT timer 计数值
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @param count 输出当前计数值
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 * @return LISA_DEVICE_ERR_IO HAL 读取失败
 */
static int venusa_gpt_timer_get_value(lisa_device_t *dev, uint8_t channel, uint32_t *count)
{
    if (!dev || !count) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (channel >= VENUSA_GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    uint16_t hal_count = 0;
    if (HAL_GPT_ReadTimerCount(data->gpt_handler, (GPT_Channel_Num_t)channel, VENUSA_GPT_TIMER_MODE, &hal_count) !=
        CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_IO;
    }

    *count = hal_count;
    return LISA_DEVICE_OK;
}

/**
 * @brief 设置 GPT timer 超时回调
 *
 * @param dev LISA 设备对象
 * @param channel LISA timer 通道号
 * @param callback 超时回调函数
 * @param user_data 用户数据
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 通道号无效
 */
static int venusa_gpt_timer_set_callback(lisa_device_t *dev, uint8_t channel, lisa_hwtimer_callback_t callback,
                                         void *user_data)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    venusa_gpt_timer_data_t *data = (venusa_gpt_timer_data_t *)dev->priv_data;
    if (!data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (channel >= VENUSA_GPT_TIMER_CHANNEL_COUNT) {
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(data);

    data->channels[channel].callback = callback;
    data->channels[channel].user_data = user_data;

    DEVICE_UNLOCK(data);

    return LISA_DEVICE_OK;
}

static const lisa_hwtimer_api_t venusa_gpt_timer_api = {
    .get_capabilities = venusa_gpt_timer_get_capabilities,
    .set_frequency = venusa_gpt_timer_set_frequency,
    .start = venusa_gpt_timer_start,
    .stop = venusa_gpt_timer_stop,
    .reset = venusa_gpt_timer_reset,
    .get_value = venusa_gpt_timer_get_value,
    .set_callback = venusa_gpt_timer_set_callback,
};

/**
 * @brief OS 资源初始化（mutex），仅 _init 阶段调用一次，跨 suspend/resume 保留
 *
 * @param priv GPT timer 私有数据
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INIT_FAIL mutex 创建失败
 */
static int venusa_gpt_timer_init_resources(venusa_gpt_timer_data_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 GPT HAL 硬件初始化
 *
 * 由 _init 调用；只动 HAL，不分配 mutex / 堆内存。唤醒后经 reinit 重新走本路径。
 *
 * @param priv GPT timer 私有数据
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INIT_FAIL HAL 初始化失败
 */
static int venusa_gpt_timer_init_hw(venusa_gpt_timer_data_t *priv)
{
    priv->gpt_handler = GPT0();
    if (!priv->gpt_handler) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (HAL_GPT_Initialize(priv->gpt_handler) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (HAL_GPT_PowerControl(priv->gpt_handler, CSK_POWER_FULL) != CSK_DRIVER_OK) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    for (uint32_t i = 0; i < VENUSA_GPT_TIMER_CHANNEL_COUNT; i++) {
        priv->channels[i].is_running = false;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief GPT Timer 初始化函数
 *
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_INIT_FAIL 初始化失败
 */
static int venusa_gpt_timer_init(void)
{
    int ret = venusa_gpt_timer_init_resources(&venusa_gpt_timer_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    return venusa_gpt_timer_init_hw(&venusa_gpt_timer_priv);
}

/**
 * @brief 停止并释放 GPT Timer 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 _init 申请相反：
 *   1) HAL 下电：先 HAL_GPT_PowerControl(OFF) 再 HAL_GPT_Uninitialize；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（各通道 is_running / 应用配置
 *      随之清零，强制唤醒后业务侧重新 start()）。
 *
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int venusa_gpt_timer_deinit(void)
{
    venusa_gpt_timer_data_t *priv = &venusa_gpt_timer_priv;

    if (priv->gpt_handler) {
        HAL_GPT_PowerControl(priv->gpt_handler, CSK_POWER_OFF);
        HAL_GPT_Uninitialize(priv->gpt_handler);
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&venusa_gpt_timer_priv, 0, sizeof(venusa_gpt_timer_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/**
 * @brief PM 空闲检查
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型，故不再需要
 * prepare_suspend / resume_restore；仅保留 check_idle：任一 GPT timer channel 正在
 * 运行时阻止系统自动进入轻睡，避免计数被关断。
 *
 * @param ctx GPT timer 私有数据
 * @return 1 可睡眠
 * @return 0 不可睡眠
 */
static int32_t venusa_gpt_timer_pm_check_idle(void *ctx)
{
    venusa_gpt_timer_data_t *priv = (venusa_gpt_timer_data_t *)ctx;
    if (priv == NULL) {
        return 1;
    }

    for (uint32_t i = 0; i < VENUSA_GPT_TIMER_CHANNEL_COUNT; i++) {
        if (priv->channels[i].is_running) {
            return 0;
        }
    }
    return 1;
}

static const lisa_pm_system_ops_t venusa_gpt_timer_pm_ops = {
    .check_idle = venusa_gpt_timer_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore = NULL,
};
#endif /* CONFIG_LISA_PM */


// clang-format off
LISA_DEVICE_REGISTER_DEINIT(gpt_timer, &venusa_gpt_timer_api, &venusa_gpt_timer_priv, NULL, venusa_gpt_timer_init,
                            venusa_gpt_timer_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(gpt_timer, &venusa_gpt_timer_pm_ops, NULL, &venusa_gpt_timer_priv);
#endif                        /* wakeup ops */
// clang-format on
