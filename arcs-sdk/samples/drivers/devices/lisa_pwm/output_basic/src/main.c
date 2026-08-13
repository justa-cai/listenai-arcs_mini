/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA PWM 基础输出示例
 * 
 * 本示例演示如何使用 LISA PWM 驱动输出固定频率和占空比的信号：
 * 1. 初始化PWM设备
 * 2. 配置引脚为PWM输出
 * 3. 设置PWM参数并启动输出
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_pwm.h"
#include "IOMuxManager.h"
#include "pinmux.h"

#include "FreeRTOS.h"
#include "task.h"

#define PWM_CHANNEL    0
#ifdef CONFIG_BOARD_ARCS_MINI
#define PWM_PIN        LCD_PWM_PIN
#else
#define PWM_PIN        20
#endif
#define PWM_DEVICE     "pwm0"

#if defined(CONFIG_BOARD_ARCS_EVB)
#define PWM_PAD        CSK_IOMUX_PAD_A
#define PWM_PIN        20
#define PWM_PIN_FUNC   12
#elif defined(CONFIG_BOARD_VENUSA_RD_EVB)
#define PWM_PAD        CSK_IOMUX_PAD_A
#define PWM_PIN        14
#define PWM_PIN_FUNC   CSK_IOMUX_FUNC_ALTER11
#endif

/*
    为满足不同板型示例场景，重定向pwm设备的pinmux配置
*/
#if defined(PWM_PIN)
void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(PWM_PAD, PWM_PIN, PWM_PIN_FUNC);
}
#endif

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== LISA PWM output example ===");

    lisa_device_t *pwm_dev = lisa_device_get(PWM_DEVICE);
    if (!lisa_device_ready(pwm_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", PWM_DEVICE);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "%s device ready", PWM_DEVICE);

    /* 配置通道 0 为边沿对齐 + 正常极性 */
    lisa_pwm_config_t config = {
        .polarity = LISA_PWM_POLARITY_NORMAL,
        .mode = LISA_PWM_MODE_EDGE_ALIGNED,
    };
    int ret = lisa_pwm_configure(pwm_dev, PWM_CHANNEL, &config);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: PWM configure failed (code: %d)", ret);
        return -1;
    }

    /* 设置 PWM 频率和占空比 */
    uint32_t duty_cycle = 50; // 占空比百分比
    ret = lisa_pwm_set(pwm_dev, PWM_CHANNEL, 5000, duty_cycle);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: PWM set failed (code: %d)", ret);
        return -1;
    }

    /* 启用 PWM 输出 */
    ret = lisa_pwm_enable(pwm_dev, PWM_CHANNEL);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: PWM enable failed (code: %d)", ret);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "PWM enabled: 5kHz, %u%% duty cycle", duty_cycle);

    /* 保持运行 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}
