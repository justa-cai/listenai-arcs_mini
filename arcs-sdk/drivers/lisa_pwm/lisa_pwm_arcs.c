/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_pwm_arcs.c
 * @brief LISA PWM ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 PWM 硬件适配
 */

#include "lisa_pwm.h"
#include "Driver_GPT_PWM.h"
#include "ClockManager.h"
#include <stddef.h>
#include <string.h>
#include "lisa_mutex.h"
#include "board.h"
#include <stdbool.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "lisa_pwm_arcs"
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

/* ===== PWM 通道映射定义 ===== */
#define MAX_PWM_CHANNELS 8

/* ===== PWM 通道配置信息 ===== */
typedef struct {
    uint32_t frequency_hz;              /* 频率 (Hz) */
    uint8_t duty_cycle_percent;         /* 占空比百分比 (0-100) */
    lisa_pwm_mode_t mode;               /* 输出对齐模式 */
    lisa_pwm_polarity_t polarity;       /* 输出极性 */
    bool configured;                    /* 是否已配置 */
    bool enabled;                       /* 是否已启用 */
    uint8_t clk_div_shift;              /* 当前使用的时钟分频 (0-7) */
} pwm_channel_info_t;

/* ===== PWM 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                       /* HAL PWM 句柄 */
    lisa_mutex_t *mutex;                     /* 互斥锁 */
    pwm_channel_info_t channels[MAX_PWM_CHANNELS]; /* 通道配置信息 */
    uint32_t max_channels;                   /* 最大通道数 */
} lisa_pwm_priv_t;

/* ===== PWM 设备静态实例 ===== */
static lisa_pwm_priv_t pwm0_priv;

/* ===== 内部辅助函数 ===== */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

static const uint32_t clk_div_reg_table[] = {
    CSK_GPT_PWM_CLKDIV_1,
    CSK_GPT_PWM_CLKDIV_2,
    CSK_GPT_PWM_CLKDIV_4,
    CSK_GPT_PWM_CLKDIV_8,
    CSK_GPT_PWM_CLKDIV_16,
    CSK_GPT_PWM_CLKDIV_32,
    CSK_GPT_PWM_CLKDIV_64,
    CSK_GPT_PWM_CLKDIV_128,
};

/**
 * @brief 将分频移位值转换为HAL寄存器值
 *
 * @param shift 分频移位值 (0-7, 对应 ÷1/2/4/8/16/32/64/128)
 * @return uint32_t HAL寄存器分频值
 */
static inline uint32_t clk_div_reg_value(uint8_t shift)
{
    if (shift >= ARRAY_SIZE(clk_div_reg_table)) {
        return clk_div_reg_table[0];
    }
    return clk_div_reg_table[shift];
}

/**
 * @brief 检查指定频率是否可以用指定分频支持
 *
 * @param frequency_hz 目标频率 (Hz)
 * @param shift 时钟分频移位值 (0-7)
 * @param pclk 外设时钟频率 (Hz)
 * @param mode PWM 输出对齐模式
 * @return true 频率在有效范围内
 * @return false 频率超出范围或无效
 */
static bool check_freq_with_shift(uint32_t frequency_hz, uint8_t shift, uint32_t pclk, lisa_pwm_mode_t mode)
{
    uint32_t effective_clk = pclk >> shift;
    if (effective_clk == 0) {
        return false;
    }

    uint32_t period_counts = effective_clk / frequency_hz;
    if (period_counts == 0) {
        return false;
    }

    if (mode == LISA_PWM_MODE_CENTER_ALIGNED) {
        period_counts >>= 1;
        if (period_counts == 0) {
            return false;
        }
    }

    /* 16位计数器最大值为 0xFFFF */
    return (period_counts <= 0xFFFF);
}

/**
 * @brief 选择支持目标频率的时钟分频器
 *
 * @param frequency_hz 目标频率 (Hz)
 * @param current_shift 当前使用的分频移位值
 * @param prefer_current 是否优先使用当前分频（如果支持）
 * @param mode PWM 输出对齐模式
 * @param out_shift 输出参数，返回选中的分频移位值
 * @return true 找到合适的分频器
 * @return false 没有找到支持该频率的分频器
 */
