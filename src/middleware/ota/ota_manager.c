#define TAG "ota_manager"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "power_manager.h"
#include "app_wakeup.h"

#include "ota_manager.h"
#include "ota_api.h"
#include "ota_flash.h"
#include "voice_msg.h"
#include "kv_sys.h"
#include "kv_user.h"

static int ota_manager_wake_word_check(void);
static int ota_manager_prompt_tone_check(void);

static ota_state_t s_ota_state = {
    .state = OTA_STATE_IDLE,
    .target = OTA_TARGET_UNSPECIFIED,
};

static ota_dev_conf_t dev_conf;

static void ota_manager_publish_state_event(void)
{
    uint32_t evt;

    switch (s_ota_state.state) {
    case OTA_STATE_CHECKING:
        evt = VOICE_MSG_OTA_CHECKING;
        break;
    case OTA_STATE_UPDATING:
        evt = VOICE_MSG_OTA_UPDATING;
        break;
    case OTA_STATE_SUCCESSED:
        evt = VOICE_MSG_OTA_SUCCESSED;
        break;
    case OTA_STATE_FAILED:
        evt = VOICE_MSG_OTA_FAILED;
        break;
    case OTA_STATE_UP_TO_DATE:
        evt = VOICE_MSG_OTA_UP_TO_DATE;
        break;
    default:
        return;
    }

    voice_msg_pub(evt, &s_ota_state, sizeof(ota_state_t));
}

static void ota_manager_notify_state(ota_state_e state)
{
    s_ota_state.state = state;
    ota_manager_publish_state_event();
}

static TickType_t last_progress_update = 0;
static void ota_manager_notify_progress(uint32_t bytes_processed, uint32_t bytes_total)
{
    s_ota_state.bytes_processed = bytes_processed;
    s_ota_state.bytes_total = bytes_total;

    TickType_t current = xTaskGetTickCount();
    if (current - last_progress_update < pdMS_TO_TICKS(10)) {
        return;
    }

    last_progress_update = current;
    ota_manager_publish_state_event();
}

static void ota_manager_update_reboot_strategy(void)
{
    s_ota_state.reboot = power_is_usb_plugged() ? OTA_REBOOT_STRATEGY_AUTO : OTA_REBOOT_STRATEGY_MANUAL;
}

static int _ota_manager_check_all(void)
{
    int ret;
    int skip_wake_word_update = 0;
    int skip_prompt_tone_update = 0;
    bool need_reboot = false;

    uint32_t start_time = xTaskGetTickCount();

    // TODO: check app update first

    memset(&dev_conf, 0, sizeof(ota_dev_conf_t));

    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_WAKEWORD_UPDATE, &skip_wake_word_update);
    if (ret != 0) {
        skip_wake_word_update = 0;
    }

    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_TONE_UPDATE, &skip_prompt_tone_update);
    if (ret != 0) {
        skip_prompt_tone_update = 0;
    }

    if (skip_wake_word_update && skip_prompt_tone_update) {
        LISA_LOGI(TAG, "Skip wake word and prompt tone update as per user settings");
    } else {
        s_ota_state.target = OTA_TARGET_UNSPECIFIED;
        ota_manager_notify_state(OTA_STATE_CHECKING);

        ret = ota_api_get_dev_conf(&dev_conf);
        if (ret < 0) {
            LISA_LOGW(TAG, "Get resource info failed (%d), assumed up-to-date", ret);
            goto up_to_date;
        }

        if (skip_wake_word_update) {
            LISA_LOGI(TAG, "Skip wake word update as per user settings");
        } else {
            s_ota_state.target = OTA_TARGET_WAKE_WORD;
            ota_manager_notify_state(OTA_STATE_CHECKING);

            ret = ota_manager_wake_word_check();
            if (ret < 0) {
                LISA_LOGE(TAG, "Wake word check failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_FAILED);
                goto reboot;
            } else if (ret == 0) {
                LISA_LOGI(TAG, "Wake word up-to-date");
            } else {
                LISA_LOGI(TAG, "Wake word updated (%d)", ret);
                need_reboot = true;
            }

            char *current_wake_word = NULL;
            lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &current_wake_word);
            if (current_wake_word == NULL || strcmp(current_wake_word, dev_conf.wakeup_word.text) != 0) {
                lisa_kv_set_string(KV_KEY_SYS_WAKEWORD, dev_conf.wakeup_word.text);
                LISA_LOGI(TAG, "Wake word set to: %s", dev_conf.wakeup_word.text);
            }
            lisa_kv_free(current_wake_word);
        }

        if (skip_prompt_tone_update) {
            LISA_LOGI(TAG, "Skip prompt tone update as per user settings");
        } else {
            s_ota_state.target = OTA_TARGET_PROMPT_TONE;
            ota_manager_notify_state(OTA_STATE_CHECKING);

            ret = ota_manager_prompt_tone_check();
            if (ret < 0) {
                LISA_LOGE(TAG, "Prompt tone check failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_FAILED);
                goto reboot;
            } else if (ret == 0) {
                LISA_LOGI(TAG, "Prompt tone up-to-date");
            } else {
                LISA_LOGI(TAG, "Prompt tone updated (%d)", ret);
                need_reboot = true;
            }
        }
    }

    if (need_reboot) {
        LISA_LOGI(TAG, "Rebooting to apply updates...");
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_SUCCESSED);
        goto reboot;
    }

