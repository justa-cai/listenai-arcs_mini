#include "FreeRTOS.h"
#include "task.h"
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
#include "alarm_ring.h"
#include "alarm.h"
#include "alarm_handler.h"

#include "voice_msg.h"
#include "lisa_kv.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "app_player.h"
#include "pa_manager.h"
#include "voice_player_comm.h"
#include "power/power_manager.h"
#include "uboot_features_api.h"
#include "lisa_display.h"
#include "battery/battery.h"
#include "lisa_ui_nav_scr.h"
#include "voice_cloud.h"
#include "app_ble_common.h"
#include "button_camera_preview.h"
#include "bt_app_hal.h"
#include "ota_manager.h"

extern uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
extern int boot_watchdog_feed(void);

#define FACTORY_RESET_TONE_POLL_MS 20U
#define BLE_NETCFG_PREPARE_DELAY_MS 1000U

static volatile bool s_ble_netcfg_entering = false;

static bool app_should_lock_wifi_provision_page(void)
{
    sys_network_status_t status;

    return sys_network_get_status(&status) == 0 && status.wifi_provision_required;
}

static bool app_try_lock_wifi_provision_page(void)
{
    sys_network_status_t status;
    bool ws_connected = voice_cloud_is_connected();

    if (app_should_lock_wifi_provision_page()) {
        return true;
    }

    if (sys_network_get_status(&status) != 0 ||
        !status.wifi_available ||
        status.active_bearer == SYS_NETWORK_BEARER_MODEM) {
        return false;
    }

    if (status.wifi_connected && ws_connected) {
        return false;
    }

    LISA_LOGI(TAG, "WiFi or WS unavailable, enter forced WiFi provisioning (wifi=%d, ws=%d)",
              status.wifi_connected, ws_connected);
    if (!sys_wifi_is_started() && sys_wifi_start(false) != 0) {
        LISA_LOGW(TAG, "Failed to start WiFi before forced provisioning");
        return false;
    }
    sys_wifi_set_force_provision(true);
    return true;
}

static void app_open_wifi_provision_info(void)
{
#if CONFIG_WIFI_MANAGER
    app_ble_adv_start(0, BLE_ADV_GEN);
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, NULL, 0);
#endif
}

static void app_prompt_wifi_provision(void)
{
#if CONFIG_WIFI_MANAGER
    static TickType_t s_last_prompt_tick = 0;
    TickType_t now = xTaskGetTickCount();

    if (s_last_prompt_tick == 0 || (now - s_last_prompt_tick) >= pdMS_TO_TICKS(1000)) {
        app_player_play(tone_player, app_tone_get_url(TONE_ID_70));
        s_last_prompt_tick = now;
    }

    app_open_wifi_provision_info();
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
    sys_wifi_set_user_force_provision(true);
    sys_wifi_set_force_provision(true);
    service_alarm_deinit();
    if (!status_ok || status.active_bearer != SYS_NETWORK_BEARER_MODEM) {
        voice_cloud_disconnect();
    } else {
        LISA_LOGI(TAG, "keep voice cloud connected on modem bearer, skip WiFi reset before BLE config");
    }

    app_ble_netcfg_prepare();
    vTaskDelay(pdMS_TO_TICKS(BLE_NETCFG_PREPARE_DELAY_MS));

    if (sys_wifi_clear_saved_aps() != 0) {
        LISA_LOGW(TAG, "Failed to reset WiFi state");
    }

    app_prompt_wifi_provision();
    sys_wifi_set_user_force_provision(false);

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
    if (app_player_play(tone_player, app_tone_get_url(TONE_ID_103)) == APP_PLAYER_OK) {
        factory_reset_wait_tone_finished();
    } else {
        LISA_LOGW(TAG, "Failed to play factory reset tone, reboot immediately");
    }
    power_reboot_soft();
}

static void voice_system_network_probe_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    // 这里可以初始化需要网络的服务
}

static void voice_cloud_connected(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (!sys_wifi_get_user_force_provision() && sys_wifi_get_force_provision()) {
        LISA_LOGI(TAG, "WS connected, exit forced WiFi provisioning");
        sys_wifi_set_force_provision(false);
    }
}

static void voice_cloud_auth_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (sys_network_on_cloud_auth_success() != 0) {
        LISA_LOGW(TAG, "Failed to switch modem runtime tuning to cloud fast profile");
    }

    service_alarm_init();
}

#ifdef CONFIG_OTA
static bool app_should_defer_wifi_provision_prompt(void)
{
    switch (ota_manager_get_state()) {
    case OTA_STATE_CHECKING:
    case OTA_STATE_UPDATING:
    case OTA_STATE_FAILED:
    case OTA_STATE_PACKAGE_INFO_FAILED:
        return true;
    default:
        return false;
    }
}
#endif

static void voice_wifi_provision_guard(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
#ifdef CONFIG_OTA
    if (app_should_defer_wifi_provision_prompt()) {
        LISA_LOGI(TAG, "OTA is active, defer WiFi provisioning page");
        return;
    }
#endif

    if (!app_should_lock_wifi_provision_page()) {
        return;
    }

    LISA_LOGI(TAG, "WiFi provisioning required, keep info page on top");
    app_open_wifi_provision_info();
}

static void voice_wifi_force_provision_required(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
#ifdef CONFIG_OTA
    if (app_should_defer_wifi_provision_prompt()) {
        LISA_LOGI(TAG, "OTA is active, defer WiFi provisioning prompt");
        return;
    }
#endif

    if (!app_should_lock_wifi_provision_page()) {
        return;
    }

    LISA_LOGI(TAG, "WiFi auto connect failed, prompt provisioning");
    app_prompt_wifi_provision();
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

    if (evt->action != VOICE_MSG_BUTTON_ACTION_LONG_HOLD &&
        app_try_lock_wifi_provision_page()) {
        LISA_LOGI(TAG, "Button redirects to WiFi provisioning");
        app_prompt_wifi_provision();
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
                if (model_voice_tts_is_playing()) {
                    LISA_LOGI(TAG, "Single click: TTS playing, stop it");
                    app_player_stop(tts_player);
                    break;
                }
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
            LISA_LOGI(TAG, "power button triple click, open info page");
            if (voice_cloud_is_connected()) {
                app_player_play(tone_player, app_tone_get_url(TONE_ID_104));
            } else {
                app_player_play(tone_player, app_tone_get_url(TONE_ID_64));
            }
            voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, NULL, 0);
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
    LOGI("Application version: %s-%s", PROJECT_VERSION_STR, PROJECT_VERSION_COMMIT);

    boot_watchdog_feed();
    button_camera_preview_init();

    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_system_network_probe_success, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS , voice_cloud_auth_success, NULL);
    voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, button_changed, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_DISCONNECTED, voice_wifi_provision_guard, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_FORCE_PROVISION_REQUIRED, voice_wifi_force_provision_required, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE, voice_wifi_provision_guard, NULL);

    service_led_init();
    service_volume_init();
    service_brightness_init();
    service_button_init();
    service_image_init();
    #if !CONFIG_LISA_MODEM
    service_camera_init();
    #endif


#if CONFIG_APPLICATION_UI
    extern int lisa_ui_init(void);
    lisa_ui_init();
#endif

    battery_init();

#if CONFIG_WIFI_MANAGER
    if (app_should_lock_wifi_provision_page()) {
        app_open_wifi_provision_info();
        app_player_play(tone_player, app_tone_get_url(TONE_ID_70));
    }
#endif

    vTaskDelay(pdMS_TO_TICKS(3000));
    while (1) {
        boot_watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return 0;
}
