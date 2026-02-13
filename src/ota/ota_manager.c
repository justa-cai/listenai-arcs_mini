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
#include "battery/battery.h"

#include "ota_manager.h"
#include "ota_api.h"
#include "ota_flash.h"
#include "assistant_controller.h"
#include "comm_service.h"
#include "app_tone.h"
#include "tone.h"
#include "kv_sys.h"
#include "kv_user.h"

static int ota_manager_wake_word_check(void);
static int ota_manager_greeting_check(void);
static int ota_manager_prompt_tone_check(void);

static ota_state_t s_ota_state = {
    .state = OTA_STATE_IDLE,
    .target = OTA_TARGET_UNSPECIFIED,
};

static ota_dev_conf_t dev_conf;

int ota_manager_init(void)
{
    return ota_flash_init();
}

static void ota_manager_notify_state(ota_state_e state)
{
    s_ota_state.state = state;
    assist_controller_trigger_event(CONTROLLER_EVENT_OTA_STATE_UPDATE, &s_ota_state, sizeof(ota_state_t));
}

static TickType_t last_progress_update = 0;
static void ota_manager_notify_progress(uint32_t bytes_processed, uint32_t bytes_total)
{
    s_ota_state.bytes_processed = bytes_processed;
    s_ota_state.bytes_total = bytes_total;

    TickType_t current = xTaskGetTickCount();
    if (current - last_progress_update < pdMS_TO_TICKS(100)) {
        return;
    }

    last_progress_update = current;
    assist_controller_trigger_event(CONTROLLER_EVENT_OTA_STATE_UPDATE, &s_ota_state, sizeof(ota_state_t));
}

static void ota_manager_update_reboot_strategy(void)
{
    s_ota_state.reboot = get_usb_status() == USB_STATUS_PLUG ? OTA_REBOOT_STRATEGY_AUTO : OTA_REBOOT_STRATEGY_MANUAL;
}