up_to_date:
    LISA_LOGI(TAG, "All resources up-to-date");

    // Always apply persisted wake word, even when wake word OTA update is skipped.
    char *wake_word = NULL;
    s_ota_state.wake_word[0] = '\0';
    lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &wake_word);
    if (wake_word != NULL && wake_word[0] != '\0') {
        strncpy(s_ota_state.wake_word, wake_word, sizeof(s_ota_state.wake_word) - 1);
        s_ota_state.wake_word[sizeof(s_ota_state.wake_word) - 1] = '\0';
    }
    lisa_kv_free(wake_word);

    uint32_t elapsed = xTaskGetTickCount() - start_time;
    LISA_LOGI(TAG, "OTA check completed in %u ms", pdTICKS_TO_MS(elapsed));

    ota_manager_notify_state(OTA_STATE_UP_TO_DATE);

    return 0;

reboot:
    if (s_ota_state.reboot == OTA_REBOOT_STRATEGY_AUTO) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        extern void sys_platform_sw_full_reset(void);
        sys_platform_sw_full_reset();
    } else if (s_ota_state.reboot == OTA_REBOOT_STRATEGY_MANUAL) {
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    return ret;
}

static void ota_manager_check_task(void *arg)
{
    _ota_manager_check_all();
    lisa_thread_delete(NULL);
}

int ota_manager_check_all(void)
{
    lisa_thread_attr_t attr = {
        .name = "ota_check",
        .stack_size = 16 * 1024,
        .priority = LISA_OS_PRIORITY_HIGH,
    };
    lisa_thread_create(&attr, ota_manager_check_task, NULL);

    return 0;
}

static int ota_manager_wake_word_download_cb(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data,
                                             uint32_t size)
{
    int ret;

    ret = ota_flash_update_step(OTA_PART_WAKE_WORD_BIN, offset, data, size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write wake_word.bin failed at offset %u, size %u (%d)", offset, size, ret);
    }

    ota_manager_notify_progress(offset + size, res_info->size);

    return ret;
}

/**
 * 检查唤醒词更新
 *
 * @return < 0: 检查或更新出错
 *         = 0: 无需更新
 *         > 0: 已更新资源数量
 */
static int ota_manager_wake_word_check(void)
{
    int ret;
    bool wake_word_bin_need_update = false;
    int updated = 0;

    if (dev_conf.wakeup_word.resource.size > 0) {
        ret = ota_flash_verify(OTA_PART_WAKE_WORD_BIN, dev_conf.wakeup_word.resource.md5,
                               dev_conf.wakeup_word.resource.size);
        if (ret < 0) {
            return 0;
        }

        wake_word_bin_need_update = ret > 0;
        LISA_LOGI(TAG, "wake_word.bin need update: %d", wake_word_bin_need_update);
    } else {
        LISA_LOGI(TAG, "wake_word.bin not found, skip checking");
    }

    if (!wake_word_bin_need_update) {
        return 0;
    }

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = dev_conf.wakeup_word.resource.size;
    ota_manager_notify_state(OTA_STATE_UPDATING);

    // 停止算法
    app_wakeup_stop();

    LISA_LOGI(TAG, "Begin update wake_word.bin...");

    ret = ota_flash_update_begin(OTA_PART_WAKE_WORD_BIN, dev_conf.wakeup_word.resource.size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Begin update of wake_word.bin partition failed (%d)", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Downloading wake_word.bin from %s, size %u", dev_conf.wakeup_word.resource.url,
              dev_conf.wakeup_word.resource.size);

    ret = ota_api_download(&dev_conf.wakeup_word.resource, ota_manager_wake_word_download_cb);
    if (ret < 0) {
        LISA_LOGE(TAG, "Download wake_word.bin failed (%d)", ret);
        return ret;
    }

    ret = ota_flash_update_finish(OTA_PART_WAKE_WORD_BIN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of wake_word.bin partition failed (%d)", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Update wake_word.bin finished");
    updated++;

    return updated;
}

static int ota_manager_prompt_tone_download_cb(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data,
                                               uint32_t size)
{
    int ret;

    ret = ota_flash_update_step(OTA_PART_PROMPT_TONE_BIN, offset, data, size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write prompt_tone.bin failed at offset %u, size %u (%d)", offset, size, ret);
    }

    ota_manager_notify_progress(offset + size, res_info->size);

    return ret;
}

static int ota_manager_prompt_tone_check(void)
{
    int ret;
    bool prompt_tone_need_update = false;
    int updated = 0;

    if (dev_conf.prompt_tone.size > 0) {
        ret = ota_flash_verify(OTA_PART_PROMPT_TONE_BIN, dev_conf.prompt_tone.md5, dev_conf.prompt_tone.size);
        if (ret < 0) {
            return 0;
        }

        prompt_tone_need_update = ret > 0;
        LISA_LOGI(TAG, "prompt_tone.bin need update: %d", prompt_tone_need_update);
    } else {
        LISA_LOGI(TAG, "prompt_tone.bin not found, skip checking");
    }

    if (!prompt_tone_need_update) {
        return 0;
    }

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = dev_conf.prompt_tone.size;
    ota_manager_notify_state(OTA_STATE_UPDATING);

    LISA_LOGI(TAG, "Begin update prompt_tone.bin...");

    ret = ota_flash_update_begin(OTA_PART_PROMPT_TONE_BIN, dev_conf.prompt_tone.size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Begin update of prompt_tone.bin partition failed (%d)", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Downloading prompt_tone.bin from %s, size %u", dev_conf.prompt_tone.url, dev_conf.prompt_tone.size);

    ret = ota_api_download(&dev_conf.prompt_tone, ota_manager_prompt_tone_download_cb);
    if (ret < 0) {
        LISA_LOGE(TAG, "Download prompt_tone.bin failed (%d)", ret);
        return ret;
    }

    ret = ota_flash_update_finish(OTA_PART_PROMPT_TONE_BIN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of prompt_tone.bin partition failed (%d)", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Update prompt_tone.bin finished");
    updated++;

    return updated;
}
