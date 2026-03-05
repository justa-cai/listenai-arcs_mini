/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA ADC 温度传感器读取示例
 *
 * 本示例演示如何使用 LISA ADC 驱动读取内部温度传感器：
 * 1. 初始化ADC设备
 * 2. 配置温度传感器通道参数（使用1.2V参考电压和10-bit分辨率）
 * 3. 循环读取温度传感器通道数值
 * 4. 转换为电压值并显示
 *
 * @note 温度传感器为内部通道7，无需配置PINMUX
 */
#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_adc.h"
#include "IOMuxManager.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "temp_calc.h"

#define ADC_CHANNEL_TEMP    7         // 内部温度传感器通道
#define ADC_DEVICE          "adc0"

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA ADC Temperature Sensor Read Example ===\n");

    /* 获取ADC设备 */
    lisa_device_t *adc_dev = lisa_device_get(ADC_DEVICE);
    if (!lisa_device_ready(adc_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready\n", ADC_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready\n", ADC_DEVICE);

    /* 配置温度传感器通道：使用1.2V参考电压，10-bit分辨率 */
    lisa_adc_channel_config_t temp_config = {
        .reference = LISA_ADC_REF_VDD_1V2,
        .resolution = LISA_ADC_RESOLUTION_10BIT,
    };
    int ret = lisa_adc_channel_setup(adc_dev, ADC_CHANNEL_TEMP, &temp_config);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to configure temperature channel %d (code: %d)\n", ADC_CHANNEL_TEMP, ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Temperature sensor channel %d configured: 1.2V reference, 10-bit resolution\n", ADC_CHANNEL_TEMP);

    /* 配置温度校准参数
     * 注意：这些参数需要根据实际测量环境进行调整
     * - temp_cal: 校准时的环境温度，需考虑芯片自热效应
     * - voltage_cal: 校准温度下测得的传感器电压值
     */
    temp_calibration_t temp_cal = {
        .temp_cal = 29.8f,          // 校准温度
        .voltage_cal = 0.73125f // 校准点传感器电压 (V)
    };

    LISA_LOGI(LOG_TAG, "Start reading temperature sensor (channel %d)...\n\n", ADC_CHANNEL_TEMP);

    while (1) {
        uint16_t temp_raw = 0;
        ret = lisa_adc_read(adc_dev, ADC_CHANNEL_TEMP, &temp_raw);

        if (ret == 0) {
            /* 转换为传感器电压值 (V) */
            float voltage = adc_raw_to_voltage(temp_raw, 1.2f, 10);  // 1.2V 参考, 10-bit

            /* 计算芯片温度 */
            float die_temp = calc_temperature(&temp_cal, voltage);

            LISA_LOGI(LOG_TAG, "Temperature: raw=0x%x (%u), voltage=%fV, temp=%.2f C\n",
                   temp_raw, temp_raw, voltage, die_temp);
        } else {
            LISA_LOGE(LOG_TAG, "Error: Temperature sensor read failed (code: %d)\n", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
