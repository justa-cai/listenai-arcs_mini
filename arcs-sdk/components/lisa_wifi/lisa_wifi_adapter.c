/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_wifi_adapter.c
 * @brief 实现wifi内部函数的重定向
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_adc.h"

#define LOG_TAG "lisa_wifi_adapter"
#include <lisa_log.h>

#define ADC_CHANNEL_TEMP    7         // 内部温度传感器通道
#define ADC_DEVICE          "adc0"

int ls_read_temp_voltage(int count, float *vout)
{
    int ret = 0;
    uint16_t temp_raw = 0;
    count = count;

    /* 获取ADC设备 */
    lisa_device_t *adc_dev = lisa_device_get(ADC_DEVICE);
    if (!lisa_device_ready(adc_dev)) {
        LOGE("Error: %s device not ready\n", ADC_DEVICE);
        return -1;
    }

    /* 配置温度传感器通道：使用1.2V参考电压，10-bit分辨率 */
    lisa_adc_channel_config_t temp_config = {
        .reference = LISA_ADC_REF_VDD_1V2,
        .resolution = LISA_ADC_RESOLUTION_10BIT,
    };
    ret = lisa_adc_channel_setup(adc_dev, ADC_CHANNEL_TEMP, &temp_config);
    if (ret != 0) {
        LOGE("Error: Failed to configure temperature channel %d (code: %d)\n", ADC_CHANNEL_TEMP, ret);
        return -1;
    }

    ret = lisa_adc_read(adc_dev, ADC_CHANNEL_TEMP, &temp_raw);
    if (ret != 0) {
        LOGE("Error: Failed to read temperature channel %d (code: %d)\n", ADC_CHANNEL_TEMP, ret);
        return -1;
    }

    /* 将ADC原始值转换为实际电压值
     * 原始值: temp_raw (0-1023)
     * 最大值: 1024 (10-bit分辨率: 2^10 = 1024)
     * 参考电压: 1.2V (LISA_ADC_REF_VDD_1V2)
     */
    *vout = temp_raw / 1024.0 * 1.2;
           
    return ret;
}