static bool select_clk_divider(uint32_t frequency_hz, uint8_t current_shift, bool prefer_current,
                               lisa_pwm_mode_t mode, uint8_t *out_shift)
{
    if (frequency_hz == 0) {
        return false;
    }

    uint32_t pclk = CRM_GetCmn_peri_pclkFreq();
    if (pclk == 0) {
        pclk = CRM_GetSrcFreq(CRM_IpSrcPeriClk);
    }

    if (pclk == 0) {
        return false;
    }

    /* 如果优先使用当前分频，先检查当前分频是否支持目标频率 */
    if (prefer_current && current_shift < ARRAY_SIZE(clk_div_reg_table)) {
        if (check_freq_with_shift(frequency_hz, current_shift, pclk, mode)) {
            if (out_shift) {
                *out_shift = current_shift;
            }
            return true;
        }
    }

    /* 当前分频不支持，遍历所有分频选项，选择第一个满足条件的分频 */
    for (uint8_t shift = 0; shift < ARRAY_SIZE(clk_div_reg_table); shift++) {
        if (check_freq_with_shift(frequency_hz, shift, pclk, mode)) {
            if (out_shift) {
                *out_shift = shift;
            }
            return true;
        }
    }

    return false;
}

/**
 * @brief 检查通道号有效性
 */
static inline int check_channel_valid(lisa_device_t *dev, uint32_t channel)
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
 * @brief 将通道号转换为HAL通道类型
 */
static inline GPT_CHANNEL_TYPE channel_to_hal_channel(uint32_t channel)
{
    return (GPT_CHANNEL_TYPE)channel;
}

/**
 * @brief 将极性转换为HAL极性控制位
 */
static uint32_t polarity_to_hal_polarity(lisa_pwm_polarity_t polarity)
{
    switch (polarity) {
    case LISA_PWM_POLARITY_NORMAL:
        return CSK_GPT_PWM_OUTPOLARITY_HIGH;  /* 正常极性：高电平有效 */
    case LISA_PWM_POLARITY_INVERTED:
        return CSK_GPT_PWM_OUTPOLARITY_LOW;   /* 反转极性：低电平有效 */
    default:
        return CSK_GPT_PWM_OUTPOLARITY_HIGH;
    }
}

/**
 * @brief 将对齐模式转换为HAL输出模式控制位
 *
 * @param mode PWM 输出对齐模式
 * @return uint32_t HAL 输出模式控制位
 */
static uint32_t mode_to_hal_outmode(lisa_pwm_mode_t mode)
{
    switch (mode) {
    case LISA_PWM_MODE_EDGE_ALIGNED:
        return CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED;
    case LISA_PWM_MODE_CENTER_ALIGNED:
        return CSK_GPT_PWM_OUTMODE_CENTRAL_ALIGNED;
    default:
        return CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED;
    }
}

/**
 * @brief 检查 PWM 输出对齐模式是否有效
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
 * @param polarity PWM 输出极性
 * @return true 极性有效
 * @return false 极性无效
 */
static bool polarity_is_valid(lisa_pwm_polarity_t polarity)
{
    return polarity == LISA_PWM_POLARITY_NORMAL || polarity == LISA_PWM_POLARITY_INVERTED;
}

/**
 * @brief 获取正常波形输出时需要写入HAL的极性位
 *
 * Edge aligned 模式下当前硬件不支持通过极性位翻转波形，因此始终使用正常极性；
 * 反转极性由占空比互补实现。Center aligned 模式使用硬件极性位。
 *
 * @param info PWM 通道配置状态
 * @return uint32_t HAL 极性控制位
 */
static uint32_t waveform_hal_polarity(const pwm_channel_info_t *info)
{
    if (info->mode == LISA_PWM_MODE_EDGE_ALIGNED) {
        return CSK_GPT_PWM_OUTPOLARITY_HIGH;
    }

    return polarity_to_hal_polarity(info->polarity);
}

/**
 * @brief 将用户占空比转换为HAL占空比
 *
 * Edge aligned 反转极性时，硬件极性位不生效，驱动使用 100%-duty
 * 转换为等效反相波形；其他场景直接使用用户占空比。
 *
 * @param info PWM 通道配置状态
 * @param duty_cycle_percent 用户设置的占空比百分比
 * @return uint8_t 写入 HAL 的占空比百分比
 */
static uint8_t waveform_hal_duty(const pwm_channel_info_t *info, uint8_t duty_cycle_percent)
{
    if (info->mode == LISA_PWM_MODE_EDGE_ALIGNED && info->polarity == LISA_PWM_POLARITY_INVERTED) {
        return LISA_PWM_DUTY_PERCENT_MAX - duty_cycle_percent;
    }

    return duty_cycle_percent;
}

