#define TAG "ota_manager"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "power_manager.h"
#include "app_wakeup.h"
#include "project_version.h"
#include "uboot_features_api.h"
#include "uboot_ota_api.h"
#include "arcs_ap_base.h"  /* CMN_FLASH_REGION */
#include "battery.h"

#include "ota_manager.h"
#include "ota_api.h"
#include "ota_flash.h"
#include "voice_msg.h"
#include "app_tone.h"
#include "tone.h"
#include "voice_player_comm.h"
#include "kv_sys.h"
#include "kv_user.h"

#define OTA_FAILURE_UI_DISPLAY_MS 3000U
#define OTA_APP_CONFIRM_INPUT_GUARD_MS 1200U
#define OTA_TONE_POLL_MS 20U
#define OTA_TONE_START_TIMEOUT_MS 3000U
#define OTA_TONE_FINISH_TIMEOUT_MS 15000U
#define OTA_DOWNLOAD_RETRY_COUNT 3
#define OTA_DOWNLOAD_RETRY_DELAY_BASE_MS 300U

static int ota_manager_app_update(const ota_app_package_t *pkg);
static int ota_manager_wake_word_update(void);
static int ota_manager_prompt_tone_update(void);
static int ota_manager_emoji_update(void);
static void ota_manager_notify_state(ota_state_e state);

typedef enum {
    OTA_APP_DECISION_NONE = 0,
    OTA_APP_DECISION_UPDATE,
    OTA_APP_DECISION_SKIP,
} ota_app_decision_e;

static ota_state_t s_ota_state = {
    .state = OTA_STATE_IDLE,
    .target = OTA_TARGET_UNSPECIFIED,
};

static ota_dev_conf_t dev_conf;
static SemaphoreHandle_t s_app_decision_sem = NULL;
static ota_app_decision_e s_app_decision = OTA_APP_DECISION_NONE;
static TickType_t s_app_prompt_ready_tick = 0;
static bool s_app_update_applied_this_boot = false;
/* APP OTA 只在开机后首次联网检查一次；资源 OTA 不受这个标志影响。 */
static bool s_app_ota_checked_this_boot = false;
static bool s_low_battery_skip_wait_power = false;
static ota_manager_resources_updated_cb_t s_resources_updated_cb = NULL;
static void *s_resources_updated_cb_user_data = NULL;
static TickType_t s_ota_state_changed_tick = 0;

static int ota_manager_ensure_app_decision_sem(void)
{
    if (s_app_decision_sem != NULL) {
        return 0;
    }

    s_app_decision_sem = xSemaphoreCreateBinary();
    if (s_app_decision_sem == NULL) {
        LISA_LOGE(TAG, "Failed to create app OTA decision semaphore");
        return -1;
    }

    return 0;
}

static void ota_manager_wait_tone_finished(void)
{
    TickType_t wait_start = xTaskGetTickCount();
    bool tone_started = false;

    if (tone_player == NULL) {
        return;
    }

    while (1) {
        switch (app_player_get_state(tone_player)) {
        case APP_PLAYER_STATE_PREPARING:
        case APP_PLAYER_STATE_PREPARED:
        case APP_PLAYER_STATE_PLAYING:
        case APP_PLAYER_STATE_PAUSED:
            tone_started = true;
            break;
        case APP_PLAYER_STATE_STOPPED:
        case APP_PLAYER_STATE_IDLE:
        case APP_PLAYER_STATE_ERROR:
        default:
            if (tone_started) {
                return;
            }
            if ((xTaskGetTickCount() - wait_start) >= pdMS_TO_TICKS(OTA_TONE_START_TIMEOUT_MS)) {
                LISA_LOGW(TAG, "OTA tone did not enter playback state within timeout");
                return;
            }
            break;
        }

        if (tone_started &&
            (xTaskGetTickCount() - wait_start) >= pdMS_TO_TICKS(OTA_TONE_FINISH_TIMEOUT_MS)) {
            LISA_LOGW(TAG, "OTA tone playback wait timed out");
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(OTA_TONE_POLL_MS));
    }
}

static void ota_manager_play_tone_sync(uint16_t tone_id, uint8_t repeat_count)
{
    char *tone_url;

    if (repeat_count == 0 || tone_player == NULL) {
        return;
    }

    tone_url = app_tone_get_url(tone_id);
    if (tone_url == NULL || tone_url[0] == '\0') {
        LISA_LOGW(TAG, "OTA tone %u not found", tone_id);
        return;
    }

    for (uint8_t i = 0; i < repeat_count; ++i) {
        if (app_player_play(tone_player, tone_url) != APP_PLAYER_OK) {
            LISA_LOGW(TAG, "Failed to play OTA tone %u", tone_id);
            return;
        }
        ota_manager_wait_tone_finished();
    }
}

static void ota_manager_play_app_success_tone_if_needed(void)
{
    if (!s_app_update_applied_this_boot) {
        return;
    }

    ota_manager_play_tone_sync(TONE_ID_88, 1);
    s_app_update_applied_this_boot = false;
}

static bool ota_manager_take_app_decision(TickType_t wait_ticks, ota_app_decision_e *decision)
{
    if (decision == NULL || s_app_decision_sem == NULL) {
        return false;
    }

    if (xSemaphoreTake(s_app_decision_sem, wait_ticks) != pdTRUE) {
        return false;
    }

    if (s_app_decision == OTA_APP_DECISION_NONE) {
        return false;
    }

    *decision = s_app_decision;
    return true;
}

static ota_app_decision_e ota_manager_play_app_prompt_tone(void)
{
    char *tone_url;

    if (tone_player == NULL) {
        return OTA_APP_DECISION_NONE;
    }

    tone_url = app_tone_get_url(TONE_ID_86);
    if (tone_url == NULL || tone_url[0] == '\0') {
        LISA_LOGW(TAG, "OTA tone %u not found", TONE_ID_86);
        return OTA_APP_DECISION_NONE;
    }

    for (uint8_t i = 0; i < 3; ++i) {
        TickType_t wait_start = xTaskGetTickCount();
        bool tone_started = false;
        ota_app_decision_e decision;

        if (ota_manager_take_app_decision(0, &decision)) {
            return decision;
        }

        if (app_player_play(tone_player, tone_url) != APP_PLAYER_OK) {
            LISA_LOGW(TAG, "Failed to play OTA tone %u", TONE_ID_86);
            return OTA_APP_DECISION_NONE;
        }

        while (1) {
            switch (app_player_get_state(tone_player)) {
            case APP_PLAYER_STATE_PREPARING:
            case APP_PLAYER_STATE_PREPARED:
            case APP_PLAYER_STATE_PLAYING:
            case APP_PLAYER_STATE_PAUSED:
                tone_started = true;
                break;
            case APP_PLAYER_STATE_STOPPED:
            case APP_PLAYER_STATE_IDLE:
            case APP_PLAYER_STATE_ERROR:
            default:
                if (tone_started ||
                    (xTaskGetTickCount() - wait_start) >= pdMS_TO_TICKS(OTA_TONE_START_TIMEOUT_MS)) {
                    goto next_repeat;
                }
                break;
            }

            if (ota_manager_take_app_decision(pdMS_TO_TICKS(OTA_TONE_POLL_MS), &decision)) {
                app_player_stop(tone_player);
                return decision;
            }
        }

next_repeat:
        ;
    }

    return OTA_APP_DECISION_NONE;
}

static void ota_manager_format_size(char *buf, size_t buf_size, uint32_t bytes)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }

    if (bytes >= 1024U * 1024U) {
        snprintf(buf, buf_size, "%.1fMB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024U) {
        snprintf(buf, buf_size, "%uKB", bytes / 1024U);
    } else {
        snprintf(buf, buf_size, "%uB", bytes);
    }
}

static void ota_manager_fill_app_prompt(const ota_app_package_t *pkg)
{
    char size_text[16] = {0};

    s_ota_state.target = OTA_TARGET_APP;
    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = 0;
    s_ota_state.elapsed_ms = 0;
    s_ota_state.post_action = OTA_POST_ACTION_NONE;
    s_ota_state.current_version[0] = '\0';
    s_ota_state.target_version[0] = '\0';
    s_ota_state.update_notes[0] = '\0';

    strncpy(s_ota_state.current_version, PROJECT_VERSION_STR, sizeof(s_ota_state.current_version) - 1);
    s_ota_state.current_version[sizeof(s_ota_state.current_version) - 1] = '\0';

    if (pkg->version[0] != '\0') {
        strncpy(s_ota_state.target_version, pkg->version, sizeof(s_ota_state.target_version) - 1);
        s_ota_state.target_version[sizeof(s_ota_state.target_version) - 1] = '\0';
    }

    if (pkg->size > 0) {
        ota_manager_format_size(size_text, sizeof(size_text), pkg->size);
    }

    bool has_size = size_text[0] != '\0';
    bool has_release_notes = pkg->release_notes[0] != '\0';

    if (has_size && has_release_notes) {
        snprintf(s_ota_state.update_notes, sizeof(s_ota_state.update_notes),
                 "更新包大小：%s\n\n%s",
                 size_text, pkg->release_notes);
    } else if (has_release_notes) {
        snprintf(s_ota_state.update_notes, sizeof(s_ota_state.update_notes),
                 "\n\n%s",
                 pkg->release_notes);
    } else if (has_size) {
        snprintf(s_ota_state.update_notes, sizeof(s_ota_state.update_notes),
                 "更新包大小：%s\n\n检测到新的系统版本，建议完成更新。",
                 size_text);
    } else {
        snprintf(s_ota_state.update_notes, sizeof(s_ota_state.update_notes),
                 "\n\n检测到新的系统版本，建议完成更新。");
    }

    s_app_prompt_ready_tick = xTaskGetTickCount() + pdMS_TO_TICKS(OTA_APP_CONFIRM_INPUT_GUARD_MS);
}

static ota_app_decision_e ota_manager_wait_app_decision(const ota_app_package_t *pkg)
{
    ota_app_decision_e decision;

    if (ota_manager_ensure_app_decision_sem() != 0) {
        LISA_LOGE(TAG, "App OTA decision synchronization unavailable, skip current boot update");
        return OTA_APP_DECISION_SKIP;
    }

    s_app_decision = OTA_APP_DECISION_NONE;
    while (xSemaphoreTake(s_app_decision_sem, 0) == pdTRUE) {
    }

    ota_manager_fill_app_prompt(pkg);
    ota_manager_notify_state(OTA_STATE_PACKAGE_INFO);
    decision = ota_manager_play_app_prompt_tone();
    if (decision != OTA_APP_DECISION_NONE) {
        return decision;
    }

    while (1) {
        if (!ota_manager_take_app_decision(portMAX_DELAY, &decision)) {
            continue;
        }
        return decision;
    }
}

