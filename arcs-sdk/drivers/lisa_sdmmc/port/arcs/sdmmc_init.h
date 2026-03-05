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
 * @brief 平台电源和时钟初始化（在设备注册时调用）
 *
 * 配置 SDMMC 硬件时钟、电源和 DMA 缓冲区
 *
 * @return 0 成功, 负值表示错误
 */
int sdmmc_platform_init(void);

/**
 * @brief 探测 SD/MMC 卡并配置（在 probe 时调用）
 *
 * 执行卡探测、总线宽度配置等操作
 *
 * @return 0 成功, 负值表示错误
 */
int sdmmc_hard_init(void);

#ifdef __cplusplus
}
#endif
