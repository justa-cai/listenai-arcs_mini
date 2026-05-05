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
#include "project_version.h"
#include "uboot_features_api.h"
#include "uboot_ota_api.h"
#include "arcs_ap_base.h"  /* CMN_FLASH_REGION */
#include "battery.h"

#include "ota_manager.h"
#include "ota_api.h"
#include "ota_flash.h"
#include "voice_msg.h"
#include "kv_sys.h"
#include "kv_user.h"

#define OTA_FAILURE_UI_DISPLAY_MS 3000U

static int ota_manager_app_update(const ota_app_package_t *pkg);
static int ota_manager_wake_word_update(void);
static int ota_manager_prompt_tone_update(void);
static int ota_manager_emoji_update(void);

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
    case OTA_STATE_PACKAGE_INFO_FAILED:
        evt = VOICE_MSG_OTA_PACKAGE_INFO_FAILED;
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

    bool wake_word_need_update = false;
    bool prompt_tone_need_update = false;
    bool emoji_need_update = false;

    uint32_t start_time = xTaskGetTickCount();

    /* 先把上一次 uboot 尝试的结果对账到 KV，必要时拉黑坏的 package_id */
    ota_manager_reconcile_last_attempt();

    /*
     * Phase 0: 检查并应用系统 OTA。
     * 系统升级必须先于资源升级：boot 应用新 CP 固件后，下一轮开机再由新固件去更新
     * 资源包。若此处系统 OTA 启动成功，函数会重启而不返回。
     */
    if (lisa_kv_get_int(KV_KEY_USER_DISABLE_APP_UPDATE, &skip_app_update) != 0) {
        skip_app_update = 0;
    }

    /* 老 boot 不消费 control store，app 升级无法落地；资源包升级不受影响 */
    if (!skip_app_update && !uboot_features_has(UBOOT_FEATURE_OTA)) {
        LISA_LOGI(TAG, "Skip app OTA: boot lacks OTA support");
        skip_app_update = 1;
    }

    /* 电池供电且剩余 < 50% 时跳过系统 OTA，避免升级过程中掉电变砖。
     * USB 已插入时不看电量；机器没带电池的情况下也按"非电池供电"处理。 */
    if (!skip_app_update && !power_is_usb_plugged() &&
        battery_get_status() != BATTERY_STATUS_NO_BATTERY) {
        uint8_t pct = battery_get_pct_raw();
        if (pct < 50) {
            LISA_LOGI(TAG, "Skip app OTA: on battery and pct=%u%% < 50%%", pct);
            skip_app_update = 1;
        }
    }

    if (skip_app_update) {
        LISA_LOGI(TAG, "App OTA disabled, skip");
    } else {
        s_ota_state.target = OTA_TARGET_APP;
        ota_manager_notify_state(OTA_STATE_CHECKING);

        ota_app_package_t app_pkg;
        ret = ota_api_check_app(&app_pkg);
        if (ret == 0 && app_pkg.available && ota_manager_is_blacklisted(app_pkg.package_id)) {
            LISA_LOGW(TAG, "Skip app OTA: package_id=%s is blacklisted (prior boot failed to apply it)",
                      app_pkg.package_id);
        } else if (ret == 0 && app_pkg.available) {
            LISA_LOGI(TAG, "App OTA available (v%s -> v%s), applying before resources", PROJECT_VERSION_STR,
                      app_pkg.version);

            s_ota_state.update_total = 1;
            s_ota_state.update_index = 1;

            if (ota_manager_app_update(&app_pkg) < 0) {
                LISA_LOGE(TAG, "App OTA failed, will retry next boot");
                vTaskDelay(pdMS_TO_TICKS(OTA_FAILURE_UI_DISPLAY_MS)); // 展示失败UI提醒用户
                /* 系统 OTA 失败不应阻塞本次资源检查，继续往下走 */
            }
            /* ota_manager_app_update 成功路径中会触发重启，不会返回 */
        } else if (ret < 0) {
            LISA_LOGW(TAG, "App OTA check failed (%d), proceed with resource check", ret);
            ota_manager_notify_state(OTA_STATE_PACKAGE_INFO_FAILED);
            vTaskDelay(pdMS_TO_TICKS(OTA_FAILURE_UI_DISPLAY_MS)); // 展示失败UI提醒用户
        } else {
            LISA_LOGI(TAG, "App OTA: already up-to-date");
        }
    }

    memset(&dev_conf, 0, sizeof(ota_dev_conf_t));

    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_WAKEWORD_UPDATE, &skip_wake_word_update);
    if (ret != 0) {
        skip_wake_word_update = 0;
    }

    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_TONE_UPDATE, &skip_prompt_tone_update);
    if (ret != 0) {
        skip_prompt_tone_update = 0;
    }

    ret = lisa_kv_get_int(KV_KEY_USER_DISABLE_EMOJI_UPDATE, &skip_emoji_update);
    if (ret != 0) {
        skip_emoji_update = 0;
    }

    if (skip_wake_word_update && skip_prompt_tone_update && skip_emoji_update) {
        LISA_LOGI(TAG, "Skip all resource updates as per user settings");
    } else {
        s_ota_state.target = OTA_TARGET_UNSPECIFIED;
        ota_manager_notify_state(OTA_STATE_CHECKING);

        ret = ota_api_get_dev_conf(&dev_conf);
        if (ret < 0) {
            LISA_LOGW(TAG, "Get resource info failed (%d), trigger re-probe", ret);
            ota_manager_notify_state(OTA_STATE_PACKAGE_INFO_FAILED);
            vTaskDelay(pdMS_TO_TICKS(OTA_FAILURE_UI_DISPLAY_MS)); // 展示失败UI提醒用户
            return ret;
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
        if (wake_word_need_update) {
            index++;
            s_ota_state.update_index = index;
            s_ota_state.target = OTA_TARGET_WAKE_WORD;

            ret = ota_manager_wake_word_update();
            if (ret < 0) {
                LISA_LOGE(TAG, "Wake word update failed (%d)", ret);
                ota_manager_update_reboot_strategy();
                ota_manager_notify_state(OTA_STATE_FAILED);
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
                ota_manager_notify_state(OTA_STATE_FAILED);
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
                ota_manager_notify_state(OTA_STATE_FAILED);
                goto reboot;
            }
            LISA_LOGI(TAG, "Emoji updated");
            need_reboot = true;
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

int ota_manager_check_all(void)
{
    lisa_thread_attr_t attr = {
        .name = "ota_check",
        .stack_size = 16 * 1024,
        .priority = LISA_OS_PRIORITY_LOW, // 6, 和 voice_msg 队列同级，以保障 UI 及时更新
    };
    lisa_thread_create(&attr, ota_manager_check_task, NULL);

    return 0;
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

    s_ota_state.bytes_processed = 0;
    /* 服务端不返回包大小，无法提前知道总长度；先设为 0，下载进度按已下载量展示 */
    s_ota_state.bytes_total = 0;
    download_start_tick = xTaskGetTickCount();
    ota_manager_notify_state(OTA_STATE_UPDATING);

    LISA_LOGI(TAG, "Begin download app OTA from %s", pkg->url);

    ret = ota_flash_update_begin(OTA_PART_APP_STAGING, OTA_FLASH_SIZE_UNKNOWN);
    if (ret < 0) {
        LISA_LOGE(TAG, "Begin staging failed (%d)", ret);
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_FAILED);
        goto fail;
    }

    ret = ota_api_download_app(pkg, ota_manager_app_download_cb, NULL);
    if (ret < 0) {
        LISA_LOGE(TAG, "Download app OTA failed (%d)", ret);
        ota_flash_update_finish(OTA_PART_APP_STAGING);  /* 释放 mutex，忽略返回值 */
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_FAILED);
        goto fail;
    }

    ret = ota_flash_update_finish(OTA_PART_APP_STAGING);
    if (ret < 0) {
        LISA_LOGE(TAG, "Finish staging failed (%d)", ret);
        ota_manager_update_reboot_strategy();
        ota_manager_notify_state(OTA_STATE_FAILED);
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
        ota_manager_notify_state(OTA_STATE_FAILED);
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

    ret = ota_flash_update_begin(OTA_PART_EMOJI_BIN, dev_conf.emoji.size);
    if (ret < 0) {
        LISA_LOGE(TAG, "Begin update of emoji.bin partition failed (%d)", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Downloading emoji.bin from %s, size %u", dev_conf.emoji.url, dev_conf.emoji.size);

    ret = ota_api_download(&dev_conf.emoji, ota_manager_emoji_download_cb);
    if (ret < 0) {
        LISA_LOGE(TAG, "Download emoji.bin failed (%d)", ret);
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