static void ota_manager_publish_state_event(void)
{
    uint32_t evt;

    switch (s_ota_state.state) {
    case OTA_STATE_CHECKING:
        evt = VOICE_MSG_OTA_CHECKING;
        break;
    case OTA_STATE_PACKAGE_INFO:
        evt = VOICE_MSG_OTA_PACKAGE_INFO;
        break;
    case OTA_STATE_UPDATING:
        evt = VOICE_MSG_OTA_UPDATING;
        break;
    case OTA_STATE_SUCCESSED:
        evt = VOICE_MSG_OTA_SUCCESSED;
        break;
    case OTA_STATE_APP_FAILED:
    case OTA_STATE_RESOURCE_FAILED:
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
    s_ota_state_changed_tick = xTaskGetTickCount();
    ota_manager_publish_state_event();
}

static TickType_t last_progress_update = 0;
static TickType_t download_start_tick = 0;
static void ota_manager_notify_progress(uint32_t bytes_processed, uint32_t bytes_total)
{
    s_ota_state.bytes_processed = bytes_processed;
    s_ota_state.bytes_total = bytes_total;
    s_ota_state.elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - download_start_tick);

    TickType_t current = xTaskGetTickCount();
    if (current - last_progress_update < pdMS_TO_TICKS(100)) {
        return;
    }

    last_progress_update = current;
    ota_manager_publish_state_event();
}

static void ota_manager_update_reboot_strategy(void)
{
    /* 老 boot 下 sys_platform_sw_full_reset 会让 VCC 掉电，纯电池供电
     * 里 AUTO 变成静悄悄关机，所以原来降级成 MANUAL 让用户手动重启。
     * 新 boot 支持正确的软重启回 app，无需区分供电方式，直接 AUTO。*/
    if (uboot_features_has(UBOOT_FEATURE_POWER_GUARD)) {
        s_ota_state.reboot = OTA_REBOOT_STRATEGY_AUTO;
    } else {
        s_ota_state.reboot = power_is_usb_plugged() ? OTA_REBOOT_STRATEGY_AUTO : OTA_REBOOT_STRATEGY_MANUAL;
    }
}

static void ota_manager_do_reboot(void)
{
    if (s_ota_state.reboot == OTA_REBOOT_STRATEGY_AUTO) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        power_reboot_soft();
    } else if (s_ota_state.reboot == OTA_REBOOT_STRATEGY_MANUAL) {
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

static void ota_manager_show_failure_notice(void)
{
    TickType_t min_display_ticks = pdMS_TO_TICKS(OTA_FAILURE_UI_DISPLAY_MS);
    TickType_t elapsed_ticks;

    /* 失败态 UI 要有最小展示时间；092 播完后再补足剩余时间，避免重启截断提示音。 */
    ota_manager_play_tone_sync(TONE_ID_92, 1);

    elapsed_ticks = xTaskGetTickCount() - s_ota_state_changed_tick;
    if (elapsed_ticks < min_display_ticks) {
        vTaskDelay(min_display_ticks - elapsed_ticks);
    }
}

/*
 * 根据 boot 报告的上一次 OTA 结果，对账 APP 侧记下的 package_id，决定是否把
 * 那个 package 拉进黑名单，避免"下载 → boot 失败 → 重启 → 再下载相同坏包"
 * 的死循环。在每轮 OTA 检查最前面调用一次即可。
 *
 * 用 package_id（服务端数据库 id）而非 version_number：服务端把坏包替换成
 * 同版本新包时 id 会变，新 id 不在黑名单里，能自动放行新尝试。
 */
static void ota_manager_reconcile_last_attempt(void)
{
    char *pending = NULL;
    lisa_kv_get_string(KV_KEY_SYS_OTA_PENDING_PKG, &pending);
    if (pending == NULL || pending[0] == '\0') {
        lisa_kv_free(pending);
        return;
    }

    uboot_ota_failure_info_t info;
    if (uboot_ota_get_last_failure(&info) != 0) {
        info.reason = UBOOT_OTA_FAILURE_NONE;
    }

    if (info.reason == UBOOT_OTA_FAILURE_NONE) {
        LISA_LOGI(TAG, "App OTA pkg=%s applied OK, clear pending and blacklist", pending);
        /* 成功升级到 pending 包，遗留的黑名单项已经失去意义，一起清掉 */
        lisa_kv_set_string(KV_KEY_SYS_OTA_FAILED_PKG, "");
        s_app_update_applied_this_boot = true;
    } else {
        LISA_LOGW(TAG, "App OTA pkg=%s failed at boot (reason=%d detail=%d), blacklist", pending, info.reason,
                  info.detail);
        lisa_kv_set_string(KV_KEY_SYS_OTA_FAILED_PKG, pending);
    }
    lisa_kv_set_string(KV_KEY_SYS_OTA_PENDING_PKG, "");
    lisa_kv_free(pending);
}

static bool ota_manager_is_blacklisted(const char *package_id)
{
    if (!package_id || package_id[0] == '\0') {
        return false;
    }
    char *failed = NULL;
    lisa_kv_get_string(KV_KEY_SYS_OTA_FAILED_PKG, &failed);
    bool hit = (failed != NULL && failed[0] != '\0' && strcmp(failed, package_id) == 0);
    lisa_kv_free(failed);
    return hit;
}

static int _ota_manager_check_all(void)
{
    int ret;
    int skip_app_update = 0;
    int skip_wake_word_update = 0;
    int skip_prompt_tone_update = 0;
    int skip_emoji_update = 0;
    bool need_reboot = false;
    bool resource_check_failed = false;

    bool wake_word_need_update = false;
    bool prompt_tone_need_update = false;
    bool emoji_need_update = false;

    uint32_t start_time = xTaskGetTickCount();

    s_low_battery_skip_wait_power = false;

    /* 先把上一次 uboot 尝试的结果对账到 KV，必要时拉黑坏的 package_id */
    ota_manager_reconcile_last_attempt();

    /*
     * Phase 0: 检查并应用系统 OTA。
     * 系统升级必须先于资源升级：boot 应用新 CP 固件后，下一轮开机再由新固件去更新
     * 资源包。若此处系统 OTA 启动成功，函数会重启而不返回。
     */
#ifdef CONFIG_OTA_DISABLE_APP_UPDATE
    skip_app_update = 1;
#else
    if (lisa_kv_get_int(KV_KEY_USER_DISABLE_APP_UPDATE, &skip_app_update) != 0) {
        skip_app_update = 0;
    }
#endif

#ifdef CONFIG_OTA_DISABLE_WAKEWORD_UPDATE
    skip_wake_word_update = 1;
#else
    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_WAKEWORD_UPDATE, &skip_wake_word_update);
    if (ret != 0) {
        skip_wake_word_update = 0;
    }
#endif

#ifdef CONFIG_OTA_DISABLE_TONE_UPDATE
    skip_prompt_tone_update = 1;
#else
    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_TONE_UPDATE, &skip_prompt_tone_update);
    if (ret != 0) {
        skip_prompt_tone_update = 0;
    }
