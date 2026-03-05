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
 * @return true 频率在有效范围内
 * @return false 频率超出范围或无效
 */
static bool check_freq_with_shift(uint32_t frequency_hz, uint8_t shift, uint32_t pclk)
{
    uint32_t effective_clk = pclk >> shift;
    if (effective_clk == 0) {
        return false;
    }

    uint32_t period_counts = effective_clk / frequency_hz;
    if (period_counts == 0) {
        return false;
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
 * @param out_shift 输出参数，返回选中的分频移位值
 * @return true 找到合适的分频器
 * @return false 没有找到支持该频率的分频器
 */
static bool select_clk_divider(uint32_t frequency_hz, uint8_t current_shift, bool prefer_current, uint8_t *out_shift)
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
        if (check_freq_with_shift(frequency_hz, current_shift, pclk)) {
            if (out_shift) {
                *out_shift = current_shift;
            }
            return true;
        }
    }

    /* 当前分频不支持，遍历所有分频选项，选择第一个满足条件的分频 */
    for (uint8_t shift = 0; shift < ARRAY_SIZE(clk_div_reg_table); shift++) {
        if (check_freq_with_shift(frequency_hz, shift, pclk)) {
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

/* ===== ARCS平台PWM实现函数 ===== */

/**
 * @brief 配置PWM通道属性
 *
 * @param dev PWM设备指针
 * @param channel 通道号 (0-7)
 * @param config 配置参数（如极性）
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm_configure(lisa_device_t *dev, uint32_t channel, const lisa_pwm_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_channel_valid(dev, channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_pwm_priv_t *priv = (lisa_pwm_priv_t *)dev->priv_data;
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);

    DEVICE_LOCK(priv);

    /* 保存极性配置 */
    priv->channels[channel].polarity = config->polarity;

    /* 如果通道已启用，需要重新配置 */
    if (priv->channels[channel].enabled) {
        uint32_t hal_polarity = polarity_to_hal_polarity(config->polarity);
        uint32_t control = CSK_GPT_PWM_MODE |
                          CSK_GPT_PWM_CLKSRC_PCLK |
                          CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED |
                          hal_polarity |
                          clk_div_reg_value(priv->channels[channel].clk_div_shift) |
                          CSK_GPT_PWM_OPERATION_MODE_PWM;

        if (HAL_GPT_PWMControl(priv->hal_handler, control, hal_channel) != 0) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to configure PWM");
            return LISA_DEVICE_ERR_IO;
        }
    }

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
 * @param was_enabled 输出参数,返回通道之前是否启用
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int reconfigure_channel_divider(lisa_pwm_priv_t *priv, uint32_t channel,
                                       uint8_t new_shift, bool *was_enabled)
{
    GPT_CHANNEL_TYPE hal_channel = channel_to_hal_channel(channel);
    uint32_t hal_polarity = polarity_to_hal_polarity(priv->channels[channel].polarity);

    /* 记录通道是否已启用 */
    *was_enabled = priv->channels[channel].enabled;

    /* 如果通道已启用,先禁用 */
    if (*was_enabled) {
        if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
            LISA_LOGE(LOG_TAG, "Failed to disable PWM for reconfiguration");
            return LISA_DEVICE_ERR_IO;
        }
        priv->channels[channel].enabled = false;
    }

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

    /* 配置新的分频 */
    uint32_t control = CSK_GPT_PWM_MODE |
                       CSK_GPT_PWM_CLKSRC_PCLK |
                       CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED |
                       hal_polarity |
                       clk_div_reg_value(new_shift) |
                       CSK_GPT_PWM_OPERATION_MODE_PWM;

    if (HAL_GPT_PWMControl(priv->hal_handler, control, hal_channel) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to configure PWM divider");
        return LISA_DEVICE_ERR_IO;
    }

    priv->channels[channel].configured = true;
    priv->channels[channel].clk_div_shift = new_shift;

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
 * - 占空比为 1-99%: 正常配置 PWM 并启用
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

    uint8_t current_shift = priv->channels[channel].clk_div_shift;
    bool is_configured = priv->channels[channel].configured;
    uint8_t target_shift = 0;
    bool was_enabled = false;
    uint8_t old_duty = priv->channels[channel].duty_cycle_percent;

    /* 选择合适的时钟分频 */
    if (is_configured) {
        /* 通道已配置,优先使用当前分频(如果支持新频率) */
        if (!select_clk_divider(frequency_hz, current_shift, true, &target_shift)) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Unsupported frequency %u Hz", frequency_hz);
            return LISA_DEVICE_ERR_INVALID;
        }
    } else {
        /* 通道未配置,选择最合适的分频 */
        if (!select_clk_divider(frequency_hz, 0, false, &target_shift)) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Unsupported frequency %u Hz", frequency_hz);
            return LISA_DEVICE_ERR_INVALID;
        }
    }

    /* 如果需要更换分频,重新配置通道 */
    if (!is_configured || target_shift != current_shift) {
        ret = reconfigure_channel_divider(priv, channel, target_shift, &was_enabled);
        if (ret != LISA_DEVICE_OK) {
            DEVICE_UNLOCK(priv);
            return ret;
        }
    }

    /* 判断占空比类型变化:
     * - 边界值 (0% 或 100%): 通过极性控制输出静态电平,不启用 PWM 硬件
     * - 正常值 (1-99%): 通过 PWM 硬件输出波形,需要启用
     */
    bool old_is_boundary = (old_duty == 0 || old_duty == 100);
    bool new_is_boundary = (duty_cycle_percent == 0 || duty_cycle_percent == 100);
    bool was_enabled_before_set = priv->channels[channel].enabled;

    /* 特殊处理: 占空比为 0% 或 100% 时不启用 PWM,通过极性控制输出电平 */
    if (new_is_boundary) {
        /* 需要配置极性以实现正确的输出电平
         * - 占空比 0%: 需要输出"无效电平"
         *   - 正常极性(高电平有效): 输出低电平,使用反转极性
         *   - 反转极性(低电平有效): 输出高电平,使用正常极性
         * - 占空比 100%: 需要输出"有效电平"
         *   - 正常极性(高电平有效): 输出高电平,使用正常极性
         *   - 反转极性(低电平有效): 输出低电平,使用反转极性
         */
        lisa_pwm_polarity_t user_polarity = priv->channels[channel].polarity;
        uint32_t hal_polarity;

        if (duty_cycle_percent == 0) {
            /* 0% 占空比: 输出与用户极性相反的电平 */
            hal_polarity = (user_polarity == LISA_PWM_POLARITY_NORMAL)
                          ? CSK_GPT_PWM_OUTPOLARITY_LOW   /* 正常极性 -> 输出低电平 */
                          : CSK_GPT_PWM_OUTPOLARITY_HIGH; /* 反转极性 -> 输出高电平 */
        } else {
            /* 100% 占空比: 输出与用户极性相同的电平 */
            hal_polarity = (user_polarity == LISA_PWM_POLARITY_NORMAL)
                          ? CSK_GPT_PWM_OUTPOLARITY_HIGH  /* 正常极性 -> 输出高电平 */
                          : CSK_GPT_PWM_OUTPOLARITY_LOW;  /* 反转极性 -> 输出低电平 */
        }

        /* 配置 PWM 控制寄存器(设置极性但不启用) */
        uint32_t control = CSK_GPT_PWM_MODE |
                          CSK_GPT_PWM_CLKSRC_PCLK |
                          CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED |
                          hal_polarity |
                          clk_div_reg_value(target_shift) |
                          CSK_GPT_PWM_OPERATION_MODE_PWM;

        if (HAL_GPT_PWMControl(priv->hal_handler, control, hal_channel) != 0) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to configure PWM for duty 0%%/100%%");
            return LISA_DEVICE_ERR_IO;
        }

        /* 如果通道当前是启用的,需要禁用 PWM 硬件以使引脚输出静态电平 */
        if (priv->channels[channel].enabled) {
            if (HAL_GPT_DisablePWM(priv->hal_handler, hal_channel) != 0) {
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to disable PWM for duty 0%%/100%%");
                return LISA_DEVICE_ERR_IO;
            }
            /* 注意: 保持 enabled 标志为 true,表示逻辑上仍处于"启用"状态 */
        }
    } else {
        /* 占空比为 1-99%: 正常设置频率和占空比 */
        ret = HAL_GPT_SetPWMFreqDuty(priv->hal_handler, hal_channel, frequency_hz, duty_cycle_percent);
        if (ret != 0) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to set PWM frequency/duty: frequency %u Hz, duty %u%%",
                      frequency_hz, duty_cycle_percent);
            return LISA_DEVICE_ERR_IO;
        }

        /* 如果通道之前是启用的,或者从边界值切换到正常值时之前是启用状态,需要启用 PWM 硬件 */
        bool should_enable = was_enabled ||                    /* 重新配置后需要恢复启用状态 */
                            (was_enabled_before_set &&         /* 之前逻辑上是启用的 */
                             old_is_boundary);                 /* 且从边界值切换到正常值 */

        if (should_enable) {
            if (HAL_GPT_EnablePWM(priv->hal_handler, hal_channel) != 0) {
                DEVICE_UNLOCK(priv);
                LISA_LOGE(LOG_TAG, "Failed to enable PWM");
                return LISA_DEVICE_ERR_IO;
            }
            priv->channels[channel].enabled = true;
        }
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
    if (duty == 0 || duty == 100) {
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
 * @brief PWM0 设备初始化函数
 *
 * 初始化 PWM0 控制器，包括：
 * - 获取 HAL 句柄
 * - 创建互斥锁
 * - 初始化 HAL PWM
 * - 使能 PWM 电源
 * - 配置引脚复用
 *
 * @return int LISA_DEVICE_OK 成功，其他值表示错误
 */
static int arcs_pwm0_init(void)
{
    /* 清空私有数据 */
    memset(&pwm0_priv, 0, sizeof(lisa_pwm_priv_t));

    /* 获取 HAL PWM 句柄 */
    pwm0_priv.hal_handler = GPT0_PWM();
    if (!pwm0_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get GPT0_PWM handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 创建互斥锁 */
    pwm0_priv.mutex = lisa_mutex_create();
    if (!pwm0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL PWM */
    if (HAL_GPT_PWMInitialize(pwm0_priv.hal_handler, NULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize GPT PWM");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 使能 PWM 电源 */
    if (HAL_GPT_PWMPowerControl(pwm0_priv.hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on GPT PWM");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 设置最大通道数 */
    pwm0_priv.max_channels = MAX_PWM_CHANNELS;

    /* 初始化所有通道为默认配置（正常极性） */
    for (uint32_t i = 0; i < MAX_PWM_CHANNELS; i++) {
        pwm0_priv.channels[i].polarity = LISA_PWM_POLARITY_NORMAL;
        pwm0_priv.channels[i].configured = false;
        pwm0_priv.channels[i].enabled = false;
        pwm0_priv.channels[i].frequency_hz = 0;
        pwm0_priv.channels[i].duty_cycle_percent = 0;
        pwm0_priv.channels[i].clk_div_shift = 0;
    }

    lisa_pwm_pinmux();

    LISA_LOGI(LOG_TAG, "PWM0 initialized successfully (max_channels=%lu)", pwm0_priv.max_channels);

    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(pwm0,                        /* 设备名称 */
                     &arcs_pwm_api,               /* API指针 */
                     &pwm0_priv,                  /* 私有数据指针 */
                     NULL,                        /* 用户数据 */
                     arcs_pwm0_init,              /* 初始化函数 */
                     LISA_DEVICE_PRIORITY_NORMAL); /* 优先级 */

