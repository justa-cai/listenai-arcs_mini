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

/*
 * 长循环（卡扫描重试等）中的协作 yield 钩子。驱动本身不假设 caller 拿
 * 这个 hook 做什么——喂自家看门狗、检查超时、上报进度等皆可。NULL =
 * no-op（默认）。
 *
 * 与其他 ops 同款 lifetime 约定：单线程 init 路径一次性设置，不要在
 * in-flight scan 中替换。
 */
struct sdmmc_runtime_ops {
    void (*yield)(void);
};

void sdmmc_set_runtime_ops(const struct sdmmc_runtime_ops *ops);

#ifdef __cplusplus
}
#endif
