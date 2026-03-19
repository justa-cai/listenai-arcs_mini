#pragma once

#include <stdint.h>

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING,
    OTA_STATE_UPDATING,
    OTA_STATE_SUCCESSED,
    OTA_STATE_FAILED,
    OTA_STATE_UP_TO_DATE,
} ota_state_e;

typedef enum {
    OTA_TARGET_UNSPECIFIED = 0,
    OTA_TARGET_APP,
    OTA_TARGET_WAKE_WORD,
    OTA_TARGET_PROMPT_TONE,
    OTA_TARGET_EMOJI,
} ota_target_e;

typedef enum {
    OTA_REBOOT_STRATEGY_AUTO = 0,
    OTA_REBOOT_STRATEGY_MANUAL,
} ota_reboot_strategy_e;

typedef struct {
    ota_state_e state;
    ota_target_e target;
    ota_reboot_strategy_e reboot;
    uint32_t bytes_processed;
    uint32_t bytes_total;
    uint32_t update_index;     // 当前更新的资源序号 (从1开始)
    uint32_t update_total;     // 需要更新的资源总数
    uint32_t elapsed_ms;       // 当前资源已下载耗时 (ms)
    char wake_word[20];
} ota_state_t;

int ota_manager_check_all(void);
