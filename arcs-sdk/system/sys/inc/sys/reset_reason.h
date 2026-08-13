/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LISA_SYS_RESET_REASON_H_
#define LISA_SYS_RESET_REASON_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sys/reset_reason.h
 * @brief SDK-level reset-reason API (chip-agnostic).
 *
 * Bitmap layout:
 *   bit  0..15  通用类别区，所有芯片必须共享同一含义
 *   bit 16..30  芯片特定扩展区（当前未暴露任何宏，将来按需开放）
 *   bit 31      UNKNOWN：硬件位非零但未匹配到任何已知类别
 *
 * 语义约定：
 *   - 多个原因可同时置位，调用方应使用按位与判断（r & SYS_RESET_REASON_*）。
 *   - 当硬件无法分辨两个原因时，SDK 会同时置位（例如 ARCS 上 POR 与 PAD
 *     共用一位，则 POR | PIN 两位同时置位），调用方按其中任一处理永远
 *     不会漏判。
 *   - 返回 0 表示"无信息"（HAL 快照为零）；
 *     SYS_RESET_REASON_UNKNOWN 表示"硬件有原因但未识别"。
 */

typedef uint32_t sys_reset_reason_t;

#define SYS_RESET_REASON_POR        (1u << 0)   /* 上电复位（Power-On Reset） */
#define SYS_RESET_REASON_PIN        (1u << 1)   /* 外部 nRST 引脚复位         */
#define SYS_RESET_REASON_WATCHDOG   (1u << 2)   /* 任意 WDT 触发              */
#define SYS_RESET_REASON_SOFTWARE   (1u << 3)   /* 软件请求复位               */

#define SYS_RESET_REASON_UNKNOWN    (1u << 31)  /* 硬件位非零但未匹配         */

/**
 * @brief Read the latched reset-reason bitmap for this boot.
 *
 * Snapshot is captured at boot time by HAL; subsequent calls return the
 * same value within one boot session.
 *
 * @return Bitmask of SYS_RESET_REASON_* values; zero if no cause was
 *         latched.
 */
sys_reset_reason_t sys_reset_reason_get(void);

/**
 * @brief Latch the chip reset cause into SDK storage (idempotent).
 *
 * 必须在启动早期、任何调用 sys_reset_reason_get() 之前被调用一次。
 * SDK 的 system_entry() 已经在 SYS_INIT 之前调用本函数；
 * 应用代码通常不需要调用。
 *
 * 内部保有 done flag，重复调用是 no-op —— 防止误用导致硬件已清后
 * 二次读取覆盖 boot snapshot。
 *
 * 首次调用副作用：清除底层硬件状态寄存器，让下次复位周期从干净状态开始。
 */
void sys_reset_reason_snapshot(void);

/**
 * @brief Print a one-line reset-reason banner to stdout.
 *
 * Invoked by the SDK boot sequence right after boot_banner() when
 * CONFIG_BANNER is enabled. Format:
 *   "Reset reason: 0xNNNNNNNN [LABEL ...]"
 */
void sys_reset_reason_banner(void);

#ifdef __cplusplus
}
#endif

#endif /* LISA_SYS_RESET_REASON_H_ */
