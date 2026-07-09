#include "FreeRTOS.h"
#include "task.h"
#include <errno.h>
#include "project_version.h"

#define TAG "main"

#include "lisa_log.h"

#include "service_led.h"
#include "service_volume.h"
#include "service_brightness.h"
#include "service_button.h"
#include "service_camera.h"
#include "service_alarm.h"
#include "service_image.h"
#include "service_sd_music.h"
#include "alarm_ring.h"
#include "alarm.h"
#include "alarm_handler.h"

#include "voice_msg.h"
#include "lisa_kv.h"
#include "kv_user.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "app_player.h"
#include "pa_manager.h"
#include "voice_player_comm.h"
#include "voice_player.h"
#include "power/power_manager.h"
#include "uboot_features_api.h"
#include "lisa_display.h"
#include "battery/battery.h"
#include "lisa_ui_nav_scr.h"
#include "apps/llm/lisa_ui_nav_scr_ids.h"
#include "apps/llm/models/model_qrcode.h"
#include "voice_cloud.h"
#include "app_ble_common.h"
#include "button_camera_preview.h"
#include "bt_app_hal.h"
#include "ota_manager.h"

extern int boot_watchdog_feed(void);

#define FACTORY_RESET_TONE_POLL_MS 20U

static volatile bool s_ble_netcfg_entering = false;
static bool s_netcfg_tone_latched = false;
static bool s_netcfg_tone_retry_pending = false;
static TickType_t s_netcfg_tone_retry_tick = 0;
static bool s_cloud_ever_connected = false;
static bool s_bind_prompted_before_first_cloud_ok = false;
static bool s_runtime_wifi_prompted = false;
static bool s_user_forced_netcfg = false;
static bool s_boot_probe_fail_prompted = false;

static bool app_cloud_info_page_enabled(void)
{
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    return false;
#else
    return true;
#endif
}

static bool app_should_open_info_by_cloud_state(uint32_t *status_out)
{
    uint32_t status = QR_STATUS_CONNECTED;

    switch (voice_cloud_get_state()) {
    case VOICE_CLOUD_STATE_CONNECTING:
        return false;
    case VOICE_CLOUD_STATE_NO_NETWORK:
    case VOICE_CLOUD_STATE_NO_INTERNET:
        status = QR_STATUS_NOT_CONNECTED;
        break;
    case VOICE_CLOUD_STATE_TOKEN_FAILED:
        status = QR_STATUS_AUTH_FAILED;
        break;
    case VOICE_CLOUD_STATE_CONNECT_FAILED:
        status = voice_cloud_is_device_unbound() ? QR_STATUS_BIND : QR_STATUS_NOT_CONNECTED;
        break;
    default:
        return false;
    }

    if (status_out) {
        *status_out = status;
    }

    return true;
}

static void app_open_cloud_info(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, &status, sizeof(status));
#else
    (void)status;
#endif
}

static bool app_should_play_netcfg_tone(uint32_t status)
{
    sys_wifi_connect_result_t connect_result;

    if (status != QR_STATUS_NOT_CONNECTED) {
        return false;
    }

    if (!sys_wifi_has_ap()) {
        return true;
    }

    connect_result = sys_wifi_get_connect_result();
    if (connect_result == SYS_WIFI_CONNECT_RESULT_FAIL_NO_AP ||
        connect_result == SYS_WIFI_CONNECT_RESULT_FAIL_PASSWORD ||
        connect_result == SYS_WIFI_CONNECT_RESULT_FAIL_IP ||
        connect_result == SYS_WIFI_CONNECT_RESULT_FAIL_OTHER) {
        return true;
    }

    switch (voice_cloud_get_state()) {
    case VOICE_CLOUD_STATE_CONNECTING:
        return false;
    case VOICE_CLOUD_STATE_NO_INTERNET:
        return true;
    default:
        return false;
    }
}

static bool app_try_play_netcfg_tone(uint32_t status)
{
    static TickType_t s_last_prompt_tick = 0;
    TickType_t now = xTaskGetTickCount();

    if (!app_should_play_netcfg_tone(status)) {
        s_netcfg_tone_retry_pending = false;
        return false;
    }

    if (s_netcfg_tone_latched) {
        s_netcfg_tone_retry_pending = false;
        return false;
    }

    if (s_last_prompt_tick == 0 || (now - s_last_prompt_tick) >= pdMS_TO_TICKS(1000)) {
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_70));
        s_last_prompt_tick = now;
        s_netcfg_tone_latched = true;
        s_netcfg_tone_retry_pending = false;
        return true;
    }

    s_netcfg_tone_retry_pending = true;
    s_netcfg_tone_retry_tick = now + pdMS_TO_TICKS(1000);
    return false;
}

static void app_prompt_cloud_info(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
    app_try_play_netcfg_tone(status);
    app_open_cloud_info(status);
#else
    (void)status;
#endif
}

static void factory_reset_wait_tone_finished(void)
{
    bool tone_started = false;

    while (1) {
        switch (app_player_get_state(tone_player)) {
        case APP_PLAYER_STATE_PREPARING:
        case APP_PLAYER_STATE_PREPARED:
        case APP_PLAYER_STATE_PLAYING:
        case APP_PLAYER_STATE_PAUSED:
            tone_started = true;
            break;
        case APP_PLAYER_STATE_STOPPED:
        case APP_PLAYER_STATE_ERROR:
            return;
        default:
            if (tone_started) {
                return;
            }
            break;
        }

        boot_watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(FACTORY_RESET_TONE_POLL_MS));
    }
}

static void app_ble_netcfg_enter_task(void *arg)
{
    sys_network_status_t status = {0};
    bool status_ok;

    (void)arg;

    if (!button_camera_preview_wait_exit()) {
        LISA_LOGW(TAG, "Timed out waiting camera preview to exit before BLE config");
    }

    status_ok = sys_network_get_status(&status) == 0;

    LISA_LOGI(TAG, "power button multi click, enter BLE config");
    s_user_forced_netcfg = true;
    service_alarm_deinit();
    if (sys_wifi_clear_saved_aps() != 0) {
        LISA_LOGW(TAG, "Failed to reset WiFi state");
    }
    kv_user_clear_sd_card_sync();
    LISA_LOGI(TAG, "TF card sync KV cleared before BLE config");

    if (!status_ok || status.active_bearer != SYS_NETWORK_BEARER_MODEM) {
        voice_cloud_disconnect();
    } else {
        LISA_LOGI(TAG, "keep voice cloud connected on modem bearer before BLE config");
    }

    app_ble_netcfg_adv_start_delayed();

    app_prompt_cloud_info(QR_STATUS_NOT_CONNECTED);

    s_ble_netcfg_entering = false;
    vTaskDelete(NULL);
}

static void app_ble_netcfg_enter_async(void)
{
    BaseType_t ret;

    if (s_ble_netcfg_entering) {
        LISA_LOGI(TAG, "BLE netcfg enter already in progress");
        return;
    }

#if CONFIG_LISA_MODEM
    {
        sys_network_status_t net_status = {0};
        if (sys_network_get_status(&net_status) == 0 && net_status.mode == SYS_NETWORK_MODE_MODEM) {
            if (net_status.connected) {
                LISA_LOGI(TAG, "4G online, show binding QR");
                app_prompt_cloud_info(QR_STATUS_BIND);
                return;
            }
            LISA_LOGI(TAG, "4G offline, switch to WiFi mode for netcfg");
            sys_network_request_mode(SYS_NETWORK_MODE_WIFI, true);
        }
    }
#endif

    s_ble_netcfg_entering = true;
    ret = xTaskCreate(app_ble_netcfg_enter_task, "ble_netcfg", 3072, NULL, 5, NULL);
    if (ret != pdPASS) {
        s_ble_netcfg_entering = false;
        LISA_LOGE(TAG, "Failed to create BLE netcfg task");
    }
}

