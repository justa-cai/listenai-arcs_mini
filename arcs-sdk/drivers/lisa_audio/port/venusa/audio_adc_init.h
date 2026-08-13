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
 * 具体 MIC 引脚由 board 的 lisa_capture_pinmux() / lisa_dmic_pinmux()
 * 提供，驱动层只负责调用板级 hook。
 *
 * @return 0 成功, 负数失败
 */
int audio_adc_gpio_init(void);

#ifdef __cplusplus
}
#endif
