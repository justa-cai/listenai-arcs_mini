/*
 * LISA App Player Component - 测试公共实现
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_common.h"
#include "FreeRTOS.h"
#include "task.h"

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