void factory_reset(void)
{
    LOGI("factory reset running...");
    lisa_kv_clear();
    if (sys_wifi_clear_saved_aps() != 0) {
        LISA_LOGW(TAG, "Failed to reset WiFi state");
    }
    LOGI("factory reset done.");
    LOGI("Wait for factory reset tone playback before reboot.");
    const char *tone_url = app_tone_get_url(TONE_ID_103);
    if (tone_url && app_player_play(tone_player, tone_url) == APP_PLAYER_OK) {
        factory_reset_wait_tone_finished();
    } else {
        LISA_LOGW(TAG, "Failed to play factory reset tone (url=%p), reboot immediately", (void *)tone_url);
    }
    power_reboot_soft();
}

static void voice_system_network_probe_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    s_boot_probe_fail_prompted = false;
}

static void voice_cloud_connected(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    s_netcfg_tone_latched = false;
    s_cloud_ever_connected = true;
    s_bind_prompted_before_first_cloud_ok = false;
    s_runtime_wifi_prompted = false;
    s_user_forced_netcfg = false;
    s_boot_probe_fail_prompted = false;
}

static void app_open_info_with_tone(uint32_t status, uint16_t tone_id)
{
    if (tone_id != TONE_ID_0) {
        voice_player_play_tone_url(app_tone_get_url(tone_id));
    }

    app_open_cloud_info(status);
}


static void voice_cloud_auth_failed(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (s_user_forced_netcfg || s_cloud_ever_connected || s_bind_prompted_before_first_cloud_ok) {
        return;
    }

    app_open_cloud_info(QR_STATUS_AUTH_FAILED);
    s_bind_prompted_before_first_cloud_ok = true;
}


static void voice_cloud_auth_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    int network_mode = SYS_NETWORK_MODE_WIFI;

    service_alarm_init();
    (void)lisa_kv_get_int("user.network_mode", &network_mode);
    if (network_mode != SYS_NETWORK_MODE_MODEM) {
        service_sd_music_init();
    }
}

#ifdef CONFIG_OTA
static bool app_should_defer_wifi_provision_prompt(void)
{
    switch (ota_manager_get_state()) {
    case OTA_STATE_CHECKING:
    case OTA_STATE_PACKAGE_INFO:
    case OTA_STATE_UPDATING:
    case OTA_STATE_APP_FAILED:
    case OTA_STATE_RESOURCE_FAILED:
        return true;
    default:
        return false;
    }
}
#endif

static void voice_wifi_provision_guard(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    bool mark_boot_probe_prompted = false;

    if (msg_id == VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL) {
        if (s_user_forced_netcfg || s_cloud_ever_connected || s_boot_probe_fail_prompted) {
            return;
        }
        mark_boot_probe_prompted = true;
    }

    if (msg_id == VOICE_MSG_WIFI_DISCONNECTED) {
        if (s_cloud_ever_connected && !s_user_forced_netcfg && !s_runtime_wifi_prompted) {
            uint32_t status = QR_STATUS_NOT_CONNECTED;
            (void)app_should_open_info_by_cloud_state(&status);
            app_open_cloud_info(status);
            s_runtime_wifi_prompted = true;
        }

        if (s_cloud_ever_connected || s_user_forced_netcfg) {
            return;
        }
    }

    if (s_user_forced_netcfg) {
        return;
    }

    if (msg_id == VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE && sys_wifi_has_ap()) {
        return;
    }

    /* 4G 模式：NETWORK_SWITCH_DONE 仅表示模组启动完成，网络探测（SNTP）尚未结束。
     * 此时不应播报配网提示音，应等待 NETWORK_PROBE_FAIL 再做最终判断。 */
    if (msg_id == VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE) {
        sys_network_status_t _guard_net_status;
        if (sys_network_get_status(&_guard_net_status) == 0 &&
            _guard_net_status.active_bearer == SYS_NETWORK_BEARER_MODEM) {
            return;
        }
    }

#ifdef CONFIG_OTA
    if (app_should_defer_wifi_provision_prompt()) {
        LISA_LOGI(TAG, "OTA is active, defer WiFi provisioning page");
        return;
    }
#endif

    uint32_t status = QR_STATUS_CONNECTED;
    if (!app_should_open_info_by_cloud_state(&status)) {
        return;
    }

    LISA_LOGI(TAG, "Cloud unavailable(%u), keep info page on top", (unsigned)status);
    app_open_cloud_info(status);
    app_try_play_netcfg_tone(status);

    if (mark_boot_probe_prompted) {
        s_boot_probe_fail_prompted = true;
    }
}

