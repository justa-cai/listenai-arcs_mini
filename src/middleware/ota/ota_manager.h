#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING,
    OTA_STATE_PACKAGE_INFO,
    OTA_STATE_UPDATING,
    OTA_STATE_SUCCESSED,
    OTA_STATE_APP_FAILED,
    OTA_STATE_RESOURCE_FAILED,
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

typedef enum {
    OTA_POST_ACTION_NONE = 0,
    OTA_POST_ACTION_REBOOT,
} ota_post_action_e;

typedef struct {
    ota_state_e state;
    ota_target_e target;
    ota_reboot_strategy_e reboot;
    ota_post_action_e post_action;
    uint32_t bytes_processed;
    uint32_t bytes_total;
    uint32_t update_index;     // 当前更新的资源序号 (从1开始)
    uint32_t update_total;     // 需要更新的资源总数
    uint32_t elapsed_ms;       // 当前资源已下载耗时 (ms)
    char wake_word[20];
    char current_version[32];
    char target_version[32];
    char update_notes[512];
} ota_state_t;

typedef int (*ota_manager_resources_updated_cb_t)(bool wake_word_updated,
                                                  bool prompt_tone_updated,
                                                  bool emoji_updated,
                                                  void *user_data);

int ota_manager_check_all(void);
int ota_manager_check_after_power_connected(void);
ota_state_e ota_manager_get_state(void);
int ota_manager_get_state_snapshot(ota_state_t *state);
int ota_manager_register_resources_updated_cb(ota_manager_resources_updated_cb_t cb, void *user_data);
int ota_manager_app_update_input_ready(void);
int ota_manager_confirm_app_update(void);
int ota_manager_skip_app_update(void);
