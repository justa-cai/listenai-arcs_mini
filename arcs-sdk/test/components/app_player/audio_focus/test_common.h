/*
 * LISA App Player Component - 焦点管理测试公共头文件
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "app_player.h"
#include "tone.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================
 * 场景播放器URL定义（统一管理，方便修改）
 * ======================================== */

/** TTS播放器URL */
#define TEST_TTS_URL "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3"

/** MUSIC播放器URL */
#define TEST_MUSIC_URL "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/haier-ac/%E9%9D%92%E8%97%8F%E9%AB%98%E5%8E%9F.mp3"

/** ALARM播放器URL */
#define TEST_ALARM_URL "https://iflyos-external.oss-cn-shanghai.aliyuncs.com/public/duomotai/player_test_url/tts.mp3"

/** TONE播放器使用本地音频文件，通过app_tone_get_url(TONE_ID_0)获取 */

/* ========================================
 * 场景播放器名称定义
 * ======================================== */

#define PLAYER_NAME_TTS   "tts"
#define PLAYER_NAME_MUSIC "music"
#define PLAYER_NAME_TONE  "tone"
#define PLAYER_NAME_ALARM "alarm"

/* ========================================
 * 测试用等待时间(毫秒)
 * ======================================== */

#define TEST_WAIT_SHORT_MS    100   // 短等待：状态检查
#define TEST_WAIT_MEDIUM_MS   500   // 中等等待：事件触发
#define TEST_WAIT_LONG_MS     2000  // 长等待：播放测试
#define TEST_WAIT_PLAYBACK_MS 5000  // 播放等待：等待音频播放

/* ========================================
 * 全局播放器实例（测试中共享）
 * ======================================== */

extern app_player_t *g_tts_player;
extern app_player_t *g_music_player;
extern app_player_t *g_tone_player;
extern app_player_t *g_alarm_player;

/* ========================================
 * 焦点管理配置
 * ======================================== */

/**
 * @brief 获取焦点通道配置
 * @param count 输出配置数组长度
 * @return 焦点配置数组指针
 */
const app_player_focus_channel_config_t* get_focus_configs(int *count);

/* ========================================
 * 测试运行器声明
 * ======================================== */

/**
 * @brief 运行焦点抢占测试（基础功能测试）
 */
void run_focus_preempt_tests(void);

/**
 * @brief 运行焦点行为策略测试（测试不同的 app_player_focus_behavior_t 配置）
 */
void run_focus_behavior_tests(void);

/**
 * @brief 运行焦点回调机制测试（测试焦点变化回调的各种场景）
 */
void run_focus_callback_tests(void);

/**
 * @brief 运行焦点并发测试（测试线程安全性和并发场景）
 */
void run_focus_concurrency_tests(void);

/* ========================================
 * 辅助函数声明
 * ======================================== */

/**
 * @brief 等待指定毫秒数
 * @param ms 等待时间(毫秒)
 */
void wait_ms(uint32_t ms);

/**
 * @brief 将播放器状态转换为字符串
 * @param state 播放器状态
 * @return 状态名称字符串
 */
const char* state_to_string(app_player_state_t state);

/**
 * @brief 将播放器事件转换为字符串
 * @param event 播放器事件
 * @return 事件名称字符串
 */
const char* test_event_to_string(app_player_event_t event);

/**
 * @brief 将焦点状态转换为字符串
 * @param focus_state 焦点状态
 * @return 焦点状态字符串
 */
const char* focus_state_to_string(app_player_focus_state_t focus_state);

/**
 * @brief 将错误码转换为字符串
 * @param error 错误码
 * @return 错误名称字符串
 */
const char* error_to_string(int error);

/**
 * @brief 获取TONE播放器的URL
 * @return TONE URL字符串
 */
const char* get_tone_url(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_COMMON_H */