#endif

#ifdef CONFIG_OTA_DISABLE_EMOJI_UPDATE
    skip_emoji_update = 1;
#else
    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_EMOJI_UPDATE, &skip_emoji_update);
    if (ret != 0) {
        skip_emoji_update = 0;
    }
#endif

    /* 老 boot 不消费 control store，app 升级无法落地；资源包升级不受影响 */
    if (!skip_app_update && !uboot_features_has(UBOOT_FEATURE_OTA)) {
        LISA_LOGI(TAG, "Skip app OTA: boot lacks OTA support");
        skip_app_update = 1;
    }

    if (!skip_app_update && s_app_ota_checked_this_boot) {
        LISA_LOGI(TAG, "Skip app OTA: already checked this boot");
        skip_app_update = 1;
    }

    /* 电池供电且剩余 < 10% 时跳过所有 OTA，避免升级过程中掉电变砖。
     * USB 已插入时不看电量；机器没带电池的情况下也按"非电池供电"处理。 */
    if (!power_is_usb_plugged() && battery_get_status() != BATTERY_STATUS_NO_BATTERY) {
        uint8_t pct = battery_get_pct_raw();
        if (pct < 10) {
            LISA_LOGI(TAG, "Skip all OTA updates: on battery and pct=%u%% < 10%%", pct);
            ota_manager_play_tone_sync(TONE_ID_93, 1);
            skip_app_update = 1;
            skip_wake_word_update = 1;
            skip_prompt_tone_update = 1;
            skip_emoji_update = 1;
            s_low_battery_skip_wait_power = true;
        }
    }

    if (skip_app_update) {
        LISA_LOGI(TAG, "App OTA disabled, skip");
    } else {
        s_ota_state.target = OTA_TARGET_APP;
        ota_manager_notify_state(OTA_STATE_CHECKING);

        ota_app_package_t app_pkg;
        /* 请求前置位：即使网络异常或下载失败，本次开机也不再重复弹 APP OTA。 */
        s_app_ota_checked_this_boot = true;
        ret = ota_api_check_app(&app_pkg);
        if (ret == 0 && app_pkg.available && ota_manager_is_blacklisted(app_pkg.package_id)) {
            LISA_LOGW(TAG, "Skip app OTA: package_id=%s is blacklisted (prior boot failed to apply it)",
                      app_pkg.package_id);
        } else if (ret == 0 && app_pkg.available) {
            ota_app_decision_e decision;

            LISA_LOGI(TAG, "App OTA available (v%s -> v%s), waiting user confirmation", PROJECT_VERSION_STR,
                      app_pkg.version);

            s_ota_state.update_total = 1;
            s_ota_state.update_index = 1;

            decision = ota_manager_wait_app_decision(&app_pkg);
            if (decision == OTA_APP_DECISION_SKIP) {
                LISA_LOGI(TAG, "App OTA deferred by user, continue resource check");
            } else if ((ret = ota_manager_app_update(&app_pkg)) < 0) {
                LISA_LOGE(TAG, "App OTA failed (%d)", ret);
                if (s_ota_state.state != OTA_STATE_APP_FAILED) {
                    ota_manager_notify_state(OTA_STATE_APP_FAILED);
                }
                ota_manager_show_failure_notice();
                return ret;
            }
            /* ota_manager_app_update 成功路径中会触发重启，不会返回 */
        } else if (ret < 0) {
            LISA_LOGW(TAG, "App OTA check failed (%d), proceed with resource check", ret);
        } else {
            LISA_LOGI(TAG, "App OTA: already up-to-date");
        }
    }

    memset(&dev_conf, 0, sizeof(ota_dev_conf_t));

    if (skip_wake_word_update && skip_prompt_tone_update && skip_emoji_update) {
        LISA_LOGI(TAG, "Skip all resource updates");
    } else {
        s_ota_state.target = OTA_TARGET_UNSPECIFIED;
        ota_manager_notify_state(OTA_STATE_CHECKING);

        ret = ota_api_get_dev_conf(&dev_conf);
        if (ret < 0) {
            LISA_LOGW(TAG, "Get resource info failed (%d)", ret);
            resource_check_failed = true;
            goto check_complete;
        }

        // Phase 1: 检查哪些资源需要更新
        if (!skip_wake_word_update && dev_conf.wakeup_word.resource.size > 0) {
            ret = ota_flash_verify(OTA_PART_WAKE_WORD_BIN, dev_conf.wakeup_word.resource.md5,
                                   dev_conf.wakeup_word.resource.size);
            wake_word_need_update = (ret > 0);
            LISA_LOGI(TAG, "wake_word.bin need update: %d", wake_word_need_update);
        }

        if (!skip_prompt_tone_update && dev_conf.prompt_tone.size > 0) {
            ret = ota_flash_verify(OTA_PART_PROMPT_TONE_BIN, dev_conf.prompt_tone.md5,
                                   dev_conf.prompt_tone.size);
            prompt_tone_need_update = (ret > 0);
            LISA_LOGI(TAG, "prompt_tone.bin need update: %d", prompt_tone_need_update);
        }

        if (!skip_emoji_update && dev_conf.emoji.size > 0) {
            ret = ota_flash_verify(OTA_PART_EMOJI_BIN, dev_conf.emoji.md5, dev_conf.emoji.size);
            emoji_need_update = (ret > 0);
            LISA_LOGI(TAG, "emoji.bin need update: %d", emoji_need_update);
        }

        uint32_t total = (wake_word_need_update ? 1 : 0) +
                         (prompt_tone_need_update ? 1 : 0) +
                         (emoji_need_update ? 1 : 0);
        uint32_t index = 0;

        s_ota_state.update_total = total;
        s_ota_state.update_index = 0;

        LISA_LOGI(TAG, "Resources to update: %u", total);

        // Phase 2: 逐个更新
        if (total > 0) {
            ota_manager_play_app_success_tone_if_needed();
            ota_manager_play_tone_sync(TONE_ID_89, 1);
        }

        if (wake_word_need_update) {
            index++;
            s_ota_state.update_index = index;
            s_ota_state.target = OTA_TARGET_WAKE_WORD;

            ret = ota_manager_wake_word_update();
            if (ret < 0) {
                LISA_LOGE(TAG, "Wake word update failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_RESOURCE_FAILED);
                goto reboot;
            }
            LISA_LOGI(TAG, "Wake word updated");
            need_reboot = true;
        }

        // 无论是否更新，都同步唤醒词文本
        if (!skip_wake_word_update && dev_conf.wakeup_word.text[0] != '\0') {
            char *current_wake_word = NULL;
            lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &current_wake_word);
            if (current_wake_word == NULL || strcmp(current_wake_word, dev_conf.wakeup_word.text) != 0) {
                lisa_kv_set_string(KV_KEY_SYS_WAKEWORD, dev_conf.wakeup_word.text);
                LISA_LOGI(TAG, "Wake word set to: %s", dev_conf.wakeup_word.text);
            }
            lisa_kv_free(current_wake_word);
        }

        if (prompt_tone_need_update) {
            index++;
            s_ota_state.update_index = index;
            s_ota_state.target = OTA_TARGET_PROMPT_TONE;

            ret = ota_manager_prompt_tone_update();
            if (ret < 0) {
                LISA_LOGE(TAG, "Prompt tone update failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_RESOURCE_FAILED);
                goto reboot;
            }
            LISA_LOGI(TAG, "Prompt tone updated");
            need_reboot = true;
        }

        if (emoji_need_update) {
            index++;
            s_ota_state.update_index = index;
            s_ota_state.target = OTA_TARGET_EMOJI;

            ret = ota_manager_emoji_update();
            if (ret < 0) {
                LISA_LOGE(TAG, "Emoji update failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_RESOURCE_FAILED);
                goto reboot;
            }
            LISA_LOGI(TAG, "Emoji updated");
            need_reboot = true;
        }
    }

    if (need_reboot) {
        LISA_LOGI(TAG, "Rebooting to apply updates...");
        ota_manager_update_reboot_strategy();
        if (s_resources_updated_cb != NULL) {
            int cb_ret = s_resources_updated_cb(wake_word_need_update,
                                                prompt_tone_need_update,
                                                emoji_need_update,
                                                s_resources_updated_cb_user_data);
            if (cb_ret < 0) {
                LISA_LOGW(TAG, "resources_updated callback failed (%d), continue OTA success flow", cb_ret);
            }
        }
        ota_manager_notify_state(OTA_STATE_SUCCESSED);
        if (s_ota_state.reboot == OTA_REBOOT_STRATEGY_AUTO) {
            ota_manager_play_tone_sync(TONE_ID_90, 1);
        } else {
            ota_manager_play_tone_sync(TONE_ID_91, 3);
        }
        goto reboot;
    }

check_complete:
    if (resource_check_failed) {
        LISA_LOGW(TAG, "Resource update check skipped: configuration unavailable");
    } else {
        LISA_LOGI(TAG, "All resources up-to-date");
    }

    ota_manager_play_app_success_tone_if_needed();

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
    if (s_ota_state.state == OTA_STATE_RESOURCE_FAILED) {
        ota_manager_show_failure_notice();
    }

    ota_manager_do_reboot();

    return ret;
}

static void ota_manager_check_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待系统稳定后再执行 OTA 检查
    _ota_manager_check_all();
    lisa_thread_delete(NULL);
}

