/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA ADC 基础读取示例
 *
 * 本示例演示如何使用 LISA ADC 驱动读取模拟信号：
 * 1. 初始化ADC设备
 * 2. 配置ADC通道参数（参考电压和分辨率）
 * 3. 循环读取ADC通道数值
 * 4. 转换为电压值并显示
 */
#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_adc.h"
#include "IOMuxManager.h"

#include "FreeRTOS.h"
#include "task.h"

#define ADC_CHANNEL    2
#define ADC_PIN        4       // GPIOB_04 -> Channel 2
#define ADC_DEVICE     "adc0"

/*
    为满足不同板型示例场景，重定向ADC设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_adc_pinmux()
{
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, ADC_PIN, CSK_AON_IOMUX_FUNC_ALTER3);
}
#endif


int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA ADC read example ===\n");

    /* 获取ADC设备 */
    lisa_device_t *adc_dev = lisa_device_get(ADC_DEVICE);
    if (!lisa_device_ready(adc_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready\n", ADC_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready\n", ADC_DEVICE);

    /* 配置ADC通道：使用3.6V参考电压，10-bit分辨率 */
    lisa_adc_channel_config_t ch_config = {
        .reference = LISA_ADC_REF_VDD_3V6,
        .resolution = LISA_ADC_RESOLUTION_10BIT,
    };
    int ret = lisa_adc_channel_setup(adc_dev, ADC_CHANNEL, &ch_config);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to configure channel %d (code: %d)\n", ADC_CHANNEL, ret);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "Channel %d configured: 3.6V reference, 10-bit resolution\n", ADC_CHANNEL);

    LISA_LOGI(LOG_TAG, "Start reading ADC channel %d...\n\n", ADC_CHANNEL);

    while (1) {
        uint16_t raw_value = 0;
        ret = lisa_adc_read(adc_dev, ADC_CHANNEL, &raw_value);

        if (ret == 0) {
            /* 转换为电压值 (mV) */
            uint32_t voltage = LISA_ADC_RAW_TO_MV(raw_value, 3600, 10);
            LISA_LOGI(LOG_TAG, "Channel %d: raw=0x%04x (%4u), voltage=%4lu mV\n",
                   ADC_CHANNEL, raw_value, raw_value, voltage);
        } else {
            LISA_LOGE(LOG_TAG, "Error: ADC read failed (code: %d)\n", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
