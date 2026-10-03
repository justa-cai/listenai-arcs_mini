/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "autoconf.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_GAME_NES_FRAME_SKIP
#define CONFIG_GAME_NES_FRAME_SKIP 1
#endif

#ifndef CONFIG_GAME_NES_DRAW_FPS_TARGET
#define CONFIG_GAME_NES_DRAW_FPS_TARGET 0
#endif

#ifndef CONFIG_GAME_NES_PROFILE_LOG_MS
#define CONFIG_GAME_NES_PROFILE_LOG_MS 2000
#endif
#ifndef CONFIG_GAME_NES_CHR_PACKED_DYNAMIC_MAX_BYTES
#define CONFIG_GAME_NES_CHR_PACKED_DYNAMIC_MAX_BYTES 0
#endif

#ifdef CONFIG_GAME_NES_AUDIO_ENABLE
#define NES_ENABLE_SOUND        (1)
#else
#define NES_ENABLE_SOUND        (0)
#endif
#define NES_AUDIO_MIX_S16_ENABLE (1)
/* 卡带 SRAM ($6000-$7FFF, 8KB): MMC1 等 mapper 的存档/工作 RAM。需为 1, 否则
 * CPU 对 $6000-$7FFF 的写入被忽略(读为 0), 存档类游戏(如 MMC1)行为异常。 */
#ifndef NES_USE_SRAM
#define NES_USE_SRAM            (1)
#endif
#define NES_FRAME_SKIP          (CONFIG_GAME_NES_FRAME_SKIP)

/*
 * 16-bit RGB565 output for direct display flush.
 */
#define NES_COLOR_DEPTH         (16)
#define NES_COLOR_SWAP          (0)
#define NES_RAM_LACK            (0)

#define NES_USE_FS              (1)
#define NES_LOG_LEVEL           NES_LOG_LEVEL_INFO

#define CONFIG_NES_VISIBLE_X_OFFSET       8
#define CONFIG_NES_VISIBLE_WIDTH          240
#define CONFIG_NES_DRAW_FPS_TARGET        CONFIG_GAME_NES_DRAW_FPS_TARGET

#ifdef CONFIG_GAME_NES_PERF_LOG_ENABLE
#define CONFIG_NES_PORT_PROFILE_ENABLE    1
#else
#define CONFIG_NES_PORT_PROFILE_ENABLE    0
#endif
#ifdef CONFIG_GAME_NES_AUDIO_FAST_S16_COPY
#define CONFIG_NES_AUDIO_FAST_S16_COPY     1
#else
#define CONFIG_NES_AUDIO_FAST_S16_COPY     0
#endif
#ifdef CONFIG_GAME_NES_FAST_VISIBLE_RENDER
#define CONFIG_NES_FAST_VISIBLE_RENDER     1
#else
#define CONFIG_NES_FAST_VISIBLE_RENDER     0
#endif
#ifdef CONFIG_GAME_NES_FAST_PPU_PACKED_PATTERN
#define CONFIG_NES_FAST_PPU_PACKED_PATTERN 1
#else
#define CONFIG_NES_FAST_PPU_PACKED_PATTERN 0
#endif
#ifdef CONFIG_GAME_NES_FAST_CPU_BRANCH
#define CONFIG_NES_FAST_CPU_BRANCH         1
#else
#define CONFIG_NES_FAST_CPU_BRANCH         0
#endif
#ifdef CONFIG_GAME_NES_FAST_CPU_JMP_IDLE
#define CONFIG_NES_FAST_CPU_JMP_IDLE       1
#else
#define CONFIG_NES_FAST_CPU_JMP_IDLE       0
#endif
#ifdef CONFIG_GAME_NES_FAST_SKIP_SPRITES
#define CONFIG_NES_FAST_SKIP_SPRITES       1
#else
#define CONFIG_NES_FAST_SKIP_SPRITES       0
#endif
#ifdef CONFIG_GAME_NES_MAPPER_INFONES_ENABLE
#define CONFIG_NES_MAPPER_INFONES_ENABLE   1
#else
#define CONFIG_NES_MAPPER_INFONES_ENABLE   0
#endif
#ifdef CONFIG_GAME_NES_MAPPER_INFONES_DEFAULT_SET
#define CONFIG_NES_MAPPER_INFONES_DEFAULT_SET 1
#else
#define CONFIG_NES_MAPPER_INFONES_DEFAULT_SET 0
#endif

#define CONFIG_NES_FAST_INLINE_CPU_HELPERS 0
#define CONFIG_NES_FAST_LOCK_CPU_HELPERS   0
#define CONFIG_NES_FAST_SCANLINE_CPU       0
#ifdef CONFIG_GAME_NES_CORE_PROFILE_ENABLE
#define CONFIG_NES_CORE_PROFILE_ENABLE     1
#else
#define CONFIG_NES_CORE_PROFILE_ENABLE     0
#endif
#ifdef CONFIG_GAME_NES_CPU_OPCODE_PROFILE_ENABLE
#define CONFIG_NES_CPU_OPCODE_PROFILE_ENABLE 1
#else
#define CONFIG_NES_CPU_OPCODE_PROFILE_ENABLE 0
#endif
#define CONFIG_NES_CHR_PACKED_DYNAMIC_MAX_BYTES CONFIG_GAME_NES_CHR_PACKED_DYNAMIC_MAX_BYTES
#define CONFIG_NES_PROFILE_LOG_MS          CONFIG_GAME_NES_PROFILE_LOG_MS

/* Render pipeline: 1 = LVGL reads nes_draw_data directly (zero-copy),
 *                  0 = legacy memcpy via g_crop_buffer (fallback)
 */
#ifndef NES_LV_ZERO_COPY
#define NES_LV_ZERO_COPY (1)
#endif

/* Measurement spike: 1 = bypass LVGL entirely, push frames directly via
 * lisa_display_write. No on-screen UI, no joypad. Use for frame-rate
 * upper-bound measurement only.
 */
#ifndef NES_DIRECT_LCD
#define NES_DIRECT_LCD (0)
#endif

int nes_log_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif
