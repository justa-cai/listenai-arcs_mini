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
    VOICE_MSG_SD_MUSIC_SYNC_STATE_IDLE = 0,
    VOICE_MSG_SD_MUSIC_SYNC_STATE_START,
    VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING,
    VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED,
    VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED,
    VOICE_MSG_SD_MUSIC_SYNC_STATE_FINISHED,
} voice_msg_sd_music_sync_state_e;

typedef enum {
    VOICE_MSG_SD_MUSIC_SYNC_RESULT_OK = 0,
    VOICE_MSG_SD_MUSIC_SYNC_RESULT_DATA_UNCHANGED = 1,
    VOICE_MSG_SD_MUSIC_SYNC_RESULT_NO_MP3 = 2,
    VOICE_MSG_SD_MUSIC_SYNC_RESULT_FAILED = -1,
} voice_msg_sd_music_sync_result_e;

typedef struct {
    uint32_t state;          /* voice_msg_sd_music_sync_state_e */
    uint32_t uploaded_count; /* 已成功上报文件数 */
    uint32_t total_count;    /* 本次预计上报的音频文件总数，不含 skipped_count */
    int32_t result;          /* voice_msg_sd_music_sync_result_e */
    uint32_t http_status_code; /* HTTP 响应码，0 表示未获取或非 HTTP 失败 */
    int32_t http_error_code;   /* HTTP 客户端错误码，0 表示无 */
    uint32_t skipped_count;         /* 扫描到但因限制未上报的音频文件数 */
} voice_msg_sd_music_sync_state_t;



#ifdef __cplusplus
}
#endif