static bool app_try_handle_ota_package_info(const voice_msg_button_evt_t *evt)
{
#ifdef CONFIG_OTA
    if (evt == NULL || evt->button_id != 0 || ota_manager_get_state() != OTA_STATE_PACKAGE_INFO) {
        return false;
    }

    if (!ota_manager_app_update_input_ready()) {
        LISA_LOGI(TAG, "Ignore button action=%d during OTA package info guard window", evt->action);
        return true;
    }

    if (evt->action == VOICE_MSG_BUTTON_ACTION_CLICK) {
        if (ota_manager_confirm_app_update() == 0) {
            LISA_LOGI(TAG, "User confirmed app OTA update");
        } else {
            LISA_LOGW(TAG, "Failed to confirm app OTA update");
        }
        return true;
    }

    if (evt->action == VOICE_MSG_BUTTON_ACTION_LONG_HOLD) {
        if (ota_manager_skip_app_update() == 0) {
            LISA_LOGI(TAG, "User deferred app OTA update");
        } else {
            LISA_LOGW(TAG, "Failed to defer app OTA update");
        }
        return true;
    }

    LISA_LOGI(TAG, "Ignore button action=%d while waiting app OTA confirmation", evt->action);
    return true;
#else
    (void)evt;
    return false;
#endif
}

static bool app_button_action_should_redirect_cloud_info(voice_msg_button_action_t action)
{
    switch (action) {
    case VOICE_MSG_BUTTON_ACTION_CLICK:
    case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_REPEAT_CLICK:
        return true;
    default:
        return false;
    }
}