/**
 * @brief 判断占空比是否为边界值
 *
 * 0% 和 100% 不走 HAL 动态占空比配置，而是转换为静态输出电平。
 *
 * @param duty_cycle_percent 占空比百分比
 * @return true 占空比为 0% 或 100%
 * @return false 占空比为 1%-99%
 */
static bool duty_is_boundary(uint8_t duty_cycle_percent)
{
    return duty_cycle_percent == 0 || duty_cycle_percent == LISA_PWM_DUTY_PERCENT_MAX;
}

/**
 * @brief 计算边界占空比对应的静态输出电平
 *
 * @param info PWM 通道配置状态
 * @param duty_cycle_percent 用户设置的边界占空比，取值为 0 或 100
 * @return true 输出高电平
 * @return false 输出低电平
 */
static bool boundary_output_is_high(const pwm_channel_info_t *info, uint8_t duty_cycle_percent)
{
    if (duty_cycle_percent == 0) {
        return info->polarity == LISA_PWM_POLARITY_INVERTED;
    }

    return info->polarity == LISA_PWM_POLARITY_NORMAL;
}

/**
 * @brief 将静态输出电平转换为 HAL 极性控制位
 *
 * @param output_high 是否输出高电平
 * @return uint32_t HAL 极性控制位
 */
static uint32_t output_level_to_hal_polarity(bool output_high)
{
    return output_high ? CSK_GPT_PWM_OUTPOLARITY_HIGH : CSK_GPT_PWM_OUTPOLARITY_LOW;
}

/**
 * @brief 组合 PWM HAL 控制字
 *
 * @param info PWM 通道配置状态
 * @param clk_div_shift 时钟分频移位值
 * @param hal_polarity HAL 极性控制位
 * @return uint32_t HAL PWM 控制字
 */
static uint32_t build_pwm_control(const pwm_channel_info_t *info, uint8_t clk_div_shift, uint32_t hal_polarity)
{
    return CSK_GPT_PWM_MODE |
           CSK_GPT_PWM_CLKSRC_PCLK |
           mode_to_hal_outmode(info->mode) |
           hal_polarity |
           clk_div_reg_value(clk_div_shift) |
           CSK_GPT_PWM_OPERATION_MODE_PWM;
}

/**
 * @brief 按指定通道状态将 PWM 控制字应用到 HAL
 *
 * 该函数用于配置切换过程中的临时状态，避免 HAL 操作失败前提前修改
 * 驱动缓存。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 要应用的 PWM 通道状态
 * @param hal_polarity HAL 极性控制位
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int apply_pwm_control_with_info(lisa_pwm_priv_t *priv, uint32_t channel,
                                       const pwm_channel_info_t *info, uint32_t hal_polarity)
{
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);
    uint32_t control = build_pwm_control(info, info->clk_div_shift, hal_polarity);

    if (HAL_GPT_PWMControl(priv->hal_handler, control, hal_channel) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to configure PWM control");
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 按指定通道状态配置 PWM 波形输出控制位
 *
 * 该函数用于配置切换过程中的目标状态预应用。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 要应用的 PWM 通道状态
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int apply_waveform_control_with_info(lisa_pwm_priv_t *priv, uint32_t channel,
                                            const pwm_channel_info_t *info)
{
    return apply_pwm_control_with_info(priv, channel, info, waveform_hal_polarity(info));
}

/**
 * @brief 按当前模式和极性配置 PWM 波形输出控制位
 *
 * 该函数用于 1%-99% 占空比的正常 PWM 波形输出。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int apply_waveform_control(lisa_pwm_priv_t *priv, uint32_t channel)
{
    return apply_waveform_control_with_info(priv, channel, &priv->channels[channel]);
}

/**
 * @brief 按指定通道状态配置静态输出电平
 *
 * 该函数用于配置切换过程中的目标状态预应用。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 要应用的 PWM 通道状态
 * @param duty_cycle_percent 用户设置的边界占空比，取值为 0 或 100
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int apply_static_output_control_with_info(lisa_pwm_priv_t *priv, uint32_t channel,
                                                 const pwm_channel_info_t *info,
                                                 uint8_t duty_cycle_percent)
{
    bool output_high = boundary_output_is_high(info, duty_cycle_percent);
    return apply_pwm_control_with_info(priv, channel, info, output_level_to_hal_polarity(output_high));
}

/**
 * @brief 按边界占空比配置静态输出电平
 *
 * 该函数用于 0% 和 100% 占空比，通过 HAL 极性控制位让引脚保持
 * 对应的静态高/低电平，不启动 PWM 波形输出。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param duty_cycle_percent 用户设置的边界占空比，取值为 0 或 100
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int apply_static_output_control(lisa_pwm_priv_t *priv, uint32_t channel, uint8_t duty_cycle_percent)
{
    return apply_static_output_control_with_info(priv, channel, &priv->channels[channel], duty_cycle_percent);
}

/**
 * @brief 按旧通道状态恢复硬件配置
 *
 * 若旧 PWM 硬件仍可能处于运行状态，先尝试关闭再重配，避免恢复路径重复
 * enable 已运行的通道。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 需要恢复的旧通道状态
 * @param hardware_running 旧 PWM 硬件当前是否可能仍在运行
 */
