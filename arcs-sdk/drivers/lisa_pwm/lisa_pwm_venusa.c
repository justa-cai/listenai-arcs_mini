/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_pwm_venusa.c
 * @brief LISA PWM Venusa 平台适配层
 *
 * 此文件实现 Venusa 芯片平台的 PWM 硬件适配
 */

#include "lisa_pwm.h"

#include "ClockManager.h"
#include "Driver_GPT_Common.h"
#include "Driver_GPT_PWM.h"
#include "board.h"
#include "lisa_mutex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "lisa_pwm_venusa"
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

#define VENUSA_PWM_PORTS_PER_CHANNEL 4U   // venusa每通道4路pwm
#define VENUSA_PWM_GROUP_COUNT       GPT_NUMBER_OF_CHANNELS     // venusa 支持双通道
#define VENUSA_PWM_MAX_CHANNELS      (VENUSA_PWM_GROUP_COUNT * VENUSA_PWM_PORTS_PER_CHANNEL)
#define VENUSA_PWM_ALL_PORT_MASK     (GPT_PWM_PORT_0_MASK | GPT_PWM_PORT_1_MASK | GPT_PWM_PORT_2_MASK | GPT_PWM_PORT_3_MASK)

/* CSK70xx User Manual GPT 18.3: period_ticks = reload + 2. */
#define VENUSA_PWM_PERIOD_OFFSET     2U
#define VENUSA_PWM_MAX_PERIOD_TICKS  (UINT16_MAX + VENUSA_PWM_PERIOD_OFFSET)
#define VENUSA_PWM_MAX_PREDIV_FACTOR 65536U

/* ===== PWM 组级时钟配置信息 ===== */
typedef struct {
    GPT_Config_Para_t hal_config; /* GPT channel 时钟配置 */
    uint32_t tick_hz;             /* 分频后的计数频率 */
    bool configured;              /* 是否已保存有效配置 */
} pwm_group_info_t;

/* ===== PWM 通道配置信息 ===== */
typedef struct {
    uint32_t frequency_hz;        /* 频率 (Hz) */
    uint16_t period_count;        /* GPT reload count */
    uint8_t duty_cycle_percent;   /* 占空比百分比 (0-100) */
    lisa_pwm_mode_t mode;         /* 输出对齐模式 */
    lisa_pwm_polarity_t polarity; /* 输出极性 */
    bool configured;              /* 是否已配置 */
    bool enabled;                 /* 是否已启用 */
} pwm_channel_info_t;

/* ===== PWM 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                                    /* HAL GPT 句柄 */
    lisa_mutex_t *mutex;                                  /* 互斥锁 */
    pwm_channel_info_t channels[VENUSA_PWM_MAX_CHANNELS]; /* 通道配置信息 */
    pwm_group_info_t groups[VENUSA_PWM_GROUP_COUNT];      /* GPT channel 级时钟信息 */
    uint32_t max_channels;                                /* 最大通道数 */
} lisa_pwm_priv_t;

/* ===== PWM 设备静态实例 ===== */
static lisa_pwm_priv_t pwm0_priv;

/* ===== PWM 通道映射定义 ===== */
typedef struct {
    GPT_Channel_Num_t gpt_channel;   /* GPT channel */
    GPT_PWM_Port_t port;             /* GPT PWM port */
    GPT_PWM_PortBitMask_t port_mask; /* GPT PWM port bit mask */
} venusa_pwm_map_t;

/* LISA 逻辑 channel 0-3 属于 GPT0_CH0，4-7 属于 GPT0_CH1。 */
static const venusa_pwm_map_t venusa_pwm_map[VENUSA_PWM_MAX_CHANNELS] = {
    {GPT_CHANNEL_0, GPT_PWM_PORT_0, GPT_PWM_PORT_0_MASK}, {GPT_CHANNEL_0, GPT_PWM_PORT_1, GPT_PWM_PORT_1_MASK},
    {GPT_CHANNEL_0, GPT_PWM_PORT_2, GPT_PWM_PORT_2_MASK}, {GPT_CHANNEL_0, GPT_PWM_PORT_3, GPT_PWM_PORT_3_MASK},
    {GPT_CHANNEL_1, GPT_PWM_PORT_0, GPT_PWM_PORT_0_MASK}, {GPT_CHANNEL_1, GPT_PWM_PORT_1, GPT_PWM_PORT_1_MASK},
    {GPT_CHANNEL_1, GPT_PWM_PORT_2, GPT_PWM_PORT_2_MASK}, {GPT_CHANNEL_1, GPT_PWM_PORT_3, GPT_PWM_PORT_3_MASK},
};

/* ===== PWM 时钟分频定义 ===== */
typedef struct {
    GPT_Clk_Div_t hal_div; /* HAL 分频枚举 */
    uint32_t factor;       /* 实际分频系数 */
} venusa_pwm_clk_div_t;

/* GPT 支持的 1/2/4/8/16/32/64/128 分频，枚举值和实际分频系数分开保存，避免公式依赖 enum 数值。 */
static const venusa_pwm_clk_div_t venusa_pwm_clk_divs[] = {
    {GPT_CLK_DIV_1, 1U},   {GPT_CLK_DIV_2, 2U},   {GPT_CLK_DIV_4, 4U},   {GPT_CLK_DIV_8, 8U},
    {GPT_CLK_DIV_16, 16U}, {GPT_CLK_DIV_32, 32U}, {GPT_CLK_DIV_64, 64U}, {GPT_CLK_DIV_128, 128U},
};

/* 只使用芯片内部稳定时钟源；EXCLK 需要外部输入，当前驱动不自动选择。 */
static const GPT_Clk_Src_t venusa_pwm_clk_sources[] = {
    GPT_CLK_SRC_PCLK,
    GPT_CLK_SRC_T0,
};

/* ===== 内部辅助函数 ===== */

/**
 * @brief 检查 PWM 输出对齐模式是否有效
 *
 * Venusa 驱动当前接受 edge/center 两种 LISA mode，其中 center 会在上层保留配置，
 * 但实际输出仍由 HAL 默认边沿对齐实现。
 *
 * @param mode PWM 输出对齐模式
 * @return true 模式有效
 * @return false 模式无效
 */
static bool mode_is_valid(lisa_pwm_mode_t mode)
{
    return mode == LISA_PWM_MODE_EDGE_ALIGNED || mode == LISA_PWM_MODE_CENTER_ALIGNED;
}

/**
 * @brief 检查 PWM 输出极性是否有效
 *
 * 仅接受正常极性和反转极性，避免无效枚举写入 HAL 极性配置。
 *
 * @param polarity PWM 输出极性
 * @return true 极性有效
 * @return false 极性无效
 */
static bool polarity_is_valid(lisa_pwm_polarity_t polarity)
{
    return polarity == LISA_PWM_POLARITY_NORMAL || polarity == LISA_PWM_POLARITY_INVERTED;
}

