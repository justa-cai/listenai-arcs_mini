/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 挂载 LCD 总线并发 panel 初始化序列，返回后屏幕仍暗（未 DISPLAY_ON / 未开背光）。*/
void boot_display_init(void);

/* 清屏、绘制 recovery 提示图，再打开 DISPLAY_ON 和背光。*/
void boot_display_show_recovery(void);

/* 清屏、绘制充电提示图（关机态插 USB），再打开 DISPLAY_ON 和背光。*/
void boot_display_show_charging(void);

/* 清屏、绘制 OTA 升级提示图，再打开 DISPLAY_ON 和背光。*/
void boot_display_show_ota(void);

/* 更新 OTA 进度条，percent 为 0..100。需 boot_display_show_ota 已调。*/
void boot_display_ota_progress(uint8_t percent);

#ifdef __cplusplus
}
#endif
