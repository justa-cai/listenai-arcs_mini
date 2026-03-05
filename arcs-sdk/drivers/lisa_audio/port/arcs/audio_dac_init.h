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
 * @brief DAC 平台初始化
 *
 * 初始化 DAC 时钟和电源
 *
 * @return 0 成功, 负数失败
 */
int audio_dac_platform_init(void);

#ifdef __cplusplus
}
#endif
