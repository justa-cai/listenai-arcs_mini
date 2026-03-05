/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio_dac_init.h"

/**
 * @brief DAC 平台初始化
 *
 * DAC 时钟和电源在 HAL 层初始化时自动配置
 * PA 功放控制已禁用,不需要 GPIO 配置
 */
int audio_dac_platform_init(void)
{
    /* DAC 时钟和电源在 HAL 层自动配置 */
    /* 不包含 PA 控制,无需 GPIO 配置 */

    return 0;
}