/**
 * @brief 判断占空比是否为静态输出边界值
 *
 * 0% 和 100% 不走 GPT PWM 计数输出，而是通过关闭 PWM port 后保持 init_level
 * 实现静态电平，规避 datasheet 中边界占空比 shadow register 不生效的问题。
 *
 * @param duty_percent 占空比百分比
 * @return true 占空比为 0% 或 100%
 * @return false 占空比为 1%-99%
 */
static bool duty_is_boundary(uint8_t duty_percent)
{
    return duty_percent == 0 || duty_percent == LISA_PWM_DUTY_PERCENT_MAX;
}

/**
 * @brief 检查通道号有效性
 *
 * 根据设备私有数据中的 max_channels 检查逻辑 PWM channel 是否可访问，
 * 所有公开 API 入口在访问通道缓存前都应调用该函数。
 *
 * @param dev PWM 设备对象
 * @param channel PWM 通道号
 * @return LISA_DEVICE_OK 通道有效
 * @return LISA_DEVICE_ERR_INVALID 设备或私有数据无效
 * @return LISA_DEVICE_ERR_RANGE 通道号超出范围
 */
static int check_channel_valid(lisa_device_t *dev, uint32_t channel)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;
    if (channel >= priv->max_channels) {
        return LISA_DEVICE_ERR_RANGE;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 判断通道是否已经设置频率
 *
 * 通道只有在配置过基础属性且 frequency_hz 非 0 时，才认为具备可写入硬件的时序信息。
 *
 * @param info PWM 通道状态
 * @return true 通道已有时序配置
 * @return false 通道尚未设置频率
 */
static bool channel_has_timing_config(const pwm_channel_info_t *info)
{
    return info && info->configured && info->frequency_hz != 0;
}

/**
 * @brief 判断通道是否需要 GPT PWM 计数输出
 *
 * 1%-99% duty 需要 GPT PWM 计数器产生波形；0%/100% duty 走静态电平路径，
 * 不占用 GPT channel 的 PWM port enable bit。
 *
 * @param info PWM 通道状态
 * @return true 需要硬件 PWM 计数输出
 * @return false 不需要硬件 PWM 计数输出
 */
static bool channel_wants_pwm_counter(const pwm_channel_info_t *info)
{
    /* 0%/100% 走静态电平路径，不占用 GPT PWM 计数输出。 */
    return channel_has_timing_config(info) && !duty_is_boundary(info->duty_cycle_percent);
}

/**
 * @brief 判断通道是否正在使用硬件 PWM 输出
 *
 * 该状态同时要求逻辑 enabled=true 且 duty 为 1%-99%，用于统计同组中真正占用
 * GPT channel clock/reload 的 port。
 *
 * @param info PWM 通道状态
 * @return true 通道正在硬件输出 PWM 波形
 * @return false 通道未占用硬件 PWM 输出
 */
static bool channel_pwm_is_hardware_enabled(const pwm_channel_info_t *info)
{
    return channel_wants_pwm_counter(info) && info->enabled;
}

/**
 * @brief 获取通道所在 group 的起始逻辑通道号
 *
 * Venusa 每 4 个 LISA channel 映射到同一个 GPT channel，因此 group base 用于
 * 遍历同组 port 和计算同组 enable mask。
 *
 * @param channel PWM 通道号
 * @return uint32_t group 起始通道号
 */
static uint32_t channel_group_base(uint32_t channel)
{
    return (channel / VENUSA_PWM_PORTS_PER_CHANNEL) * VENUSA_PWM_PORTS_PER_CHANNEL;
}

/**
 * @brief 获取通道所在 group index
 *
 * group index 与 GPT channel 编号一一对应，0 表示逻辑通道 0-3，1 表示逻辑通道 4-7。
 *
 * @param channel PWM 通道号
 * @return uint32_t group index
 */
static uint32_t channel_group_index(uint32_t channel)
{
    return channel / VENUSA_PWM_PORTS_PER_CHANNEL;
}

/**
 * @brief 获取通道所在 group 的结束逻辑通道号
 *
 * 返回值为左闭右开区间的结束位置，便于 for 循环遍历同组通道。
 *
 * @param channel PWM 通道号
 * @return uint32_t group 结束通道号
 */
static uint32_t channel_group_end(uint32_t channel)
{
    return channel_group_base(channel) + VENUSA_PWM_PORTS_PER_CHANNEL;
}

/**
 * @brief 32 位四舍五入除法
 *
 * 用于把 tick_hz/frequency_hz 转换为最接近目标频率的整数周期 tick 数。
 *
 * @param numerator 被除数
 * @param denominator 除数
 * @return uint32_t 四舍五入后的商
 */
static uint32_t round_div_u32(uint32_t numerator, uint32_t denominator)
{
    return (numerator + denominator / 2U) / denominator;
}

/**
 * @brief 64 位向上取整除法
 *
 * 用于根据 16-bit reload 上限反推最小 prediv，避免中间乘法溢出 32 位。
 *
 * @param numerator 被除数
 * @param denominator 除数
 * @return uint32_t 向上取整后的商
 */
static uint32_t ceil_div_u64(uint64_t numerator, uint64_t denominator)
{
    return (uint32_t)((numerator + denominator - 1U) / denominator);
}

/**
 * @brief 获取 GPT 时钟源当前频率
 *
 * 从 ClockManager 读取运行时 PCLK/T0 频率，避免假设系统时钟固定；EXCLK 需要外部输入，
 * 当前驱动不自动选择，返回 0。
 *
 * @param clk_src GPT 时钟源
 * @return uint32_t 时钟源频率，单位 Hz；0 表示不可用
 */
static uint32_t pwm_source_frequency_hz(GPT_Clk_Src_t clk_src)
{
    /* 从 ClockManager 读取运行时频率，避免假设 PCLK/T0 固定。 */
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
 * @brief 根据计数频率和目标频率计算 GPT reload count
 *
 * Venusa GPT PWM 周期公式为 period_ticks = reload + 2，本函数先四舍五入计算
 * period_ticks，再转换成 HAL_GPT_SetPWMFrequence() 实际写入的 reload count。
 *
 * @param tick_hz GPT channel 分频后的计数频率
 * @param frequency_hz 目标 PWM 频率
 * @param period_count 输出参数，返回 GPT reload count
 * @return LISA_DEVICE_OK 计算成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 目标频率无法用 16-bit reload 表示
 */
static int period_count_from_tick(uint32_t tick_hz, uint32_t frequency_hz, uint16_t *period_count)
{
    if (!period_count || tick_hz == 0 || frequency_hz == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* Datasheet 周期公式为 period_ticks = reload + 2，HAL_GPT_SetPWMFrequence() 实际写 reload。 */
    uint32_t period_ticks = round_div_u32(tick_hz, frequency_hz);
    if (period_ticks < VENUSA_PWM_PERIOD_OFFSET || period_ticks > VENUSA_PWM_MAX_PERIOD_TICKS) {
        return LISA_DEVICE_ERR_RANGE;
    }

    *period_count = (uint16_t)(period_ticks - VENUSA_PWM_PERIOD_OFFSET);
    return LISA_DEVICE_OK;
}

/**
 * @brief 比较两组 GPT channel 时钟配置是否相同
 *
 * 判断目标 group 配置是否和当前硬件缓存一致，用于决定是否需要重新配置
 * GPT channel clock/prediv/divider。
 *
 * @param lhs 左侧 group 时钟配置
 * @param rhs 右侧 group 时钟配置
 * @return true 两组配置相同
 * @return false 两组配置不同
 */
static bool group_info_is_same(const pwm_group_info_t *lhs, const pwm_group_info_t *rhs)
{
    return lhs && rhs && lhs->configured == rhs->configured && lhs->tick_hz == rhs->tick_hz &&
           lhs->hal_config.clk_src == rhs->hal_config.clk_src && lhs->hal_config.prediv == rhs->hal_config.prediv &&
           lhs->hal_config.clk_div == rhs->hal_config.clk_div;
}

/**
 * @brief 为空闲 GPT group 选择一组最优时钟和 reload 配置
 *
 * 遍历 PCLK/T0、GPT divider 和 prediv，选择能够满足目标频率且 tick_hz 最高的配置，
 * 以尽量提高 duty 分辨率。该函数只计算目标配置，不直接写硬件。
 *
 * @param frequency_hz 目标 PWM 频率
 * @param group 输出参数，返回选中的 GPT group 时钟配置
 * @param period_count 输出参数，返回目标频率对应的 GPT reload count
 * @return LISA_DEVICE_OK 找到可用配置
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 目标频率不支持
 */
static int select_best_group_timing(uint32_t frequency_hz, pwm_group_info_t *group, uint16_t *period_count)
{
    if (!group || !period_count || frequency_hz == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    bool found = false;
    pwm_group_info_t best_group = {0};
    uint16_t best_period = 0;

    for (uint32_t src_idx = 0; src_idx < sizeof(venusa_pwm_clk_sources) / sizeof(venusa_pwm_clk_sources[0]);
         src_idx++) {
        GPT_Clk_Src_t clk_src = venusa_pwm_clk_sources[src_idx];
        uint32_t src_hz = pwm_source_frequency_hz(clk_src);
        if (src_hz == 0) {
            continue;
        }

        for (uint32_t div_idx = 0; div_idx < sizeof(venusa_pwm_clk_divs) / sizeof(venusa_pwm_clk_divs[0]); div_idx++) {
            uint32_t base_hz = src_hz / venusa_pwm_clk_divs[div_idx].factor;
            if (base_hz == 0) {
                continue;
            }

            /*
             * 先用 16-bit reload 上限反推最小 prediv，再少量试探相邻值；
             * 最终选择满足目标频率的最高 tick_hz，以获得更好的占空比分辨率。
             */
            uint32_t prediv_factor = ceil_div_u64(base_hz, (uint64_t)frequency_hz * VENUSA_PWM_MAX_PERIOD_TICKS);
            if (prediv_factor == 0) {
                prediv_factor = 1;
            }

            for (uint32_t candidate = prediv_factor; candidate <= prediv_factor + 2U; candidate++) {
                if (candidate == 0 || candidate > VENUSA_PWM_MAX_PREDIV_FACTOR) {
                    break;
                }

                uint32_t tick_hz = base_hz / candidate;
                uint16_t reload = 0;
                if (period_count_from_tick(tick_hz, frequency_hz, &reload) != LISA_DEVICE_OK) {
                    continue;
                }

                /* GPT prediv 寄存器语义为实际分频 factor - 1。 */
                if (!found || tick_hz > best_group.tick_hz) {
                    best_group.hal_config.clk_src = clk_src;
                    best_group.hal_config.prediv = (uint16_t)(candidate - 1U);
                    best_group.hal_config.clk_div = venusa_pwm_clk_divs[div_idx].hal_div;
                    best_group.tick_hz = tick_hz;
                    best_group.configured = true;
                    best_period = reload;
                    found = true;
                }
            }
        }
    }

    if (!found) {
        LISA_LOGE(LOG_TAG, "Unsupported PWM frequency %lu Hz", frequency_hz);
        return LISA_DEVICE_ERR_RANGE;
    }

    *group = best_group;
    *period_count = best_period;
    LISA_LOGD(LOG_TAG, "Selected PWM timing: freq=%lu Hz, clk_src=%d, prediv=%u, clk_div=%d, tick=%lu Hz, reload=%u",
              frequency_hz, (int)group->hal_config.clk_src, group->hal_config.prediv, (int)group->hal_config.clk_div,
              group->tick_hz, *period_count);
    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化阶段选择一个可用默认时钟
 *
 * PWM 初始化时先给每个 GPT channel 写入一个可用 clock，后续 lisa_pwm_set()/enable()
 * 会根据目标频率重新选择更合适的 group 配置。
 *
 * @param group 输出参数，返回默认 GPT group 时钟配置
 * @return LISA_DEVICE_OK 找到可用默认时钟
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_INIT_FAIL 无可用 GPT 时钟源
 */
static int select_default_group_clock(pwm_group_info_t *group)
{
    if (!group) {
        return LISA_DEVICE_ERR_INVALID;
    }

    for (uint32_t i = 0; i < sizeof(venusa_pwm_clk_sources) / sizeof(venusa_pwm_clk_sources[0]); i++) {
        uint32_t src_hz = pwm_source_frequency_hz(venusa_pwm_clk_sources[i]);
        if (src_hz == 0) {
            continue;
        }

        group->hal_config.clk_src = venusa_pwm_clk_sources[i];
        group->hal_config.prediv = 0;
        group->hal_config.clk_div = GPT_CLK_DIV_1;
        group->tick_hz = src_hz;
        group->configured = true;
        return LISA_DEVICE_OK;
    }

    LISA_LOGE(LOG_TAG, "No valid GPT clock source for PWM");
    return LISA_DEVICE_ERR_INIT_FAIL;
}

/**
 * @brief 将占空比百分比转换为 GPT duty count
 *
 * duty 计算同样基于完整周期 period_ticks = reload + 2，最后转换为硬件寄存器
 * 需要的 high_ticks - 1 形式。
 *
 * @param period_count GPT reload count
 * @param duty_percent 占空比百分比
 * @return uint16_t GPT duty high count
 */
static uint16_t duty_percent_to_count(uint16_t period_count, uint8_t duty_percent)
{
    /* duty high count 也按 reload + 2 的完整周期换算，再转回硬件需要的 count - 1。 */
    uint32_t period_ticks = (uint32_t)period_count + VENUSA_PWM_PERIOD_OFFSET;
    uint32_t high_ticks = (period_ticks * duty_percent + LISA_PWM_DUTY_PERCENT_MAX / 2U) / LISA_PWM_DUTY_PERCENT_MAX;

    if (high_ticks == 0) {
        high_ticks = 1;
    } else if (high_ticks > period_ticks) {
        high_ticks = period_ticks;
    }

    return (uint16_t)(high_ticks - 1U);
}

/**
 * @brief 将 LISA PWM 极性转换为 HAL 输出极性
 *
 * 正常极性映射为 active high，反转极性映射为 active low。
 *
 * @param polarity LISA PWM 输出极性
 * @return GPT_PWM_OutputPolarity_t HAL 输出极性
 */
static GPT_PWM_OutputPolarity_t polarity_to_hal(lisa_pwm_polarity_t polarity)
{
    return polarity == LISA_PWM_POLARITY_INVERTED ? GPT_PWM_OUTPUT_ACTIVE_LOW : GPT_PWM_OUTPUT_ACTIVE_HIGH;
}

/**
 * @brief 根据极性和边界占空比计算 HAL 初始电平
 *
 * 常规 PWM 输出时 init_level 用于端口启用前的空闲态；0%/100% 静态输出时，
 * 驱动关闭 PWM port，只靠 init_level 输出目标电平。
 *
 * @param info PWM 通道状态
 * @return GPT_PWM_InitPolarity_t HAL 初始电平配置
 */
static GPT_PWM_InitPolarity_t init_level_to_hal(const pwm_channel_info_t *info)
{
    /*
     * 常规 PWM 输出时 init_level 用于端口启用前的空闲态；
     * 0%/100% 静态输出时，驱动关闭 PWM port，只靠 init_level 输出目标电平。
     */
    bool output_high = info->polarity == LISA_PWM_POLARITY_INVERTED;

    if (duty_is_boundary(info->duty_cycle_percent)) {
        if (info->polarity == LISA_PWM_POLARITY_NORMAL) {
            output_high = info->duty_cycle_percent == LISA_PWM_DUTY_PERCENT_MAX;
        } else {
            output_high = info->duty_cycle_percent == 0;
        }
    }

    return output_high ? GPT_PWM_INIT_LEVEL_HIGH : GPT_PWM_INIT_LEVEL_LOW;
}

/**
 * @brief 获取 group 内正在硬件输出的 PWM port mask
 *
 * 只统计 1%-99% 且逻辑 enabled 的通道，静态 0%/100% 通道不需要重新 enable PWM port。
 *
 * @param priv PWM 设备私有数据
 * @param group_base group 起始逻辑通道号
 * @return GPT_PWM_PortBitMask_t 正在硬件输出的 port mask
 */
static GPT_PWM_PortBitMask_t group_enabled_mask(lisa_pwm_priv_t *priv, uint32_t group_base)
{
    uint32_t mask = 0;

    /* 只统计真正运行 GPT 计数器的 port，静态 0%/100% 不需要重新 enable PWM。 */
    for (uint32_t i = group_base; i < group_base + VENUSA_PWM_PORTS_PER_CHANNEL; i++) {
        if (channel_pwm_is_hardware_enabled(&priv->channels[i])) {
            mask |= venusa_pwm_map[i].port_mask;
        }
    }

    return (GPT_PWM_PortBitMask_t)mask;
}

/**
 * @brief 获取应用目标状态后的 group PWM port mask
 *
 * 在不立即修改软件缓存的情况下，把目标通道状态临时代入，计算硬件配置完成后
 * 应恢复 enable 的同组 port mask。
 *
 * @param priv PWM 设备私有数据
 * @param group_base group 起始逻辑通道号
 * @param target_channel 正在应用新状态的 PWM 通道号
 * @param target_info 目标通道状态
 * @return GPT_PWM_PortBitMask_t 应启用的 port mask
 */
static GPT_PWM_PortBitMask_t group_enabled_mask_with_target(lisa_pwm_priv_t *priv, uint32_t group_base,
                                                            uint32_t target_channel,
                                                            const pwm_channel_info_t *target_info)
{
    uint32_t mask = 0;

    for (uint32_t i = group_base; i < group_base + VENUSA_PWM_PORTS_PER_CHANNEL; i++) {
        const pwm_channel_info_t *info = (i == target_channel) ? target_info : &priv->channels[i];
        if (channel_pwm_is_hardware_enabled(info)) {
            mask |= venusa_pwm_map[i].port_mask;
        }
    }

    return (GPT_PWM_PortBitMask_t)mask;
}

/**
 * @brief 获取 group 内除目标通道外正在硬件输出的 PWM port mask
 *
 * 用于判断同组是否已有其它 port 正在运行，以及配置完成后是否需要恢复 peer port。
 *
 * @param priv PWM 设备私有数据
 * @param group_base group 起始逻辑通道号
 * @param target_channel 需要排除的目标通道号
 * @return GPT_PWM_PortBitMask_t peer port mask
 */
static GPT_PWM_PortBitMask_t group_enabled_peer_mask(lisa_pwm_priv_t *priv, uint32_t group_base,
                                                     uint32_t target_channel)
{
    uint32_t mask = 0;

    for (uint32_t i = group_base; i < group_base + VENUSA_PWM_PORTS_PER_CHANNEL; i++) {
        if (i != target_channel && channel_pwm_is_hardware_enabled(&priv->channels[i])) {
            mask |= venusa_pwm_map[i].port_mask;
        }
    }

    return (GPT_PWM_PortBitMask_t)mask;
}

/**
 * @brief 使能 group 内指定 PWM port mask
 *
 * 对非 0 mask 调用 HAL_GPT_EnablePWM()。mask 为 0 时直接返回成功，便于统一处理
 * 静态输出或无 peer 的场景。
 *
 * @param priv PWM 设备私有数据
 * @param group_base group 起始逻辑通道号
 * @param mask 需要使能的 PWM port mask
 * @return LISA_DEVICE_OK 使能成功或无需使能
 * @return LISA_DEVICE_ERR_IO HAL 使能失败
 */
static int enable_group_mask(lisa_pwm_priv_t *priv, uint32_t group_base, GPT_PWM_PortBitMask_t mask)
{
    if (mask == 0) {
        return LISA_DEVICE_OK;
    }

    const venusa_pwm_map_t *map = &venusa_pwm_map[group_base];
    if (HAL_GPT_EnablePWM(priv->hal_handler, map->gpt_channel, mask) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to enable PWM group %lu mask 0x%x", group_base, mask);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 查找 group 内当前 reload 的拥有者
 */
static const pwm_channel_info_t *find_group_period_owner(lisa_pwm_priv_t *priv, uint32_t group_base)
{
    /* 同一个 GPT channel 下所有 port 共享 reload，任一运行中的 PWM port 都可以代表当前周期。 */
    for (uint32_t i = group_base; i < group_base + VENUSA_PWM_PORTS_PER_CHANNEL; i++) {
        if (channel_pwm_is_hardware_enabled(&priv->channels[i])) {
            return &priv->channels[i];
        }
    }

    return NULL;
}

/**
 * @brief 查找同组正在硬件输出的其它 PWM 通道
 */
static const pwm_channel_info_t *find_group_hardware_peer(lisa_pwm_priv_t *priv, uint32_t channel)
{
    uint32_t start = channel_group_base(channel);
    uint32_t end = channel_group_end(channel);

    for (uint32_t i = start; i < end; i++) {
        /* i != channel 是为了“只找同组其它通道”，排除当前正在操作的目标通道自己 */
        if (i != channel && channel_pwm_is_hardware_enabled(&priv->channels[i])) {
            return &priv->channels[i];
        }
    }

    return NULL;
}

/**
 * @brief 根据同组 port 当前状态选择目标 group 时序
 *
 * 同组已有 1%-99% PWM 输出时，新的 PWM 输出必须复用当前 tick_hz/reload；
 * 若同组只有当前 port 或只有 0%/100% 静态输出，则允许重新选择 group clock/reload。
 *
 * @param priv PWM 设备私有数据
 * @param channel 目标 PWM 通道号
 * @param frequency_hz 目标 PWM 频率
 * @param target_wants_pwm_counter 目标状态是否需要硬件 PWM 计数输出
 * @param group 输出参数，返回目标 group 时钟配置
 * @param period_count 输出参数，返回目标 GPT reload count
 * @return LISA_DEVICE_OK 选择成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 目标频率超出范围
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 同组活动 PWM 无法复用该频率
 * @return LISA_DEVICE_ERR_NOT_READY group 状态异常
 */
static int select_group_timing(lisa_pwm_priv_t *priv, uint32_t channel, uint32_t frequency_hz,
                               bool target_wants_pwm_counter, pwm_group_info_t *group, uint16_t *period_count)
{
    uint32_t group_idx = channel_group_index(channel);
    const pwm_channel_info_t *peer = find_group_hardware_peer(priv, channel);

    if (peer) {
        /*
         * 同组已有 1%-99% PWM 输出时，不能重新配置组级 clock/reload；
         * 新的 PWM 输出必须能复用当前 tick_hz 和 reload，否则返回 NOT_SUPPORT。
         */
        if (!priv->groups[group_idx].configured) {
            LISA_LOGE(LOG_TAG, "PWM group %lu has active port but no saved clock config", group_idx);
            return LISA_DEVICE_ERR_NOT_READY;
        }

        *group = priv->groups[group_idx];
        int ret = period_count_from_tick(group->tick_hz, frequency_hz, period_count);
        if (target_wants_pwm_counter) {
            if (ret != LISA_DEVICE_OK || *period_count != peer->period_count) {
                uint32_t start = channel_group_base(channel);
                uint32_t end = channel_group_end(channel);
                LISA_LOGE(LOG_TAG,
                          "PWM channels %lu-%lu share one GPT period; requested %lu Hz conflicts with "
                          "active %lu Hz",
                          start, end - 1, frequency_hz, peer->frequency_hz);
                return LISA_DEVICE_ERR_NOT_SUPPORT;
            }
            LISA_LOGD(LOG_TAG, "Reuse PWM group %lu timing for channel %lu: freq=%lu Hz, reload=%u", group_idx, channel,
                      frequency_hz, *period_count);
            return LISA_DEVICE_OK;
        }

        if (ret == LISA_DEVICE_OK) {
            LISA_LOGD(LOG_TAG, "Keep active PWM group %lu timing for static channel %lu", group_idx, channel);
            return LISA_DEVICE_OK;
        }

        /*
         * 目标为 0%/100% 静态输出时，不改变同组正在运行的 clock/reload；
         * 这里只校验用户给的 frequency_hz 本身是否在驱动可支持范围内。
         */
        pwm_group_info_t unused_group = {0};
        uint16_t unused_period = 0;
        ret = select_best_group_timing(frequency_hz, &unused_group, &unused_period);
        if (ret != LISA_DEVICE_OK) {
            return ret;
        }
        *period_count = 0;
        return LISA_DEVICE_OK;
    }

    return select_best_group_timing(frequency_hz, group, period_count);
}

/**
 * @brief 配置单个 PWM port 的初始电平和有效极性
 *
 * 该函数只配置 port 级 init_level/output_polarity。Venusa HAL_GPT_PWMControl()
 * 内部会 disable 整个 GPT channel，调用方需要负责恢复同组 enable mask。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 需要应用的通道状态
 * @return LISA_DEVICE_OK 配置成功
 * @return LISA_DEVICE_ERR_IO HAL 配置失败
 */
static int apply_pwm_config(lisa_pwm_priv_t *priv, uint32_t channel, const pwm_channel_info_t *info)
{
    const venusa_pwm_map_t *map = &venusa_pwm_map[channel];
    GPT_PWM_Config_t config = {
        .init_level = init_level_to_hal(info),
        .output_polarity = polarity_to_hal(info->polarity),
    };

    if (HAL_GPT_PWMControl(priv->hal_handler, map->gpt_channel, map->port, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to configure PWM channel %lu", channel);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 配置 GPT channel 级时钟参数
 *
 * HAL_GPT_Control() 配置的是 GPT channel 级 clock/prediv/divider，同组 4 个 PWM port
 * 会一起受影响，因此调用前需要确认没有不可中断的同组 PWM 输出。
 *
 * @param priv PWM 设备私有数据
 * @param group_base group 起始逻辑通道号
 * @param group 需要应用的 GPT group 时钟配置
 * @return LISA_DEVICE_OK 配置成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_IO HAL 配置失败
 */
static int apply_group_clock(lisa_pwm_priv_t *priv, uint32_t group_base, const pwm_group_info_t *group)
{
    if (!group || !group->configured) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* HAL_GPT_Control() 配置的是 GPT channel 级时钟，同组 4 个 PWM port 会一起受影响。 */
    const venusa_pwm_map_t *map = &venusa_pwm_map[group_base];
    GPT_Config_Para_t config = group->hal_config;

    if (HAL_GPT_Control(priv->hal_handler, map->gpt_channel, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to configure PWM group %lu clock", group_base);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGD(LOG_TAG, "Configured PWM group %lu clock: clk_src=%d, prediv=%u, clk_div=%d, tick=%lu Hz", group_base,
              (int)group->hal_config.clk_src, group->hal_config.prediv, (int)group->hal_config.clk_div, group->tick_hz);
    return LISA_DEVICE_OK;
}

/**
 * @brief 写入组级 reload 和当前 port 的 duty count
 *
 * HAL_GPT_SetPWMFrequence() 名称中的 frequency 实际是 GPT reload count；
 * reload 属于 GPT channel 级资源，duty count 属于 PWM port 级资源。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 需要应用的通道状态
 * @return LISA_DEVICE_OK 配置成功
 * @return LISA_DEVICE_ERR_IO HAL 配置失败
 */
static int apply_pwm_period_and_duty(lisa_pwm_priv_t *priv, uint32_t channel, const pwm_channel_info_t *info)
{
    const venusa_pwm_map_t *map = &venusa_pwm_map[channel];
    uint16_t duty_count = duty_percent_to_count(info->period_count, info->duty_cycle_percent);

    /* HAL API 名称是 Frequence，但参数实际是 GPT reload count。 */
    if (HAL_GPT_SetPWMFrequence(priv->hal_handler, map->gpt_channel, info->period_count) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set PWM period count %u", info->period_count);
        return LISA_DEVICE_ERR_IO;
    }

    if (HAL_GPT_SetPWMDuty(priv->hal_handler, map->gpt_channel, map->port, duty_count) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set PWM duty count %u", duty_count);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGD(LOG_TAG, "Configured PWM channel %lu: freq=%lu Hz, duty=%u%%, reload=%u, duty_count=%u", channel,
              info->frequency_hz, info->duty_cycle_percent, info->period_count, duty_count);
    return LISA_DEVICE_OK;
}

/**
 * @brief 尝试恢复通道原有硬件配置
 *
 * configure/set/enable 过程中若目标 HAL 配置失败，使用该函数把硬件恢复到旧缓存描述的状态，
 * 避免后续 enable/set 基于旧缓存时遇到硬件残留的新配置。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param old_info 需要恢复的旧通道状态
 * @param old_group 需要恢复的旧 group 时钟配置
 */
static void restore_channel_hardware(lisa_pwm_priv_t *priv, uint32_t channel, const pwm_channel_info_t *old_info,
                                     const pwm_group_info_t *old_group)
{
    uint32_t group_base = channel_group_base(channel);

    /* 硬件配置失败后尽量回滚到调用前状态，避免同组其它 PWM port 被停住。 */
    if (old_group && old_group->configured) {
        if (apply_group_clock(priv, group_base, old_group) != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Failed to restore PWM group %lu clock", group_base);
        }
    }

    if (old_info->configured) {
        if (apply_pwm_config(priv, channel, old_info) != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Failed to restore PWM channel %lu config", channel);
        }
    }

    const pwm_channel_info_t *period_owner = channel_pwm_is_hardware_enabled(old_info) ? old_info : NULL;
    if (!period_owner) {
        period_owner = find_group_period_owner(priv, group_base);
    }
    if (period_owner) {
        /* reload 属于组级资源，优先恢复旧目标通道，否则用同组仍在运行的 port。 */
        const venusa_pwm_map_t *map = &venusa_pwm_map[group_base];
        if (HAL_GPT_SetPWMFrequence(priv->hal_handler, map->gpt_channel, period_owner->period_count) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to restore PWM group %lu period", group_base);
        }
    }

    if (channel_pwm_is_hardware_enabled(old_info)) {
        const venusa_pwm_map_t *map = &venusa_pwm_map[channel];
        uint16_t duty_count = duty_percent_to_count(old_info->period_count, old_info->duty_cycle_percent);
        if (HAL_GPT_SetPWMDuty(priv->hal_handler, map->gpt_channel, map->port, duty_count) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to restore PWM channel %lu duty", channel);
        }
    }

    if (enable_group_mask(priv, group_base, group_enabled_mask(priv, group_base)) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to restore PWM group %lu enable state", group_base);
    }
}

/**
 * @brief 将目标通道状态应用到 Venusa GPT/PWM 硬件
 *
 * 该函数集中处理 group clock 重配、port 极性配置、reload/duty 写入、静态输出切换和
 * 同组 enable mask 恢复。失败时会调用 restore_channel_hardware() 尽量回滚。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param old_info 调用前的软件缓存通道状态
 * @param target_info 需要应用的目标通道状态
 * @param target_group 需要应用的目标 group 时钟配置
 * @return LISA_DEVICE_OK 应用成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_IO HAL 操作失败
 */
static int apply_configured_channel_state(lisa_pwm_priv_t *priv, uint32_t channel, const pwm_channel_info_t *old_info,
                                          const pwm_channel_info_t *target_info, const pwm_group_info_t *target_group)
{
    uint32_t group_base = channel_group_base(channel);
    uint32_t group_idx = channel_group_index(channel);
    pwm_group_info_t old_group = priv->groups[group_idx];
    GPT_PWM_PortBitMask_t peer_mask = group_enabled_peer_mask(priv, group_base, channel);
    bool need_config = !old_info->configured || old_info->polarity != target_info->polarity ||
                       old_info->mode != target_info->mode || duty_is_boundary(old_info->duty_cycle_percent) ||
                       duty_is_boundary(target_info->duty_cycle_percent);
    bool old_hw_enabled = channel_pwm_is_hardware_enabled(old_info);
    bool target_hw_enabled = channel_pwm_is_hardware_enabled(target_info);
    bool need_group_clock = !group_info_is_same(&old_group, target_group);
    int ret;

    if (need_group_clock && group_enabled_mask(priv, group_base) != 0) {
        /* 改组级 clock 前必须停掉同组 PWM port，否则 reload/计数时钟切换会影响正在输出的波形。 */
        const venusa_pwm_map_t *map = &venusa_pwm_map[group_base];
        if (HAL_GPT_DisablePWM(priv->hal_handler, map->gpt_channel, group_enabled_mask(priv, group_base)) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to disable PWM group %lu for clock reconfiguration", group_base);
            return LISA_DEVICE_ERR_IO;
        }
    }

    if (need_group_clock) {
        ret = apply_group_clock(priv, group_base, target_group);
        if (ret != LISA_DEVICE_OK) {
            restore_channel_hardware(priv, channel, old_info, &old_group);
            return ret;
        }
    }

    if (need_config) {
        /* HAL_GPT_PWMControl() 内部会 disable 整个 GPT channel，后面统一恢复 enable mask。 */
        ret = apply_pwm_config(priv, channel, target_info);
        if (ret != LISA_DEVICE_OK) {
            restore_channel_hardware(priv, channel, old_info, &old_group);
            return ret;
        }
    }

    if (channel_has_timing_config(target_info) && !duty_is_boundary(target_info->duty_cycle_percent) &&
        (target_info->enabled || peer_mask == 0)) {
        /* 未 enable 的首个 port 也提前写入 reload/duty，后续 enable 时即可输出；有 peer 时只更新目标 duty。 */
        ret = apply_pwm_period_and_duty(priv, channel, target_info);
        if (ret != LISA_DEVICE_OK) {
            restore_channel_hardware(priv, channel, old_info, &old_group);
            return ret;
        }
    }

    if (old_hw_enabled && !target_hw_enabled) {
        /* 从 1%-99% PWM 切到 0%/100% 静态电平时，关闭该 port 的 PWM 计数输出。 */
        const venusa_pwm_map_t *map = &venusa_pwm_map[channel];
        if (HAL_GPT_DisablePWM(priv->hal_handler, map->gpt_channel, map->port_mask) != 0) {
            restore_channel_hardware(priv, channel, old_info, &old_group);
            LISA_LOGE(LOG_TAG, "Failed to disable PWM channel %lu for static output", channel);
            return LISA_DEVICE_ERR_IO;
        }
    }

    ret = enable_group_mask(priv, group_base, group_enabled_mask_with_target(priv, group_base, channel, target_info));
    if (ret != LISA_DEVICE_OK) {
        restore_channel_hardware(priv, channel, old_info, &old_group);
        return ret;
    }

    return LISA_DEVICE_OK;
}

/* ===== Venusa平台PWM实现函数 ===== */

/**
 * @brief 配置 PWM 通道属性
 *
 * 更新指定通道的输出模式和极性。如果通道已经设置过频率，则同步更新硬件配置；
 * 若通道尚未设置频率，只更新软件缓存等待后续 set/enable。
 *
 * @param dev PWM 设备对象
 * @param channel PWM 通道号
 * @param config PWM 通道配置
 * @return LISA_DEVICE_OK 配置成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 通道号超出范围
 * @return LISA_DEVICE_ERR_IO HAL 配置失败
 */
static int venusa_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!mode_is_valid(config->mode) || !polarity_is_valid(config->polarity)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_channel_valid(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    pwm_channel_info_t old_info = priv->channels[channel];
    pwm_channel_info_t next = priv->channels[channel];
    next.mode = config->mode;
    next.polarity = config->polarity;
    next.configured = true;
    next.enabled = old_info.enabled;

    if (config->mode == LISA_PWM_MODE_CENTER_ALIGNED) {
        LISA_LOGW(LOG_TAG, "Venusa GPT PWM has no alignment selector; using hardware default edge-aligned output");
    }

    if (!channel_has_timing_config(&old_info)) {
        priv->channels[channel] = next;
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    pwm_group_info_t target_group = {0};
    ret = select_group_timing(priv, channel, next.frequency_hz, channel_wants_pwm_counter(&next), &target_group,
                              &next.period_count);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    ret = apply_configured_channel_state(priv, channel, &old_info, &next, &target_group);
    if (ret == LISA_DEVICE_OK) {
        priv->channels[channel] = next;
        priv->groups[channel_group_index(channel)] = target_group;
        LISA_LOGD(LOG_TAG, "Configured PWM channel %lu: mode=%d, polarity=%d", channel, (int)next.mode,
                  (int)next.polarity);
    }
    DEVICE_UNLOCK(priv);

    return ret;
}

/**
 * @brief 获取 PWM 通道属性
 *
 * 从软件缓存读取指定通道当前保存的输出模式和极性。
 *
 * @param dev PWM 设备对象
 * @param channel PWM 通道号
 * @param config 输出参数，返回 PWM 通道配置
 * @return LISA_DEVICE_OK 获取成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 通道号超出范围
 */
static int venusa_pwm_get_config(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_channel_valid(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);
    config->mode = priv->channels[channel].mode;
    config->polarity = priv->channels[channel].polarity;
    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置 PWM 通道频率和占空比
 *
 * 根据目标频率和 duty 更新通道软件缓存，并执行 Venusa group 级时序仲裁。
 * 同组已有活动 PWM 时，新通道必须复用同一组 clock/reload。
 *
 * @param dev PWM 设备对象
 * @param channel PWM 通道号
 * @param frequency_hz 目标 PWM 频率
 * @param duty_cycle_percent 占空比百分比
 * @return LISA_DEVICE_OK 设置成功
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_RANGE 通道号或频率超出范围
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 同组活动 PWM 无法复用该频率
 * @return LISA_DEVICE_ERR_IO HAL 配置失败
 */
static int venusa_pwm_set(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_cycle_percent)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (duty_cycle_percent > LISA_PWM_DUTY_PERCENT_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_channel_valid(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    pwm_channel_info_t old_info = priv->channels[channel];
    pwm_channel_info_t next = old_info;
    if (!next.configured) {
        next.mode = LISA_PWM_MODE_EDGE_ALIGNED;
        next.polarity = LISA_PWM_POLARITY_NORMAL;
        next.configured = true;
    }
    next.frequency_hz = frequency_hz;
    next.duty_cycle_percent = duty_cycle_percent;
    next.enabled = old_info.enabled;

    /* 频率选择需要在锁内完成，因为它依赖同组其它 port 的当前启用状态。 */
    pwm_group_info_t target_group = {0};
    ret = select_group_timing(priv, channel, frequency_hz, channel_wants_pwm_counter(&next), &target_group,
                              &next.period_count);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    ret = apply_configured_channel_state(priv, channel, &old_info, &next, &target_group);
    if (ret == LISA_DEVICE_OK) {
        priv->channels[channel] = next;
        priv->groups[channel_group_index(channel)] = target_group;
        LISA_LOGD(LOG_TAG, "Set PWM channel %lu: freq=%lu Hz, duty=%u%%, enabled=%d", channel, next.frequency_hz,
                  next.duty_cycle_percent, next.enabled);
    }

    DEVICE_UNLOCK(priv);

    return ret;
}

/**
 * @brief 启用 PWM 通道
 *
 * 1%-99% duty 会启用对应 PWM port 输出波形；0%/100% duty 只标记逻辑 enabled，
 * 硬件保持 PWM port disabled 并通过 init_level 输出静态电平。
 *
 * @param dev PWM 设备对象
 * @param channel PWM 通道号
 * @return LISA_DEVICE_OK 启用成功
 * @return LISA_DEVICE_ERR_NOT_READY 设备或通道未就绪
 * @return LISA_DEVICE_ERR_RANGE 通道号超出范围
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 同组活动 PWM 无法复用该频率
 * @return LISA_DEVICE_ERR_IO HAL 操作失败
 */
static int venusa_pwm_enable(lisa_device_t *dev, uint32_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int ret = check_channel_valid(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    if (!priv->channels[channel].configured || priv->channels[channel].frequency_hz == 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGW(LOG_TAG, "PWM channel %lu not configured before enable", channel);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->channels[channel].enabled) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    pwm_channel_info_t old_info = priv->channels[channel];
    pwm_channel_info_t target_info = priv->channels[channel];
    target_info.enabled = true;

    pwm_group_info_t target_group = {0};
    ret = select_group_timing(priv, channel, target_info.frequency_hz, channel_wants_pwm_counter(&target_info),
                              &target_group, &target_info.period_count);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    ret = apply_configured_channel_state(priv, channel, &old_info, &target_info, &target_group);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to enable PWM channel %lu", channel);
        return ret;
    }

    priv->channels[channel] = target_info;
    priv->groups[channel_group_index(channel)] = target_group;
    LISA_LOGD(LOG_TAG, "Enabled PWM channel %lu: freq=%lu Hz, duty=%u%%, hw_enabled=%d", channel,
              target_info.frequency_hz, target_info.duty_cycle_percent, channel_pwm_is_hardware_enabled(&target_info));

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 禁用 PWM 通道
 *
 * 关闭对应 PWM port 的硬件输出并清除软件 enabled 标志。若同组仍有其它 port 输出，
 * HAL 会保留 GPT channel 继续运行。
 *
 * @param dev PWM 设备对象
 * @param channel PWM 通道号
 * @return LISA_DEVICE_OK 禁用成功或通道原本未启用
 * @return LISA_DEVICE_ERR_NOT_READY 设备未初始化
 * @return LISA_DEVICE_ERR_RANGE 通道号超出范围
 * @return LISA_DEVICE_ERR_IO HAL 禁用失败
 */
static int venusa_pwm_disable(lisa_device_t *dev, uint32_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int ret = check_channel_valid(dev, channel);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    if (!priv->channels[channel].enabled) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    const venusa_pwm_map_t *map = &venusa_pwm_map[channel];
    if (HAL_GPT_DisablePWM(priv->hal_handler, map->gpt_channel, map->port_mask) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to disable PWM channel %lu", channel);
        return LISA_DEVICE_ERR_IO;
    }

    priv->channels[channel].enabled = false;
    LISA_LOGD(LOG_TAG, "Disabled PWM channel %lu", channel);

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/* ===== Venusa PWM API 实例 ===== */
static const lisa_pwm_api_t venusa_pwm_api = {
    .enable = venusa_pwm_enable,
    .disable = venusa_pwm_disable,
    .set = venusa_pwm_set,
    .configure = venusa_pwm_configure,
    .get_config = venusa_pwm_get_config,
};

/* ===== 设备初始化函数 ===== */

/**
 * @brief 初始化驱动资源
 *
 * 创建 PWM 驱动互斥锁，供后续 configure/set/enable/disable 保护软件缓存和 HAL 操作。
 *
 * @param priv PWM 设备私有数据
 * @return LISA_DEVICE_OK 初始化成功
 * @return LISA_DEVICE_ERR_INIT_FAIL 互斥锁创建失败
 */
static int venusa_pwm_init_resources(lisa_pwm_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 重置通道软件状态
 *
 * 将所有通道恢复为默认 edge aligned、normal polarity、未配置、未启用状态。
 * 该函数只修改软件缓存，不直接访问硬件寄存器。
 *
 * @param priv PWM 设备私有数据
 */
static void venusa_pwm_reset_channels(lisa_pwm_priv_t *priv)
{
    for (uint32_t i = 0; i < VENUSA_PWM_MAX_CHANNELS; i++) {
        priv->channels[i].frequency_hz = 0;
        priv->channels[i].period_count = 0;
        priv->channels[i].duty_cycle_percent = 0;
        priv->channels[i].mode = LISA_PWM_MODE_EDGE_ALIGNED;
        priv->channels[i].polarity = LISA_PWM_POLARITY_NORMAL;
        priv->channels[i].configured = false;
        priv->channels[i].enabled = false;
    }
}

/**
 * @brief 初始化 PWM0 硬件
 *
 * 获取 GPT0 HAL 句柄、初始化并上电 GPT，然后为每个 GPT channel 写入默认 clock 配置，
 * 最后调用板级 pinmux 完成 PWM 引脚复用。
 *
 * @param priv PWM 设备私有数据
 * @return LISA_DEVICE_OK 初始化成功
 * @return LISA_DEVICE_ERR_INIT_FAIL HAL 初始化或上电失败
 */
static int venusa_pwm0_init_hw(lisa_pwm_priv_t *priv)
{
    priv->hal_handler = GPT0();
    if (!priv->hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get GPT0 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (HAL_GPT_Initialize(priv->hal_handler) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize GPT");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (HAL_GPT_PowerControl(priv->hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on GPT");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    for (uint32_t i = 0; i < VENUSA_PWM_GROUP_COUNT; i++) {
        /* 初始化先给每个 GPT channel 写入有效默认时钟，真正频率在 lisa_pwm_set() 时重新选择。 */
        pwm_group_info_t default_group = {0};
        int ret = select_default_group_clock(&default_group);
        if (ret != LISA_DEVICE_OK) {
            return ret;
        }

        if (HAL_GPT_Control(priv->hal_handler, (GPT_Channel_Num_t)i, &default_group.hal_config) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to configure GPT channel %lu clock", i);
            return LISA_DEVICE_ERR_INIT_FAIL;
        }
        priv->groups[i] = default_group;
    }

    priv->max_channels = VENUSA_PWM_MAX_CHANNELS;
    venusa_pwm_reset_channels(priv);
    lisa_pwm_pinmux();

    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化 PWM0 设备
 *
 * 清空静态私有数据，初始化驱动资源和硬件，并注册为 lisa_device 框架中的 pwm0 设备。
 *
 * @return LISA_DEVICE_OK 初始化成功
 * @return LISA_DEVICE_ERR_INIT_FAIL 初始化失败
 */
static int venusa_pwm0_init(void)
{
    memset(&pwm0_priv, 0, sizeof(pwm0_priv));

    int ret = venusa_pwm_init_resources(&pwm0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = venusa_pwm0_init_hw(&pwm0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "PWM0 initialized successfully (max_channels=%lu)", pwm0_priv.max_channels);
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止并释放 PWM0 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 venusa_pwm0_init 申请相反：
 *   1) 关闭两个 GPT channel 的全部 PWM port，随后 HAL_GPT_PowerControl(OFF) +
 *      HAL_GPT_Uninitialize；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（通道逻辑标志随之清零，
 *      强制唤醒后业务侧重新 configure() + set()）。
 *
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int venusa_pwm0_deinit(void)
{
    lisa_pwm_priv_t *priv = &pwm0_priv;

    if (priv->hal_handler) {
        for (uint32_t i = 0; i < VENUSA_PWM_GROUP_COUNT; i++) {
            HAL_GPT_DisablePWM(priv->hal_handler, (GPT_Channel_Num_t)i, VENUSA_PWM_ALL_PORT_MASK);
        }
        HAL_GPT_PowerControl(priv->hal_handler, CSK_POWER_OFF);
        HAL_GPT_Uninitialize(priv->hal_handler);
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&pwm0_priv, 0, sizeof(pwm0_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM

/**
 * @brief 检查 PWM0 是否允许系统进入低功耗
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(pwm0) 释放全部软硬件资源，唤醒后经 lisa_device_reinit(pwm0)
 * 重建，业务再重新 configure() + set()。因此 prepare_suspend / resume_restore 不再需要，
 * 仅保留 check_idle：只要任一通道逻辑 enabled，就认为 PWM 仍在使用中并阻止系统 suspend。
 *
 * @param ctx PWM 设备私有数据
 * @return int32_t 1 表示 idle 可睡眠，0 表示 busy 不可睡眠
 */
static int32_t venusa_pwm_pm_check_idle(void *ctx)
{
    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)ctx;
    if (!priv) {
        return 1;
    }

    for (uint32_t i = 0; i < priv->max_channels; i++) {
        if (priv->channels[i].enabled) {
            return 0;
        }
    }

    return 1;
}

static const lisa_pm_system_ops_t venusa_pwm0_pm_ops = {
    .check_idle = venusa_pwm_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore = NULL,
};
#endif


// clang-format off
LISA_DEVICE_REGISTER_DEINIT(pwm0, &venusa_pwm_api, &pwm0_priv, NULL, venusa_pwm0_init,
                            venusa_pwm0_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(pwm0, &venusa_pwm0_pm_ops, NULL, &pwm0_priv);
#endif

// clang-format on
