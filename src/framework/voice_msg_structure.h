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
    char command[32];
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


#ifdef __cplusplus
}
#endif
