/**
 * @file config.h
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2023-04-17
 *
 * @copyright Copyright (c) 2023
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_LOG_ENABLE
#define CONFIG_LOG_LEVEL 5
#endif

#if 1
// #define SDL_HOR_RES (360) // 320
// #define SDL_VER_RES (360) // 240

#define SDL_HOR_RES (320) // 320
#define SDL_VER_RES (240) // 240
#else
#define SDL_HOR_RES (600)
#define SDL_VER_RES (400)
#endif

#define LV_COLOR_DEPTH 16

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H__ */