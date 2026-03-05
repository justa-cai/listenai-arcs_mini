/*
 * LISA App Player Component - 焦点管理测试公共实现
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_tone.h"
#include "lisa_gpio.h"
#include "lisa_device.h"

/* ========================================
 * 全局播放器实例
 * ======================================== */

app_player_t *g_tts_player = NULL;
app_player_t *g_music_player = NULL;
app_player_t *g_tone_player = NULL;
app_player_t *g_alarm_player = NULL;

/* ========================================
 * 焦点管理配置
 * ======================================== */

/**
 * @brief 焦点通道配置定义
 *
 * 优先级说明：
 * - ALARM (priority=0): 最高优先级，闹钟/紧急提醒
 * - TONE  (priority=1): 高优先级，系统提示音
 * - TTS   (priority=2): 中等优先级，语音播报
 * - MUSIC (priority=3): 最低优先级，背景音乐
 *
 * 抢占规则：
 * - ALARM 可以强制抢占所有其他播放器（通过capture_names）
 * - TONE 可以强制抢占 TTS（通过capture_names），通过优先级抢占 MUSIC
 * - TTS 通过优先级抢占 MUSIC（不在capture_names中，所以MUSIC变为BACKGROUND而不是NONE）
 * - MUSIC 不抢占任何播放器
 *
 * 关键设计：
 * - TTS 的 capture_names 不包含 MUSIC，这样 TTS 通过优先级仲裁获得焦点
 * - MUSIC 会收到 BACKGROUND 焦点并暂停，而不是 NONE（停止）
 * - 当 TTS 完成后，MUSIC 可以自动恢复播放
 */
static app_player_focus_channel_config_t s_focus_configs[] = {
    {
        .name = PLAYER_NAME_ALARM,
        .priority = 0,  // 最高优先级
        .capture_names = (const char *[]){PLAYER_NAME_TONE, PLAYER_NAME_TTS, PLAYER_NAME_MUSIC},
        .capture_count = 3,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = PLAYER_NAME_TONE,
        .priority = 1,  // 高优先级
        .capture_names = (const char *[]){PLAYER_NAME_TTS},  // 只强制抢占TTS，MUSIC通过优先级抢占
        .capture_count = 1,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = PLAYER_NAME_TTS,
        .priority = 2,  // 中等优先级
        .capture_names = NULL,  // 不强制抢占任何播放器，完全通过优先级仲裁
        .capture_count = 0,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = PLAYER_NAME_MUSIC,
        .priority = 3,  // 最低优先级
        .capture_names = NULL,
        .capture_count = 0,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    }
};

const app_player_focus_channel_config_t* get_focus_configs(int *count)
{
    if (count) {
        *count = sizeof(s_focus_configs) / sizeof(s_focus_configs[0]);
    }
    return s_focus_configs;
}

/* ========================================
 * 辅助函数实现
 * ======================================== */

void wait_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

const char* state_to_string(app_player_state_t state)
{
    switch(state) {
        case APP_PLAYER_STATE_IDLE:      return "IDLE";
        case APP_PLAYER_STATE_PREPARING: return "PREPARING";
        case APP_PLAYER_STATE_PREPARED:  return "PREPARED";
        case APP_PLAYER_STATE_PLAYING:   return "PLAYING";
        case APP_PLAYER_STATE_PAUSED:    return "PAUSED";
        case APP_PLAYER_STATE_STOPPED:   return "STOPPED";
        case APP_PLAYER_STATE_ERROR:     return "ERROR";
        default:                          return "UNKNOWN";
    }
}

const char* test_event_to_string(app_player_event_t event)
{
    switch(event) {
        case APP_PLAYER_EVENT_ERROR:         return "ERROR";
        case APP_PLAYER_EVENT_PREPARED:      return "PREPARED";
        case APP_PLAYER_EVENT_PLAYING:       return "PLAYING";
        case APP_PLAYER_EVENT_PAUSED:        return "PAUSED";
        case APP_PLAYER_EVENT_STOPPED:       return "STOPPED";
        case APP_PLAYER_EVENT_COMPLETED:     return "COMPLETED";
        case APP_PLAYER_EVENT_SEEK_COMPLETE: return "SEEK_COMPLETE";
        default:                              return "UNKNOWN";
    }
}

const char* focus_state_to_string(app_player_focus_state_t focus_state)
{
    switch(focus_state) {
        case APP_PLAYER_FOCUS_FOREGROUND: return "FOREGROUND";
        case APP_PLAYER_FOCUS_BACKGROUND: return "BACKGROUND";
        case APP_PLAYER_FOCUS_NONE:       return "NONE";
        default:                           return "UNKNOWN";
    }
}

const char* error_to_string(int error)
{
    switch(error) {
        case APP_PLAYER_OK:                  return "OK";
        case APP_PLAYER_ERR_INVALID_PARAM:   return "INVALID_PARAM";
        case APP_PLAYER_ERR_NO_MEMORY:       return "NO_MEMORY";
        case APP_PLAYER_ERR_INVALID_STATE:   return "INVALID_STATE";
        case APP_PLAYER_ERR_NOT_SUPPORTED:   return "NOT_SUPPORTED";
        case APP_PLAYER_ERR_TIMEOUT:         return "TIMEOUT";
        case APP_PLAYER_ERR_IO:              return "IO";
        default:                              return "UNKNOWN";
    }
}

const char* get_tone_url(void)
{
    return app_tone_get_url(TONE_ID_0);
}

