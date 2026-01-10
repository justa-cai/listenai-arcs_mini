#pragma once

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
    OTA_TARGET_GREETING,
} ota_target_e;

typedef struct {
    ota_state_e state;
    ota_target_e target;
    uint32_t bytes_processed;
    uint32_t bytes_total;
} ota_state_t;

int ota_manager_init(void);

int ota_manager_check_all(void);
