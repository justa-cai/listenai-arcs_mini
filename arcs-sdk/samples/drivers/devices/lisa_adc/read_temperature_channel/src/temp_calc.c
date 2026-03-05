/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file temp_calc.c
 * @brief 温度传感器计算实现
 */

#include "temp_calc.h"

float adc_raw_to_voltage(uint16_t raw_value, float reference_v, uint8_t resolution)
{
    uint32_t max_value = (1UL << resolution);  // 2^resolution
    return (float)raw_value / (float)max_value * reference_v;
}

float calc_temperature(const temp_calibration_t *cal, float voltage)
{
    if (!cal) {
        return 0.0f;
    }

    /* 计算公式:
     * temperature = temp_cal + (temp_cal + 273.15) / voltage_cal * (voltage - voltage_cal)
     *
     * 公式推导：
     * 1. 传感器电压与绝对温度成正比: voltage / T = k (常数)
     * 2. 在校准点: voltage_cal / T_cal = k
     * 3. 在测量点: voltage / T = k
     * 4. 因此: T = T_cal * (voltage / voltage_cal)
     * 5. 温度变化量: ΔT = T - T_cal = T_cal * (voltage - voltage_cal) / voltage_cal
     * 6. 当前温度(℃): temperature = temp_cal + ΔT
     *
     * 其中 T_cal = temp_cal + 273.15 (绝对温度K)
     */
    float temp_abs_cal = cal->temp_cal + 273.15f;  // 校准点绝对温度 (K)
    float k = temp_abs_cal / cal->voltage_cal;     // 温度-电压转换系数 (K/V)
    float delta_temp = k * (voltage - cal->voltage_cal); // 温度变化量 (K 或 ℃)

    return cal->temp_cal + delta_temp;
}
