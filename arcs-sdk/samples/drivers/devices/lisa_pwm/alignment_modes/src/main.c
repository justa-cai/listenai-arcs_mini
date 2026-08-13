/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA PWM 边沿对齐和中心对齐模式示例
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "IOMuxManager.h"
#include "lisa_device.h"
#include "lisa_pwm.h"

#define PWM_DEVICE          "pwm0"
#define PWM_CHANNEL         0
#define PWM_FREQUENCY_HZ    5000
#define PWM_DUTY_PERCENT    30
#define PWM_STEP_DELAY_MS   3000

#if defined(CONFIG_BOARD_ARCS_EVB)
#define PWM_PAD             CSK_IOMUX_PAD_A
#define PWM_PIN             20
#define PWM_PIN_FUNC        12
#define PWM_OUTPUT_PIN_NAME "PA20"
#define PWM_PINMUX_OVERRIDDEN 1
#elif defined(CONFIG_BOARD_VENUSA_RD_EVB)
#define PWM_PAD             CSK_IOMUX_PAD_A
#define PWM_PIN             14
#define PWM_PIN_FUNC        CSK_IOMUX_FUNC_ALTER11
#define PWM_OUTPUT_PIN_NAME "PA14"
#define PWM_PINMUX_OVERRIDDEN 1
#else
#define PWM_OUTPUT_PIN_NAME "board default PWM0 channel 0 pin"
#define PWM_PINMUX_OVERRIDDEN 0
#endif

/*
 * 为满足示例观测场景，重定向 pwm0 channel 0 到板级可观测引脚。
 * 可使用示波器或逻辑分析仪观察输出模式变化。
 */
#if defined(PWM_PIN)
void lisa_pwm_pinmux(void)
{
    IOMuxManager_PinConfigure(PWM_PAD, PWM_PIN, PWM_PIN_FUNC);
}
#endif

typedef struct {
    const char *name;
    lisa_pwm_mode_t mode;
    lisa_pwm_polarity_t polarity;
} pwm_mode_case_t;

static const pwm_mode_case_t pwm_cases[] = {
    {
        .name = "edge aligned normal",
        .mode = LISA_PWM_MODE_EDGE_ALIGNED,
        .polarity = LISA_PWM_POLARITY_NORMAL,
    },
    {
        .name = "edge aligned inverted",
        .mode = LISA_PWM_MODE_EDGE_ALIGNED,
        .polarity = LISA_PWM_POLARITY_INVERTED,
    },
#if !defined(CONFIG_SOC_VENUSA)
    {
        .name = "center aligned normal",
        .mode = LISA_PWM_MODE_CENTER_ALIGNED,
        .polarity = LISA_PWM_POLARITY_NORMAL,
    },
    {
        .name = "center aligned inverted",
        .mode = LISA_PWM_MODE_CENTER_ALIGNED,
        .polarity = LISA_PWM_POLARITY_INVERTED,
    },
#endif
};

static int apply_pwm_case(lisa_device_t *pwm_dev, const pwm_mode_case_t *test_case)
{
    lisa_pwm_config_t config = {
        .polarity = test_case->polarity,
        .mode = test_case->mode,
    };

    int ret = lisa_pwm_configure(pwm_dev, PWM_CHANNEL, &config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Configure %s failed: %d", test_case->name, ret);
        return ret;
    }

    ret = lisa_pwm_set(pwm_dev, PWM_CHANNEL, PWM_FREQUENCY_HZ, PWM_DUTY_PERCENT);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Set %s failed: %d", test_case->name, ret);
        return ret;
    }

    ret = lisa_pwm_enable(pwm_dev, PWM_CHANNEL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Enable %s failed: %d", test_case->name, ret);
        return ret;
    }

    LISA_LOGI(LOG_TAG, "%s: %uHz, %u%% duty", test_case->name, PWM_FREQUENCY_HZ, PWM_DUTY_PERCENT);
    return LISA_DEVICE_OK;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LISA_LOGI(LOG_TAG, "=== LISA PWM alignment modes example ===");

    lisa_device_t *pwm_dev = lisa_device_get(PWM_DEVICE);
    if (!lisa_device_ready(pwm_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", PWM_DEVICE);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "%s device ready", PWM_DEVICE);
    if (!PWM_PINMUX_OVERRIDDEN) {
        LISA_LOGW(LOG_TAG, "No board-specific PWM pinmux override; verify %s before probing",
                  PWM_OUTPUT_PIN_NAME);
    } else {
        LISA_LOGI(LOG_TAG, "Observe PWM output on %s", PWM_OUTPUT_PIN_NAME);
    }
    LISA_LOGI(LOG_TAG, "PWM alignment modes example running");
#if defined(CONFIG_SOC_VENUSA)
    LISA_LOGW(LOG_TAG, "Venusa GPT PWM HAL has no center-aligned selector; run edge-aligned polarity cases only");
#endif

    while (1) {
        for (uint32_t i = 0; i < sizeof(pwm_cases) / sizeof(pwm_cases[0]); i++) {
            if (apply_pwm_case(pwm_dev, &pwm_cases[i]) != LISA_DEVICE_OK) {
                return -1;
            }
            vTaskDelay(pdMS_TO_TICKS(PWM_STEP_DELAY_MS));
        }
    }

    return 0;
}
