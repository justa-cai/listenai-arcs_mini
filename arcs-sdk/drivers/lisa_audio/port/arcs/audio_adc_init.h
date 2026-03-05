/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADC 平台初始化
 *
 * 配置 GPIO/IOMUX 引脚,初始化时钟和电源
 *
 * @return 0 成功, 负数失败
 */
int audio_adc_platform_init(void);

/**
 * @brief ADC GPIO 引脚配置
 *
 * 配置 MIC 引脚为差分输入模式
 * GPIOA_28: MIC1_INP
 * GPIOA_29: MIC1_INN
 * GPIOA_30: MIC0_INP
 * GPIOA_31: MIC0_INN
 *
 * @return 0 成功, 负数失败
 */
int audio_adc_gpio_init(void);

#ifdef __cplusplus
}
#endif