static void restore_channel_hardware(lisa_pwm_priv_t *priv, uint32_t channel,
                                     const pwm_channel_info_t *info, bool hardware_running)
{
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);
    int ret;

    if (!info->configured) {
        return;
    }

    if (duty_is_boundary(info->duty_cycle_percent)) {
        ret = apply_static_output_control_with_info(priv, channel, info, info->duty_cycle_percent);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Failed to restore PWM static output");
        }
        return;
    }

    if (info->enabled && hardware_running) {
        if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to stop PWM before restore");
        } else {
            hardware_running = false;
        }
    }

    ret = apply_waveform_control_with_info(priv, channel, info);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to restore PWM waveform control");
        return;
    }

    uint8_t hal_duty = waveform_hal_duty(info, info->duty_cycle_percent);
    if (HAL_GPT_SetPWMFreqDuty(priv->hal_handler, hal_channel, info->frequency_hz, hal_duty) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to restore PWM frequency/duty");
        return;
    }

    if (info->enabled && !hardware_running && HAL_GPT_EnablePWM(priv->hal_handler, hal_channel) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to restore PWM enable state");
    }
}

/**
 * @brief 尝试恢复通道原有硬件配置
 *
 * configure 过程中若目标 HAL 配置失败，使用该函数把硬件恢复到旧缓存描述的状态，
 * 避免后续 enable/set 基于旧缓存时遇到硬件残留的新配置。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param info 需要恢复的旧通道状态
 */
static void restore_configured_channel_hardware(lisa_pwm_priv_t *priv, uint32_t channel,
                                                const pwm_channel_info_t *info)
{
    restore_channel_hardware(priv, channel, info, false);
}

/**
 * @brief 回滚通道缓存并尽量恢复旧硬件状态
 *
 * set/configure 过程中若 HAL 操作失败，使用该函数把驱动缓存恢复到
 * 进入操作前的状态，并按旧缓存重新配置硬件。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param old_info 进入操作前保存的旧通道状态
 * @param old_hardware_running 旧 PWM 硬件当前是否可能仍在运行
 */
static void rollback_channel_state(lisa_pwm_priv_t *priv, uint32_t channel,
                                   const pwm_channel_info_t *old_info,
                                   bool old_hardware_running)
{
    restore_channel_hardware(priv, channel, old_info, old_hardware_running);
    priv->channels[channel] = *old_info;
}

/**
 * @brief 将已配置通道切换到目标硬件状态
 *
 * 该函数只操作 HAL，不修改驱动缓存。调用者应在返回成功后再更新
 * priv->channels[channel]。
 *
 * @param priv PWM 设备私有数据
 * @param channel PWM 通道号
 * @param old_info 当前缓存中的旧通道状态
 * @param target_info 要切换到的新通道状态
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int apply_configured_channel_state(lisa_pwm_priv_t *priv, uint32_t channel,
                                          const pwm_channel_info_t *old_info,
                                          const pwm_channel_info_t *target_info)
{
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);
    bool old_hardware_running = old_info->enabled && !duty_is_boundary(old_info->duty_cycle_percent);
    int ret;

    if (old_hardware_running) {
        if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to disable PWM for reconfiguration");
            return LISA_DEVICE_ERR_IO;
        }
    }

    if (duty_is_boundary(target_info->duty_cycle_percent)) {
        ret = apply_static_output_control_with_info(priv, channel, target_info, target_info->duty_cycle_percent);
        if (ret != LISA_DEVICE_OK) {
            restore_configured_channel_hardware(priv, channel, old_info);
            return ret;
        }
        return LISA_DEVICE_OK;
    }

    ret = apply_waveform_control_with_info(priv, channel, target_info);
    if (ret != LISA_DEVICE_OK) {
        restore_configured_channel_hardware(priv, channel, old_info);
        return ret;
    }

    uint8_t hal_duty = waveform_hal_duty(target_info, target_info->duty_cycle_percent);
    ret = HAL_GPT_SetPWMFreqDuty(priv->hal_handler, hal_channel, target_info->frequency_hz, hal_duty);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set PWM frequency/duty: frequency %u Hz, duty %u%%",
                  target_info->frequency_hz, hal_duty);
        restore_configured_channel_hardware(priv, channel, old_info);
        return LISA_DEVICE_ERR_IO;
    }

    if (target_info->enabled) {
        if (HAL_GPT_EnablePWM(priv->hal_handler, hal_channel) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to enable PWM");
            restore_configured_channel_hardware(priv, channel, old_info);
            return LISA_DEVICE_ERR_IO;
        }
    }

    return LISA_DEVICE_OK;
}

/* ===== ARCS平台PWM实现函数 ===== */