ota_state_e ota_manager_get_state(void)
{
    return s_ota_state.state;
}

int ota_manager_get_state_snapshot(ota_state_t *state)
{
    if (state == NULL) {
        return -1;
    }

    memcpy(state, &s_ota_state, sizeof(*state));
    return 0;
}

int ota_manager_register_resources_updated_cb(ota_manager_resources_updated_cb_t cb, void *user_data)
{
    s_resources_updated_cb = cb;
    s_resources_updated_cb_user_data = user_data;
    return 0;
}

int ota_manager_app_update_input_ready(void)
{
    if (s_ota_state.state != OTA_STATE_PACKAGE_INFO) {
        return 0;
    }

    return xTaskGetTickCount() >= s_app_prompt_ready_tick;
}

int ota_manager_check_all(void)
{
    if (s_ota_state.state == OTA_STATE_CHECKING ||
        s_ota_state.state == OTA_STATE_PACKAGE_INFO ||
        s_ota_state.state == OTA_STATE_UPDATING) {
        LISA_LOGI(TAG, "OTA check already active, skip duplicated request");
        return 0;
    }

    if (s_ota_state.state == OTA_STATE_APP_FAILED) {
        LISA_LOGI(TAG, "App OTA failed this boot, skip duplicated OTA check");
        return 0;
    }

    lisa_thread_attr_t attr = {
        .name = "ota_check",
        .stack_size = 16 * 1024,
        .priority = LISA_OS_PRIORITY_LOW, // 6, 和 voice_msg 队列同级，以保障 UI 及时更新
    };
    lisa_thread_create(&attr, ota_manager_check_task, NULL);

    return 0;
}

