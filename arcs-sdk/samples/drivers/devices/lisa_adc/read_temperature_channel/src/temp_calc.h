/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file temp_calc.h
 * @brief 温度传感器计算接口
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 温度传感器校准参数结构体
 *
 * 用于存储温度传感器的校准数据。
 * 温度传感器输出电压与绝对温度成正比关系。
 */
typedef struct {
    float temp_cal;       /**< 校准点温度 (℃) */
    float voltage_cal;    /**< 校准点对应的电压值 (V) */
} temp_calibration_t;

/**
 * @brief 将 ADC 原始值转换为电压
 *
 * @param raw_value     ADC 原始采样值
 * @param reference_v   参考电压 (V)
 * @param resolution    分辨率 (bits)
 *
 * @return 传感器电压值 (V)
 *
 * @example
 * @code
 * uint16_t raw = 600;
 * float voltage = adc_raw_to_voltage(raw, 1.2f, 10);  // 10-bit, 1.2V 参考
 * @endcode
 */
float adc_raw_to_voltage(uint16_t raw_value, float reference_v, uint8_t resolution);

/**
 * @brief 基于传感器电压计算芯片温度
 *
 * 使用单点校准的线性温度计算公式：
 *   temperature = temp_cal + (temp_cal + 273.15) / voltage_cal * (voltage - voltage_cal)
 *
 * 原理说明：
 *   传感器电压与绝对温度成正比: V ∝ T(K)
 *   通过校准点建立电压-温度关系，计算当前温度偏移量
 *
 * @param cal     校准参数结构体指针
 * @param voltage 当前测量的传感器电压值 (V)
 *
 * @return 计算得到的芯片温度 (℃)
 *
 * @note 计算精度依赖于校准参数的准确性
 * @note 建议在稳定温度环境下进行校准
 *
 * @example 使用示例：
 * @code
 * // 定义校准参数（通常在生产时测定）
 * temp_calibration_t cal = {
 *     .temp_cal = 26.3,              // 校准温度 26.3℃
 *     .voltage_cal = 0.70819921875   // 对应电压 0.708V
 * };
 *
 * // 读取 ADC 并转换为电压
 * uint16_t raw;
 * lisa_adc_read(adc_dev, 7, &raw);
 * float voltage = adc_raw_to_voltage(raw, 1.2f, 10);
 *
 * // 计算温度
 * float temperature = calc_temperature(&cal, voltage);
 * @endcode
 */
float calc_temperature(const temp_calibration_t *cal, float voltage);

#ifdef __cplusplus
}
#endif