/**
 * @brief 配置PWM通道属性
 *
 * @param dev PWM设备指针
 * @param channel 通道号 (0-7)
 * @param config 配置参数（输出模式、极性）
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!mode_is_valid(config->mode) || !polarity_is_valid(config->polarity)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_channel_valid(dev, channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    pwm_channel_info_t target_info = priv->channels[channel];
    target_info.mode = config->mode;
    target_info.polarity = config->polarity;

    if (!priv->channels[channel].configured) {
        priv->channels[channel] = target_info;
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    if (!select_clk_divider(target_info.frequency_hz,
                            target_info.clk_div_shift,
                            true,
                            target_info.mode,
                            &target_info.clk_div_shift)) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Unsupported frequency %u Hz", target_info.frequency_hz);
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = apply_configured_channel_state(priv, channel, &priv->channels[channel], &target_info);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    priv->channels[channel] = target_info;

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取PWM通道配置
 *
 * @param dev PWM设备指针
 * @param channel 通道号 (0-7)
 * @param config 输出参数，返回当前配置
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm_get_config(lisa_device_t *dev, uint32_t channel, lisa_pwm_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_channel_valid(dev, channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);
    config->mode = priv->channels[channel].mode;
    config->polarity = priv->channels[channel].polarity;
    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 重新配置通道的时钟分频
 *
 * @param priv PWM私有数据指针
 * @param channel 通道号
 * @param new_shift 新的分频移位值
 * @param hardware_running 输出参数,返回失败或成功后旧 PWM 硬件是否可能仍在运行
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int reconfigure_channel_divider(lisa_pwm_priv_t *priv, uint32_t channel,
                                       uint8_t new_shift, bool *hardware_running)
{
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);

    /* 记录通道是否已启用 */
    bool was_enabled = priv->channels[channel].enabled;
    *hardware_running = was_enabled && !duty_is_boundary(priv->channels[channel].duty_cycle_percent);

    /* 如果通道正在输出PWM波形,先禁用硬件 */
    if (*hardware_running) {
        if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to disable PWM for reconfiguration");
            return LISA_DEVICE_ERR_IO;
        }
        *hardware_running = false;
    }

    priv->channels[channel].enabled = false;

    /* 如果通道已配置过,需要重新初始化PWM控制器以重置通道状态 */
    if (priv->channels[channel].configured) {
        if (HAL_GPT_PWMInitialize(priv->hal_handler, NULL) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to reinitialize PWM");
            return LISA_DEVICE_ERR_IO;
        }

        /* 重新使能PWM电源 */
        if (HAL_GPT_PWMPowerControl(priv->hal_handler, CSK_POWER_FULL) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to power on PWM");
            return LISA_DEVICE_ERR_IO;
        }
    }

    priv->channels[channel].clk_div_shift = new_shift;

    /* 配置新的分频和正常波形输出控制位 */
    if (apply_waveform_control(priv, channel) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to configure PWM divider");
        return LISA_DEVICE_ERR_IO;
    }

    priv->channels[channel].configured = true;

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置PWM频率和占空比
 *
 * 此函数自动处理跨时钟分频的频率切换。
 * 当需要切换时钟分频时，会自动禁用通道、重新配置、然后重新启用。
 *
 * 特殊处理:
 * - 占空比为 0%: 禁用 PWM,通过极性配置使引脚输出低电平(正常极性)或高电平(反转极性)
 * - 占空比为 100%: 禁用 PWM,通过极性配置使引脚输出高电平(正常极性)或低电平(反转极性)
 * - 占空比为 1-99%: 正常配置 PWM 并启用，边沿对齐反转极性使用 100%-duty 模拟
 *
 * @param dev PWM设备指针
 * @param channel 通道号 (0-7)
 * @param frequency_hz 频率 (Hz)
 * @param duty_cycle_percent 占空比百分比 (0-100)
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm_set(lisa_device_t *dev, uint32_t channel, uint32_t frequency_hz, uint8_t duty_cycle_percent)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_channel_valid(dev, channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    if (duty_cycle_percent > LISA_PWM_DUTY_PERCENT_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);
    int ret;

    DEVICE_LOCK(priv);

    pwm_channel_info_t old_info = priv->channels[channel];
    uint8_t current_shift = priv->channels[channel].clk_div_shift;
    bool is_configured = priv->channels[channel].configured;
    uint8_t target_shift = 0;
    uint8_t old_duty = priv->channels[channel].duty_cycle_percent;
    bool old_is_boundary = duty_is_boundary(old_duty);
    bool was_enabled_before_set = priv->channels[channel].enabled;
    bool divider_reconfigured = false;
    bool hardware_running = was_enabled_before_set && is_configured && !old_is_boundary;

    /* 选择合适的时钟分频 */
    if (is_configured) {
        /* 通道已配置,优先使用当前分频(如果支持新频率) */
        if (!select_clk_divider(frequency_hz,
                                current_shift,
                                true,
                                priv->channels[channel].mode,
                                &target_shift)) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Unsupported frequency %u Hz", frequency_hz);
            return LISA_DEVICE_ERR_INVALID;
        }
    } else {
        /* 通道未配置,选择最合适的分频 */
        if (!select_clk_divider(frequency_hz,
                                0,
                                false,
                                priv->channels[channel].mode,
                                &target_shift)) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Unsupported frequency %u Hz", frequency_hz);
            return LISA_DEVICE_ERR_INVALID;
        }
    }

    /* 如果需要更换分频,重新配置通道 */
    if (!is_configured || target_shift != current_shift) {
        ret = reconfigure_channel_divider(priv, channel, target_shift, &hardware_running);
        if (ret != LISA_DEVICE_OK) {
            rollback_channel_state(priv, channel, &old_info, hardware_running);
            DEVICE_UNLOCK(priv);
            return ret;
        }
        divider_reconfigured = true;
    }

    /* 判断占空比类型变化:
     * - 边界值 (0% 或 100%): 通过极性控制输出静态电平,不启用 PWM 硬件
     * - 正常值 (1-99%): 通过 PWM 硬件输出波形,需要启用
     */
    bool new_is_boundary = duty_is_boundary(duty_cycle_percent);

    /* 特殊处理: 占空比为 0% 或 100% 时不启用 PWM,通过极性控制输出电平 */
    if (new_is_boundary) {
        ret = apply_static_output_control(priv, channel, duty_cycle_percent);
        if (ret != LISA_DEVICE_OK) {
            rollback_channel_state(priv, channel, &old_info, hardware_running);
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to configure PWM for duty 0%%/100%%");
            return ret;
        }

        /* 如果通道此前正在输出PWM波形,需要禁用硬件以使引脚输出静态电平 */
        if (was_enabled_before_set && !old_is_boundary && !divider_reconfigured) {
            if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
                rollback_channel_state(priv, channel, &old_info, hardware_running);
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to disable PWM for duty 0%%/100%%");
                return LISA_DEVICE_ERR_IO;
            }
            hardware_running = false;
        }

        priv->channels[channel].enabled = was_enabled_before_set;
    } else {
        if (old_is_boundary || divider_reconfigured) {
            ret = apply_waveform_control(priv, channel);
            if (ret != LISA_DEVICE_OK) {
                rollback_channel_state(priv, channel, &old_info, hardware_running);
                DEVICE_UNLOCK(priv);
                return ret;
            }
        }

        uint8_t hal_duty = waveform_hal_duty(&priv->channels[channel], duty_cycle_percent);

        /* 占空比为 1-99%: 正常设置频率和占空比 */
        ret = HAL_GPT_SetPWMFreqDuty(priv->hal_handler, hal_channel, frequency_hz, hal_duty);
        if (ret != 0) {
            rollback_channel_state(priv, channel, &old_info, hardware_running);
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to set PWM frequency/duty: frequency %u Hz, duty %u%%",
                      frequency_hz, hal_duty);
            return LISA_DEVICE_ERR_IO;
        }

        /* 仅当逻辑已启用且当前硬件未运行时启用 PWM:
         * - 跨分频重配置会先关闭/重置硬件
         * - 旧占空比为 0%/100% 时只保持静态电平,硬件未运行
         */
        bool should_enable = was_enabled_before_set && !hardware_running;

        if (should_enable) {
            if (HAL_GPT_EnablePWM(priv->hal_handler, hal_channel) != 0) {
                rollback_channel_state(priv, channel, &old_info, hardware_running);
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to enable PWM");
                return LISA_DEVICE_ERR_IO;
            }
            hardware_running = true;
        }
        priv->channels[channel].enabled = was_enabled_before_set;
    }

    /* 保存配置 */
    priv->channels[channel].frequency_hz = frequency_hz;
    priv->channels[channel].duty_cycle_percent = duty_cycle_percent;

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 启用PWM通道
 *
 * 启用前通道必须已通过 arcs_pwm_set 配置过。
 *
 * 特殊行为:
 * - 占空比为 0%/100% 时,不会启用 PWM 硬件,而是通过极性配置保持对应的静态电平
 * - 占空比为 1-99% 时,正常启用 PWM 输出波形
 *
 * @param dev PWM设备指针
 * @param channel 通道号 (0-7)
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm_enable(lisa_device_t *dev, uint32_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_channel_valid(dev, channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);

    DEVICE_LOCK(priv);

    /* 检查通道是否已配置 */
    if (!priv->channels[channel].configured) {
        DEVICE_UNLOCK(priv);
        LISA_LOGW(LOG_TAG, "PWM not configured before enable");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 特殊处理: 占空比为 0% 或 100% 时不启用 PWM 硬件
     * 此时引脚已通过极性配置输出对应的静态电平,无需启用 PWM */
    uint8_t duty = priv->channels[channel].duty_cycle_percent;
    if (duty_is_boundary(duty)) {
        /* 标记为"已启用"状态,但实际不调用 HAL_GPT_EnablePWM */
        priv->channels[channel].enabled = true;
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    /* 占空比为 1-99%: 启用PWM硬件输出波形 */
    if (HAL_GPT_EnablePWM(priv->hal_handler, hal_channel) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to enable PWM");
        return LISA_DEVICE_ERR_IO;
    }

    priv->channels[channel].enabled = true;

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 禁用PWM通道
 *
 * 禁用后通道配置信息（分频、极性等）仍然保留，可以重新启用。
 *
 * @param dev PWM设备指针
 * @param channel 通道号 (0-7)
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm_disable(lisa_device_t *dev, uint32_t channel)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_channel_valid(dev, channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);

    DEVICE_LOCK(priv);

    /* 禁用PWM通道 */
    if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to disable PWM");
        return LISA_DEVICE_ERR_IO;
    }

    priv->channels[channel].enabled = false;

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/* ===== ARCS PWM API 实例 ===== */
static const lisa_pwm_api_t arcs_pwm_api = {
    .enable = arcs_pwm_enable,
    .disable = arcs_pwm_disable,
    .set = arcs_pwm_set,
    .configure = arcs_pwm_configure,
    .get_config = arcs_pwm_get_config,
};

/* ===== 设备初始化函数 ===== */

/**
 * @brief OS 资源初始化（mutex），仅 _init 阶段调用一次，跨 suspend/resume 保留
 */
static int arcs_pwm_init_resources(lisa_pwm_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 PWM0 HAL 硬件初始化
 *
 * 由 _init 调用；只动 HAL / pinmux，不分配 mutex / 堆内存。唤醒后经 reinit
 * 重新走 _init 路径时，destroy 阶段已先 HAL_GPT_PWMPowerControl(OFF) +
 * HAL_GPT_PWMUninitialize 清零 HAL 状态；启动期首次调用时 HAL 状态本就为零，
 * 重复 Initialize 无副作用。
 */
static int arcs_pwm0_init_hw(lisa_pwm_priv_t *priv)
{
    /* 获取 HAL PWM 句柄 */
    priv->hal_handler = GPT0_PWM();
    if (!priv->hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get GPT0_PWM handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL PWM */
    if (HAL_GPT_PWMInitialize(priv->hal_handler, NULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize GPT PWM");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 使能 PWM 电源 */
    if (HAL_GPT_PWMPowerControl(priv->hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on GPT PWM");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 设置最大通道数 */
    priv->max_channels = MAX_PWM_CHANNELS;

    /* 初始化所有通道为默认配置（正常极性）。
     * 唤醒路径下硬件已被 PowerControl(OFF) 重置，逻辑通道状态也必须随之清零，
     * 强制应用 wake 后重新 configure() + set() 才能重新出波形。*/
    for (uint32_t i = 0; i < MAX_PWM_CHANNELS; i++) {
        pwm0_priv.channels[i].mode = LISA_PWM_MODE_EDGE_ALIGNED;
        pwm0_priv.channels[i].polarity = LISA_PWM_POLARITY_NORMAL;
        pwm0_priv.channels[i].configured = false;
        pwm0_priv.channels[i].enabled = false;
        pwm0_priv.channels[i].frequency_hz = 0;
        pwm0_priv.channels[i].duty_cycle_percent = 0;
        pwm0_priv.channels[i].clk_div_shift = 0;
    }

    lisa_pwm_pinmux();

    return LISA_DEVICE_OK;
}

/**
 * @brief PWM0 设备初始化函数
 *
 * 初始化 PWM0 控制器：
 * - 创建互斥锁（_init_resources，仅一次）
 * - 获取 HAL 句柄 / Initialize / PowerControl / pinmux（_init_hw，幂等）
 *
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm0_init(void)
{
    /* 清空私有数据 */
    memset(&pwm0_priv, 0, sizeof(lisa_pwm_priv_t));

    int ret = arcs_pwm_init_resources(&pwm0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_pwm0_init_hw(&pwm0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "PWM0 initialized successfully (max_channels=%lu)", pwm0_priv.max_channels);

    return LISA_DEVICE_OK;
}

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放 PWM0 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 arcs_pwm0_init 申请相反：
 *   1) 逐通道 DisablePWM 停波形，再 HAL_GPT_PWMPowerControl(OFF) +
 *      HAL_GPT_PWMUninitialize（PowerControl(OFF) 内部会读 INITIALIZED 状态，
 *      故顺序固定）；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（通道逻辑标志随之清零，
 *      强制唤醒后业务侧重新 configure() + set()）。
 *
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int arcs_pwm0_deinit(void)
{
    lisa_pwm_priv_t *priv = &pwm0_priv;

    if (priv->hal_handler) {
        for (uint32_t i = 0; i < priv->max_channels; i++) {
            if (priv->channels[i].configured) {
                HAL_GPT_DisablePWM(priv->hal_handler, channel_to_hal_channel(i));
            }
        }
        HAL_GPT_PWMPowerControl(priv->hal_handler, CSK_POWER_OFF);
        HAL_GPT_PWMUninitialize(priv->hal_handler);
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&pwm0_priv, 0, sizeof(lisa_pwm_priv_t));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(pwm0) 释放全部软硬件资源（HAL 下电 + mutex），唤醒后在
 * PM after_wake 回调中调 lisa_device_reinit(pwm0) 重建到 _init 后的状态，并由业务
 * 重新 configure() + set()。因此 prepare_suspend / resume_restore 不再需要（原先它们
 * 只做 HAL 拆卸 / 通道字段清零，已被 destroy/reinit 覆盖，且二者运行于 PM 临界区
 * 无法做重活）。
 *
 * 仅保留 check_idle：任一通道 enabled 即代表应用要求 PWM 输出（含 0%/100% 边界态），
 * 禁止 AUTO_LIGHT_SLEEP。只读 priv，不取 mutex / 不读 HAL。
 */
static int32_t arcs_pwm_pm_check_idle(void *ctx)
{
    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    for (uint32_t i = 0; i < priv->max_channels; i++) {
        if (priv->channels[i].enabled) {
            return 0;
        }
    }
    return 1;
}

static const lisa_pm_system_ops_t arcs_pwm0_pm_ops = {
    .check_idle      = arcs_pwm_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */

/* ===== 设备注册 ===== */


LISA_DEVICE_REGISTER_DEINIT(pwm0, &arcs_pwm_api, &pwm0_priv, NULL, arcs_pwm0_init,
                            arcs_pwm0_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(pwm0, &arcs_pwm0_pm_ops, NULL, &pwm0_priv);
#endif