int ota_manager_check_after_power_connected(void)
{
    if (!s_low_battery_skip_wait_power) {
        return 0;
    }

    if (!power_is_usb_plugged()) {
        return 0;
    }

    if (s_ota_state.state == OTA_STATE_CHECKING ||
        s_ota_state.state == OTA_STATE_PACKAGE_INFO ||
        s_ota_state.state == OTA_STATE_UPDATING) {
        LISA_LOGI(TAG, "OTA check already active, keep low-battery retry pending");
        return 0;
    }

    LISA_LOGI(TAG, "Power connected after low-battery OTA skip, retry OTA check");
    s_low_battery_skip_wait_power = false;

    return ota_manager_check_all();
}

int ota_manager_confirm_app_update(void)
{
    if (s_ota_state.state != OTA_STATE_PACKAGE_INFO || ota_manager_ensure_app_decision_sem() != 0) {
        return -1;
    }

    s_app_decision = OTA_APP_DECISION_UPDATE;
    return xSemaphoreGive(s_app_decision_sem) == pdTRUE ? 0 : -1;
}

int ota_manager_skip_app_update(void)
{
    if (s_ota_state.state != OTA_STATE_PACKAGE_INFO || ota_manager_ensure_app_decision_sem() != 0) {
        return -1;
    }

    s_app_decision = OTA_APP_DECISION_SKIP;
    return xSemaphoreGive(s_app_decision_sem) == pdTRUE ? 0 : -1;
}

static int ota_manager_app_download_cb(void *user, uint32_t chunk_offset, const uint8_t *data, uint32_t size,
                                       uint32_t total)
{
    (void)user;

    int ret = ota_flash_update_step(OTA_PART_APP_STAGING, chunk_offset, data, size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write staging failed at offset %u, size %u (%d)", chunk_offset, size, ret);
        return ret;
    }

    uint32_t downloaded = chunk_offset + size;
    if (total > 0 && total > s_ota_state.bytes_total) {
        s_ota_state.bytes_total = total;
    }
    ota_manager_notify_progress(downloaded, s_ota_state.bytes_total);

    return 0;
}

/**
 * 下载系统 OTA 包，写入 Flash 暂存区，然后请求 boot 应用升级并重启。
 *
 * 成功路径不会返回（会触发软复位）。
 *
 * @return < 0 表示失败
 */
