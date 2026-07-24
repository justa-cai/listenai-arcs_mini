#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ==================== 相机拍照流枚举与结构体 ==================== */

/** 拍照触发来源 */
typedef enum {
    CAMERA_PREVIEW_MODE_NONE = 0,
    CAMERA_PREVIEW_MODE_BUTTON,
    CAMERA_PREVIEW_MODE_MCP,
} camera_preview_mode_t;

/** 拍照流阶段 */
typedef enum {
    CAMERA_FLOW_PHASE_NONE = 0,
    CAMERA_FLOW_PHASE_PREVIEW,
    CAMERA_FLOW_PHASE_PROCESSING,
    CAMERA_FLOW_PHASE_RESULT_TTS,
} camera_flow_phase_t;

typedef struct {
    camera_preview_mode_t mode;
    uint8_t sync;
    uint8_t no_pushup_tts;   /* MCP 拍照时 JSON args 带 "sync":true → 云端不会下发 pushup TTS URL */
    uint8_t reserved[1];
    uint32_t auto_capture_delay_ms;
    char context_id[64];
} voice_msg_camera_preview_req_t;

typedef struct {
    uint8_t active;
    camera_preview_mode_t mode;
    uint8_t captured;
    camera_flow_phase_t phase;
} voice_msg_camera_preview_state_t;

/* ==================== PHOTO_FLOW intent API ==================== */

int voice_intent_photo_flow_register(void);
