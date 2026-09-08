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

typedef enum {
    VOICE_MSG_POWER_POLICY_STATE_NORMAL = 0,
    VOICE_MSG_POWER_POLICY_STATE_IDLE,
    VOICE_MSG_POWER_POLICY_STATE_HIBERNATE,
    VOICE_MSG_POWER_POLICY_STATE_SHUTDOWN_PENDING,
} voice_msg_power_policy_state_t;

typedef enum {
    VOICE_MSG_POWER_POLICY_REASON_INIT = 0,
    VOICE_MSG_POWER_POLICY_REASON_IDLE_TIMEOUT,
    VOICE_MSG_POWER_POLICY_REASON_HIBERNATE_TIMEOUT,
    VOICE_MSG_POWER_POLICY_REASON_LOW_BATTERY,
    VOICE_MSG_POWER_POLICY_REASON_VOICE_WAKE,
    VOICE_MSG_POWER_POLICY_REASON_BUTTON,
    VOICE_MSG_POWER_POLICY_REASON_EXTERNAL_POWER,
    VOICE_MSG_POWER_POLICY_REASON_INTERACTION,
} voice_msg_power_policy_reason_t;

typedef struct {
    uint8_t state;  /* voice_msg_power_policy_state_t */
    uint8_t reason; /* voice_msg_power_policy_reason_t */
    uint8_t reserved[2];
} voice_msg_power_policy_state_event_t;

typedef enum {
    VOICE_MSG_MUSIC_SOURCE_ONLINE = 0,
    VOICE_MSG_MUSIC_SOURCE_TF_CARD,
} voice_msg_music_source_e;

#define VOICE_MSG_MUSIC_NAME_MAX 256

typedef struct {
    uint32_t source; /* voice_msg_music_source_e */
    char name[VOICE_MSG_MUSIC_NAME_MAX];
} voice_msg_music_info_t;

typedef struct {
    uint8_t auto_reboot;
    uint8_t reserved[3];
    uint32_t delay_ms;
} voice_msg_cloud_reboot_t;

/** 云端会话中断选项；空 payload 保持历史行为并视为用户主动中断。 */
typedef struct {
    uint8_t report_reply_position;
} voice_msg_cloud_session_interrupt_t;

/** Timeline 字幕条目；text 紧随结构体存储且以 NUL 结尾。 */
typedef struct {
    uint32_t index;
    uint32_t start_index;
    uint32_t end_index;
    uint32_t start_ms;
    uint32_t end_ms;
    uint16_t text_len;
    uint16_t reserved;
    char text[];
} voice_msg_tts_timeline_t;

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
    VOICE_MSG_SD_MUSIC_SYNC_RESULT_FS_UNSUPPORTED = -2,
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

typedef struct {
    uint8_t available;
    uint8_t reserved[3];
} voice_msg_sd_card_state_t;



#ifdef __cplusplus
}
#endif