static int _ota_manager_check_all(void)
{
    int ret;
    int skip_wake_word_update = 0;
    int skip_greeting_update = 0;
    bool need_reboot = false;

    // TODO: check app update first

    memset(&dev_conf, 0, sizeof(ota_dev_conf_t));

    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_WAKEWORD_UPDATE, &skip_wake_word_update);
    if (ret != 0) {
        skip_wake_word_update = 0;
    }

    ret = lisa_kv_get_int(KV_KEY_USER_USE_LOCAL_ACK_TONE, &skip_greeting_update);
    if (ret != 0) {
        skip_greeting_update = 0;
    }

    if (skip_wake_word_update && skip_greeting_update) {
        LISA_LOGI(TAG, "Skip wake word and greeting update as per user settings");
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

        if (skip_greeting_update) {
            LISA_LOGI(TAG, "Skip greeting update as per user settings");
        } else {
            s_ota_state.target = OTA_TARGET_GREETING;
            ota_manager_notify_state(OTA_STATE_CHECKING);

            ret = ota_manager_greeting_check();
            if (ret < 0) {
                LISA_LOGE(TAG, "Greeting check failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_FAILED);
                goto reboot;
            } else if (ret == 0) {
                LISA_LOGI(TAG, "Greeting up-to-date");
            } else {
                LISA_LOGI(TAG, "Greeting updated (%d)", ret);
                // no reboot needed
            }
        }

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
            // no reboot needed
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

    // Ensure algo is running
    lis_ivw_run();

    // Apply wake word
    if (!skip_wake_word_update) {
        char *wake_word = NULL;
        lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &wake_word);
        if (wake_word != NULL && strlen(wake_word) > 0) {
            assist_controller_trigger_event(CONTROLLER_EVENT_WAKE_WORD_UPDATE, wake_word, strlen(wake_word) + 1);
        }
        lisa_kv_free(wake_word);
    }

    // Apply greeting mp3 and prompt tone from OTA partition
    if (!skip_greeting_update) {
        // greeting mp3
        const void *data = NULL;
        uint32_t limit = 0;
        uint32_t size = 0;

        if (ota_flash_get(OTA_PART_GREETING_MP3, &data, &limit) < 0) {
            LISA_LOGW(TAG, "Get greeting.mp3 from flash failed");
            limit = 0;
        }

        if (lisa_kv_get_int(KV_KEY_SYS_GREETING_TONE_SIZE, (int *)&size) != 0) {
            size = dev_conf.greeting.size;
        }

        if (data && size > 0 && size <= limit) {
            LISA_LOGI(TAG, "Apply greeting tone from flash, size %u", size);
            app_tone_override(TONE_ID_0, data, size);
        }
    
        // prompt tone 
        const void *tone_data = NULL;
        uint32_t tone_limit = 0;
        uint32_t tone_size = 0;

        if (ota_flash_get(OTA_PART_TONE_BIN, &tone_data, &tone_limit) < 0) {
            LISA_LOGW(TAG, "Get prompt tone from flash failed");
        } else if (lisa_kv_get_int(KV_KEY_SYS_PROMPT_TONE_SIZE, (int *)&tone_size) != 0) {
            tone_size = dev_conf.prompt_tone.size;
        }

        if (tone_data && tone_size > 0 && tone_size <= tone_limit) {
            LISA_LOGI(TAG, "Apply prompt tone from flash, size %u", tone_size);
            struct romfs *tone_fs = NULL;
            if (romfs_init(&tone_fs, tone_data, tone_size) == 0) {
                int loaded = app_tone_load_from_romfs(tone_fs, "/");
                LISA_LOGI(TAG, "Loaded %d prompt tones from OTA partition", loaded);
            } else {
                LISA_LOGW(TAG, "Failed to init romfs for prompt tone");
            }
        }

    }

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

static lisa_semaphore_t *ota_check_sem = NULL;

static void ota_manager_check_task(void *arg)
{
    _ota_manager_check_all();
    lisa_semaphore_give(ota_check_sem);
    lisa_thread_delete(NULL);
}

int ota_manager_check_all(void)
{
    if (ota_check_sem != NULL) {
        LISA_LOGW(TAG, "OTA already in progress, state: %d", s_ota_state.state);
        return -1;
    }

    ota_check_sem = lisa_semaphore_create(1);
    if (ota_check_sem == NULL) {
        LISA_LOGE(TAG, "Create OTA check semaphore failed");
        return -1;
    }

    lisa_semaphore_take(ota_check_sem, 0);

    lisa_thread_attr_t attr = {
        .name = "ota_check",
        .stack_size = 16 * 1024,
        .priority = LISA_OS_PRIORITY_HIGH,
    };
    lisa_thread_create(&attr, ota_manager_check_task, NULL);

    lisa_semaphore_take(ota_check_sem, LISA_OS_WAIT_FOREVER);

    lisa_semaphore_delete(ota_check_sem);
    ota_check_sem = NULL;

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
    lis_ivw_idle();

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

static int ota_manager_greeting_download_cb(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data,
                                            uint32_t size)
{
    int ret;

    ret = ota_flash_update_step(OTA_PART_GREETING_MP3, offset, data, size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write greeting.mp3 failed at offset %u, size %u (%d)", offset, size, ret);
    }

    ota_manager_notify_progress(offset + size, res_info->size);

    return ret;
}

static int ota_manager_greeting_check(void)
{
    int ret;
    bool greeting_mp3_need_update = false;
    int updated = 0;

    if (dev_conf.greeting.size > 0) {
        ret = ota_flash_verify(OTA_PART_GREETING_MP3, dev_conf.greeting.md5, dev_conf.greeting.size);
        if (ret < 0) {
            return 0;
        }

        greeting_mp3_need_update = ret > 0;
        LISA_LOGI(TAG, "greeting.mp3 need update: %d", greeting_mp3_need_update);
    } else {
        LISA_LOGI(TAG, "greeting.mp3 not found, skip checking");
    }

    if (!greeting_mp3_need_update) {
        return 0;
    }

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = dev_conf.greeting.size;
    ota_manager_notify_state(OTA_STATE_UPDATING);

    LISA_LOGI(TAG, "Begin update greeting.mp3...");

    ret = ota_flash_update_begin(OTA_PART_GREETING_MP3, dev_conf.greeting.size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Begin update of greeting.mp3 partition failed (%d)", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Downloading greeting.mp3 from %s, size %u", dev_conf.greeting.url, dev_conf.greeting.size);

    ret = ota_api_download(&dev_conf.greeting, ota_manager_greeting_download_cb);
    if (ret < 0) {
        LISA_LOGE(TAG, "Download greeting.mp3 failed (%d)", ret);
        return ret;
    }

    ret = ota_flash_update_finish(OTA_PART_GREETING_MP3);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of greeting.mp3 partition failed (%d)", ret);
        return ret;
    }

    lisa_kv_set_int(KV_KEY_SYS_GREETING_TONE_SIZE, dev_conf.greeting.size);

    LISA_LOGI(TAG, "Update greeting.mp3 finished");
    updated++;

    return updated;
}

static int ota_manager_prompt_tone_download_cb(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data,
                                               uint32_t size)
{
    int ret;

    ret = ota_flash_update_step(OTA_PART_TONE_BIN, offset, data, size);
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
        ret = ota_flash_verify(OTA_PART_TONE_BIN, dev_conf.prompt_tone.md5, dev_conf.prompt_tone.size);
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

    ret = ota_flash_update_begin(OTA_PART_TONE_BIN, dev_conf.prompt_tone.size);
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

    ret = ota_flash_update_finish(OTA_PART_TONE_BIN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of prompt_tone.bin partition failed (%d)", ret);
        return ret;
    }

    lisa_kv_set_int(KV_KEY_SYS_PROMPT_TONE_SIZE, dev_conf.prompt_tone.size);

    LISA_LOGI(TAG, "Update prompt_tone.bin finished");
    updated++;

    return updated;
}