static int ota_manager_app_update(const ota_app_package_t *pkg)
{
    int ret;
    uint32_t total_size = (pkg != NULL) ? pkg->size : 0;

    ota_manager_play_tone_sync(TONE_ID_87, 1);

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = total_size;
    download_start_tick = xTaskGetTickCount();
    ota_manager_notify_state(OTA_STATE_UPDATING);

    LISA_LOGI(TAG, "Begin download app OTA from %s", pkg->url);

    ret = ota_flash_update_begin(OTA_PART_APP_STAGING,
                                 total_size > 0 ? total_size : OTA_FLASH_SIZE_UNKNOWN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Begin staging failed (%d)", ret);
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_APP_FAILED);
        goto fail;
    }

    ret = ota_api_download_app(pkg, ota_manager_app_download_cb, NULL);
    if (ret < 0) {
        LISA_LOGE(TAG, "Download app OTA failed (%d)", ret);
        ota_flash_update_finish(OTA_PART_APP_STAGING);  /* 释放 mutex，忽略返回值 */
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_APP_FAILED);
        goto fail;
    }

    ret = ota_flash_update_finish(OTA_PART_APP_STAGING);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish staging failed (%d)", ret);
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_APP_FAILED);
        goto fail;
    }

    uint32_t package_size = (uint32_t)ret;
    const void *mapped = NULL;
    ota_flash_get(OTA_PART_APP_STAGING, &mapped, NULL);
    uint32_t flash_offset = (uint32_t)mapped - CMN_FLASH_REGION;
    LISA_LOGI(TAG, "Staged app OTA: %u bytes at flash offset 0x%08X", package_size, flash_offset);

    /* 最终 100% 进度 */
    s_ota_state.bytes_processed = package_size;
    s_ota_state.bytes_total = package_size;
    s_ota_state.elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - download_start_tick);
    ota_manager_publish_state_event();

    /* 记录本次尝试的 package_id，重启回来后由 reconcile 用 boot 的失败报告
     * 给它盖章"成功"或"拉黑"。package_id 缺失时无法追踪，直接跳过记录，后续
     * 也就享受不到黑名单保护。 */
    if (pkg->package_id[0] != '\0') {
        lisa_kv_set_string(KV_KEY_SYS_OTA_PENDING_PKG, pkg->package_id);
    } else {
        LISA_LOGW(TAG, "App OTA package_id missing, cannot track failure for blacklist");
    }

    /* 交给 boot：设置 OTA request 后软重启；成功则 boot 下次启动接管升级 */
    ret = uboot_ota_start_from_flash(flash_offset, package_size);
    if (ret != 0) {
        LISA_LOGE(TAG, "uboot_ota_start_from_flash failed (%d)", ret);
        /* 请求压根没写进 control store，把刚才记下的 pending 清掉，
         * 否则下次重启时 reconcile 会把 boot 无关的上次失败记录挂在它头上 */
        lisa_kv_set_string(KV_KEY_SYS_OTA_PENDING_PKG, "");
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_APP_FAILED);
        goto fail;
    }

    LISA_LOGI(TAG, "App OTA request saved, rebooting into boot recovery");
    ota_manager_update_reboot_strategy();
    ota_manager_notify_state(OTA_STATE_SUCCESSED);

    ota_manager_do_reboot();

fail:
    return ret;
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
 * 下载并更新唤醒词（调用前已确认需要更新）
 *
 * @return 0: 成功, < 0: 失败
 */
static int ota_manager_wake_word_update(void)
{
    int ret;

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = dev_conf.wakeup_word.resource.size;
    download_start_tick = xTaskGetTickCount();
    ota_manager_notify_state(OTA_STATE_UPDATING);

    // 停止算法
    app_wakeup_stop();

    LISA_LOGI(TAG, "Begin update wake_word.bin...");

    for (int attempt = 0; attempt < OTA_DOWNLOAD_RETRY_COUNT; attempt++) {
        if (attempt > 0) {
            LISA_LOGW(TAG, "Wake word download retry %d/%d, %ums delay",
                      attempt + 1, OTA_DOWNLOAD_RETRY_COUNT,
                      OTA_DOWNLOAD_RETRY_DELAY_BASE_MS * attempt);
            vTaskDelay(pdMS_TO_TICKS(OTA_DOWNLOAD_RETRY_DELAY_BASE_MS * attempt));
        }

        ret = ota_flash_update_begin(OTA_PART_WAKE_WORD_BIN, dev_conf.wakeup_word.resource.size);
        if (ret < 0) {
            LISA_LOGE(TAG, "Begin update of wake_word.bin partition failed (%d)", ret);
            return ret;
        }

        LISA_LOGI(TAG, "Downloading wake_word.bin from %s, size %u (attempt %d)",
                  dev_conf.wakeup_word.resource.url,
                  dev_conf.wakeup_word.resource.size, attempt + 1);

        ret = ota_api_download(&dev_conf.wakeup_word.resource, ota_manager_wake_word_download_cb);
        if (ret >= 0) {
            break;
        }

        LISA_LOGW(TAG, "Wake word download attempt %d failed (%d)", attempt + 1, ret);
        ota_flash_update_abort(OTA_PART_WAKE_WORD_BIN);
    }

    if (ret < 0) {
        LISA_LOGE(TAG, "Download wake_word.bin failed after %d attempts (%d)",
                  OTA_DOWNLOAD_RETRY_COUNT, ret);
        return ret;
    }

    ret = ota_flash_update_finish(OTA_PART_WAKE_WORD_BIN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of wake_word.bin partition failed (%d)", ret);
        return ret;
    }

    // 发送最终 100% 进度
    s_ota_state.bytes_processed = s_ota_state.bytes_total;
    s_ota_state.elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - download_start_tick);
    ota_manager_publish_state_event();

    LISA_LOGI(TAG, "Update wake_word.bin finished");

    return 0;
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

/**
 * 下载并更新提示音（调用前已确认需要更新）
 *
 * @return 0: 成功, < 0: 失败
 */