static void button_changed(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    voice_msg_button_evt_t *evt = (voice_msg_button_evt_t *)data;

    if (!evt || len < sizeof(*evt)) {
        return;
    }

    if (evt->button_id != 0) {
        return;
    }

    if (app_try_handle_ota_package_info(evt)) {
        return;
    }

    uint32_t redirect_status = QR_STATUS_CONNECTED;
    if (app_button_action_should_redirect_cloud_info(evt->action) &&
        app_should_open_info_by_cloud_state(&redirect_status)) {
        LISA_LOGI(TAG, "Button redirects to cloud info, status=%u", (unsigned)redirect_status);
        app_prompt_cloud_info(redirect_status);
        return;
    }

    switch (evt->action) {
        case VOICE_MSG_BUTTON_ACTION_CLICK:
        {
            /* 单击：照片预览时触发拍照；响铃时稍后提醒；非主页先回主页；主页则触发按键唤醒 */
            // 触发拍照
            if (button_camera_preview_handle_click()) {
                break;
            }
            // 响铃时稍后提醒
            if (alarm_ring_is_active()) {
                LISA_LOGI(TAG, "Single click: alarm ringing, handle snooze");
                alarm_handle_snooze();
                alarm_ring_stop();
                break;
            }
            // 非主页先回主页
            if (lisa_ui_nav_scr_get_top_id() != 0) {
                LISA_LOGI(TAG, "Single click: not on home page, navigating home");
                lisa_ui_nav_scr_nav_to(0);
                break;
            } else {
                // 主页则触发按键唤醒
                LISA_LOGI(TAG, "Single click: wakeup trigger");
                // 如果会话中，退出会话
                if (model_voice_cloud_is_running()) {
                    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
                } else {
                    // 否则触发唤醒
                    const char keyword[] = "xiao ling xiao ling";
                    voice_msg_pub(VOICE_MSG_WAKEUP_KEYWORD, (void *)keyword, sizeof(keyword));
                }
            }
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
        {
            button_camera_preview_handle_double_click();
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK:
        {
            /* 三击：打开信息/二维码页 */
            uint32_t status = QR_STATUS_CONNECTED;
            (void)app_should_open_info_by_cloud_state(&status);
            LISA_LOGI(TAG, "power button triple click, open info page");
            if (voice_cloud_is_connected()) {
                voice_player_play_prompt_tone_url(app_tone_get_url(TONE_ID_104));
            } else {
                voice_player_play_prompt_tone_url(app_tone_get_url(TONE_ID_64));
            }

            if (!app_cloud_info_page_enabled()) {
                LISA_LOGI(TAG, "power button triple click, info page disabled on this board");
                break;
            }
            app_open_cloud_info(status);
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK:
        {
            /* 四击： */
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
        case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
        case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
        {
            app_ble_netcfg_enter_async();
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_REPEAT_CLICK: 
        {
            /* 连击 >=8：恢复出厂 */
            LISA_LOGI(TAG, "power button repeat click, factory reset");
            factory_reset();
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_LONG_HOLD:
        {
            /* 长按：响铃时停止闹铃并生成下一个闹钟；照片预览页则退出预览页；否则关机（USB 供电时忽略） */
            // 退出照片预览
            if (button_camera_preview_handle_long_hold()) {
                break;
            }
            // 关闭闹铃并生成下一个闹钟
            if (alarm_ring_is_active()) {
                LISA_LOGI(TAG, "Double click: alarm ringing, stop and generate next");
                alarm_ring_stop();
                alarm_handle_stop_and_next();
                break;
            }
            /* 关机：新 boot 下 power_shutdown 自身会按供电方式分流
             * （纯电池真关机；USB 插着走 stage0 假关机）。老 boot 下
             * USB 插着拉低 PWR_LOCK 无效，跳过避免用户以为关了却没关。*/
            if (!uboot_features_has(UBOOT_FEATURE_POWER_GUARD) && power_is_usb_plugged()) {
                LISA_LOGI(TAG, "USB connected on legacy boot, ignore long press shutdown");
            } else {
                LISA_LOGI(TAG, "power button long press hold, shutting down...");
                power_shutdown();
            }
            break;
        }
    }
}

int main(int argc, char **argv)
{
    sys_network_status_t network_status = {0};
    bool wifi_mode = false;

    LOGI("Application version: %s-%s", PROJECT_VERSION_STR, PROJECT_VERSION_COMMIT);

    boot_watchdog_feed();
    button_camera_preview_init();

    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_system_network_probe_success, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, voice_wifi_provision_guard, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, voice_cloud_auth_failed, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS , voice_cloud_auth_success, NULL);
    voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, button_changed, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_DISCONNECTED, voice_wifi_provision_guard, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE, voice_wifi_provision_guard, NULL);

    service_led_init();
    service_volume_init();
    service_brightness_init();
    service_button_init();
    service_image_init();
    service_camera_init();


#if CONFIG_APPLICATION_UI
    extern int lisa_ui_init(void);
    lisa_ui_init();
#endif

    battery_init();

    wifi_mode = (sys_network_get_status(&network_status) == 0) &&
                (network_status.mode == SYS_NETWORK_MODE_WIFI);
    if (wifi_mode && !sys_wifi_has_ap()) {
#ifdef CONFIG_OTA
        if (!app_should_defer_wifi_provision_prompt())
#endif
        {
            app_open_cloud_info(QR_STATUS_NOT_CONNECTED);
        }
    }


    vTaskDelay(pdMS_TO_TICKS(3000));
    while (1) {
        boot_watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return 0;
}
