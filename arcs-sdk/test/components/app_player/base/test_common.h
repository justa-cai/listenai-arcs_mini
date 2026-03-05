/*
 * LISA App Player Component - 测试公共头文件
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "app_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================
 * 测试常量定义
 * ======================================== */

/** 测试用播放器名称 */
#define TEST_PLAYER_NAME "test_player"

/** 测试用网络URL - 可统一修改 */
#define TEST_URL "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3"

/** 测试用等待时间(毫秒) */
#define TEST_WAIT_SHORT_MS    100
#define TEST_WAIT_MEDIUM_MS   500
#define TEST_WAIT_LONG_MS     2000

/* ========================================
 * 测试运行器声明
 * ======================================== */

void run_player_lifecycle_tests(void);
void run_player_callback_tests(void);
void run_player_state_tests(void);
void run_player_play_tests(void);
void run_player_control_tests(void);
void run_player_seek_tests(void);
void run_player_volume_tests(void);
void run_player_stream_tests(void);

/* ========================================
 * 辅助函数声明
 * ======================================== */

/**
 * @brief 等待指定毫秒数
 * @param ms 等待时间(毫秒)
 */
void wait_ms(uint32_t ms);

/**
 * @brief 将状态码转换为字符串
 * @param state 播放器状态
 * @return 状态名称字符串
 */
const char* state_to_string(app_player_state_t state);

/**
 * @brief 将事件码转换为字符串
 * @param event 播放器事件
 * @return 事件名称字符串
 */
const char* test_event_to_string(app_player_event_t event);

/**
 * @brief 将错误码转换为字符串
 * @param error 错误码
 * @return 错误名称字符串
 */
const char* error_to_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* TEST_COMMON_H */
