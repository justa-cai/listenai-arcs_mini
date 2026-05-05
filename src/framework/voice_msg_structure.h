#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct{
    char id[40];
    char name[64];
}voice_msg_cloud_mcp_context_t;

typedef struct{
    voice_msg_cloud_mcp_context_t context;
    char command[64];
}voice_msg_cloud_recognized_command_t;

typedef struct{
    voice_msg_cloud_mcp_context_t context;
    char cmd[64];
    char time[32];
}voice_msg_cloud_recognition_cmd_timer_t;

typedef enum {
    VOICE_MSG_BATTERY_STATUS_NO_BATTERY = 0,
    VOICE_MSG_BATTERY_STATUS_NOT_CONNECT,
    VOICE_MSG_BATTERY_STATUS_CHARGING,
    VOICE_MSG_BATTERY_STATUS_CHARGE_DONE,
    VOICE_MSG_BATTERY_STATUS_UNKNOWN,
} voice_msg_battery_status_t;

typedef struct {
    uint8_t level;  /* 0-100 */
    uint8_t status; /* voice_msg_battery_status_t */
} voice_msg_battery_info_t;

typedef struct {
    uint8_t auto_reboot;
    uint8_t reserved[3];
    uint32_t delay_ms;
} voice_msg_cloud_reboot_t;

typedef enum {
    VOICE_MSG_CAMERA_PREVIEW_MODE_NONE = 0,
    VOICE_MSG_CAMERA_PREVIEW_MODE_BUTTON_PHOTO,
    VOICE_MSG_CAMERA_PREVIEW_MODE_MCP_PHOTO,
} voice_msg_camera_preview_mode_t;

typedef enum {
    VOICE_MSG_CAMERA_FLOW_PHASE_NONE = 0,
    VOICE_MSG_CAMERA_FLOW_PHASE_PREVIEW,
    VOICE_MSG_CAMERA_FLOW_PHASE_PROCESSING,
    VOICE_MSG_CAMERA_FLOW_PHASE_RESULT_TTS,
} voice_msg_camera_flow_phase_t;

typedef struct {
    uint8_t mode;
    uint8_t reserved[3];
    uint32_t auto_capture_delay_ms;
    char context_id[64];
} voice_msg_camera_preview_req_t;

typedef struct {
    uint8_t active;
    uint8_t mode;
    uint8_t captured;
    uint8_t phase;
} voice_msg_camera_preview_state_t;



#ifdef __cplusplus
}
#endif