static int ota_manager_prompt_tone_update(void)
{
    int ret;

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = dev_conf.prompt_tone.size;
    download_start_tick = xTaskGetTickCount();
    ota_manager_notify_state(OTA_STATE_UPDATING);

    LISA_LOGI(TAG, "Begin update prompt_tone.bin...");

    for (int attempt = 0; attempt < OTA_DOWNLOAD_RETRY_COUNT; attempt++) {
        if (attempt > 0) {
            LISA_LOGW(TAG, "Prompt tone download retry %d/%d, %ums delay",
                      attempt + 1, OTA_DOWNLOAD_RETRY_COUNT,
                      OTA_DOWNLOAD_RETRY_DELAY_BASE_MS * attempt);
            vTaskDelay(pdMS_TO_TICKS(OTA_DOWNLOAD_RETRY_DELAY_BASE_MS * attempt));
        }

        ret = ota_flash_update_begin(OTA_PART_PROMPT_TONE_BIN, dev_conf.prompt_tone.size);
        if (ret < 0) {
            LISA_LOGE(TAG, "Begin update of prompt_tone.bin partition failed (%d)", ret);
            return ret;
        }

        LISA_LOGI(TAG, "Downloading prompt_tone.bin from %s, size %u (attempt %d)",
                  dev_conf.prompt_tone.url, dev_conf.prompt_tone.size, attempt + 1);

        ret = ota_api_download(&dev_conf.prompt_tone, ota_manager_prompt_tone_download_cb);
        if (ret >= 0) {
            break;
        }

        LISA_LOGW(TAG, "Prompt tone download attempt %d failed (%d)", attempt + 1, ret);
        ota_flash_update_abort(OTA_PART_PROMPT_TONE_BIN);
    }

    if (ret < 0) {
        LISA_LOGE(TAG, "Download prompt_tone.bin failed after %d attempts (%d)",
                  OTA_DOWNLOAD_RETRY_COUNT, ret);
        return ret;
    }

    ret = ota_flash_update_finish(OTA_PART_PROMPT_TONE_BIN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of prompt_tone.bin partition failed (%d)", ret);
        return ret;
    }

    s_ota_state.bytes_processed = s_ota_state.bytes_total;
    s_ota_state.elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - download_start_tick);
    ota_manager_publish_state_event();

    LISA_LOGI(TAG, "Update prompt_tone.bin finished");

    return 0;
}

static int ota_manager_emoji_download_cb(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data,
                                         uint32_t size)
{
    int ret;

    ret = ota_flash_update_step(OTA_PART_EMOJI_BIN, offset, data, size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Write emoji.bin failed at offset %u, size %u (%d)", offset, size, ret);
    }

    ota_manager_notify_progress(offset + size, res_info->size);

    return ret;
}

/**
 * 下载并更新表情（调用前已确认需要更新）
 *
 * @return 0: 成功, < 0: 失败
 */
static int ota_manager_emoji_update(void)
{
    int ret;

    s_ota_state.bytes_processed = 0;
    s_ota_state.bytes_total = dev_conf.emoji.size;
    download_start_tick = xTaskGetTickCount();
    ota_manager_notify_state(OTA_STATE_UPDATING);

    LISA_LOGI(TAG, "Begin update emoji.bin...");

    for (int attempt = 0; attempt < OTA_DOWNLOAD_RETRY_COUNT; attempt++) {
        if (attempt > 0) {
            LISA_LOGW(TAG, "Emoji download retry %d/%d, %ums delay",
                      attempt + 1, OTA_DOWNLOAD_RETRY_COUNT,
                      OTA_DOWNLOAD_RETRY_DELAY_BASE_MS * attempt);
            vTaskDelay(pdMS_TO_TICKS(OTA_DOWNLOAD_RETRY_DELAY_BASE_MS * attempt));
        }

        ret = ota_flash_update_begin(OTA_PART_EMOJI_BIN, dev_conf.emoji.size);
        if (ret < 0) {
            LISA_LOGE(TAG, "Begin update of emoji.bin partition failed (%d)", ret);
            return ret;
        }

        LISA_LOGI(TAG, "Downloading emoji.bin from %s, size %u (attempt %d)",
                  dev_conf.emoji.url, dev_conf.emoji.size, attempt + 1);

        ret = ota_api_download(&dev_conf.emoji, ota_manager_emoji_download_cb);
        if (ret >= 0) {
            break; /* download + MD5 OK */
        }

        LISA_LOGW(TAG, "Emoji download attempt %d failed (%d)", attempt + 1, ret);

        /* 放弃本轮 flash 写入（释放 mutex，不刷缓冲区），
         * 下一轮 ota_flash_update_begin 会重新预擦分区 */
        ota_flash_update_abort(OTA_PART_EMOJI_BIN);
    }

    if (ret < 0) {
        LISA_LOGE(TAG, "Download emoji.bin failed after %d attempts (%d)",
                  OTA_DOWNLOAD_RETRY_COUNT, ret);
        return ret;
    }

    ret = ota_flash_update_finish(OTA_PART_EMOJI_BIN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish update of emoji.bin partition failed (%d)", ret);
        return ret;
    }

    s_ota_state.bytes_processed = s_ota_state.bytes_total;
    s_ota_state.elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - download_start_tick);
    ota_manager_publish_state_event();

    LISA_LOGI(TAG, "Update emoji.bin finished");

    return 0;
}
