/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_adc.h
 * @brief LISA ADC 设备驱动接口示例
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ADC 类型定义
 * ======================================================================== */

#define LISA_ADC_VALUE_INVALID 0xFFFFU /**< 无效的 ADC 采样占位值 */

/**
 * @brief ADC 参考电压枚举
 *
 * 定义 ADC 支持的参考电压类型。
 * - 固定电压类型直接在枚举名中标明电压值(如 LISA_ADC_REF_VDD_1V2)
 * - 可变电压类型需要用户根据硬件电路确定实际电压值
 *
 * @warning 硬件限制: 实际可测量的外部电压不能超过 VDD_IO (默认 3.3V)。
 *          超过此电压可能损坏芯片。请确保输入信号经过适当的分压或限幅处理。
 *
 * @note LISA_ADC_REF_VDD_IO_AUTO 比较特殊:
 *       硬件会自动根据外部 VDD_IO 电压选择内部分压系数:
 *       - 当 VDD_IO ≤ 2.4V 时，分压系数为 1/2 (参考电压 = VDD_IO/2)
 *       - 当 VDD_IO > 2.4V 时，分压系数为 1/3 (参考电压 = VDD_IO/3)
 */
typedef enum {
    LISA_ADC_REF_VDD_1V2 = 0,         /**< 固定 1.2V 参考电压 (内部 Bandgap) */
    LISA_ADC_REF_VDD_3V6 = 1,         /**< 固定 3.6V 参考电压 (Vbg 1.2V + 3倍缓冲) */
    LISA_ADC_REF_VDD_IO_AUTO = 2,     /**< VDD_IO 自动分压: VDD_IO/2(≤2.4V) 或 VDD_IO/3(>2.4V) */
    LISA_ADC_REF_VDD_IO_AUTO_MUL3 = 3,/**< VDD_IO 自动分压 + 3倍缓冲 */
    LISA_ADC_REF_EXTERNAL = 4,        /**< 外部参考电压 Vref_ext (依赖硬件外部参考) */
} lisa_adc_reference_t;

/**
 * @brief ADC 分辨率枚举
 */
typedef enum {
    LISA_ADC_RESOLUTION_10BIT = 10,   /**< 10-bit 分辨率 (0-1023) */
} lisa_adc_resolution_t;

/**
 * @brief ADC 通道配置结构体
 *
 * 用于配置单个 ADC 通道的采样参数,使不同通道可以使用不同的参考电压和分辨率。
 */
typedef struct {
    lisa_adc_reference_t reference;   /**< 参考电压选择 */
    lisa_adc_resolution_t resolution; /**< ADC 分辨率 */
} lisa_adc_channel_config_t;

/* ========================================================================
 * ADC 设备 API 结构体
 * ======================================================================== */

typedef struct {
    /**
     * @brief 读取单次 ADC 转换结果
     *
     * @param dev     ADC 设备指针
     * @param channel 通道号（由具体驱动定义范围）
     * @param value   输出采样值指针
     *
     * @return 0 成功
     * @return LISA_DEVICE_ERR_INVALID 参数无效
     * @return LISA_DEVICE_ERR_NOT_SUPPORT 驱动未实现 read 功能
     * @return 其他负值 具体错误码，详见 lisa_device.h
     */
    int (*read)(lisa_device_t *dev, uint32_t channel, uint16_t *value);

    /**
     * @brief 配置 ADC 通道参数 (可选)
     *
     * @param dev     ADC 设备指针
     * @param channel 通道号
     * @param config  通道配置参数
     *
     * @return 0 成功
     * @return LISA_DEVICE_ERR_NOT_SUPPORT 驱动未实现或不支持该配置
     * @return 其他负值 具体错误码
     */
    int (*channel_setup)(lisa_device_t *dev, uint32_t channel,
                         const lisa_adc_channel_config_t *config);
} lisa_adc_api_t;

/* ========================================================================
 * ADC 对外接口函数
 * ======================================================================== */

/**
 * @brief 配置 ADC 通道
 *
 * 在读取通道之前调用,用于设置该通道的参考电压和分辨率。
 * 不同通道可以配置不同的参数,以适应不同的测量需求。
 *
 * @param dev     ADC 设备指针
 * @param channel 通道号
 * @param config  通道配置参数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 驱动未实现或不支持该配置
 */
static inline int lisa_adc_channel_setup(lisa_device_t *dev, uint32_t channel,
                                          const lisa_adc_channel_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_adc_api_t *api = (lisa_adc_api_t *)dev->api;
    if (!api->channel_setup) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->channel_setup(dev, channel, config);
}

/**
 * @brief 读取单次 ADC 转换结果
 *
 * 该示例仅提供最小接口：通过驱动实现的 @ref lisa_adc_api_t::read
 * 直接获取指定通道的原始采样值。
 *
 * @param dev     ADC 设备指针
 * @param channel 通道号（由具体驱动定义范围）
 * @param value   输出采样值指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 驱动未实现 read 功能
 * @return 其他负值 具体错误码，详见 lisa_device.h
 */
static inline int lisa_adc_read(lisa_device_t *dev, uint32_t channel, uint16_t *value)
{
    if (!dev || !dev->api || !value) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_adc_api_t *api = (lisa_adc_api_t *)dev->api;
    return api->read ? api->read(dev, channel, value) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 辅助宏: 将 ADC 原始值转换为电压(毫伏)
 *
 * @param raw_value     ADC 原始采样值
 * @param reference_mv  参考电压(毫伏)
 * @param resolution    分辨率(bits)
 *
 * @return 电压值(毫伏)
 */
#define LISA_ADC_RAW_TO_MV(raw_value, reference_mv, resolution) \
    (((uint32_t)(raw_value) * (reference_mv)) / (1UL << (resolution)))

#ifdef __cplusplus
}
#endif
