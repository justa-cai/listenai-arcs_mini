#include "FreeRTOS.h"
#include "task.h"

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

#include "app_button.h"
#include "button_blecfg.h"
#include "voice_msg.h"
#include "lisa_kv.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "voice_player_comm.h"
#include "battery/battery.h"
#include "apps/llm/models/model_qrcode.h"
#include "voice_cloud.h"

/* 被动网络提示的单次触发状态。 */
static bool s_netcfg_tone_latched = false;
static bool s_cloud_ever_connected = false;
static bool s_bind_prompted_before_first_cloud_ok = false;
static bool s_runtime_wifi_prompted = false;
static bool s_boot_probe_fail_prompted = false;

/* 被动网络异常的展示策略暂留在 main，后续可独立改为 Home 页二维码表情。 */
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

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
static void app_show_wifi_emoji(void)
{
    static const char wifi_emoji[] = "wifi";

    voice_msg_pub(VOICE_MSG_CLOUD_EMOJI, (void *)wifi_emoji, sizeof(wifi_emoji));
}
#endif

static void app_open_cloud_info(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    if (status == QR_STATUS_NOT_CONNECTED) {
        app_show_wifi_emoji();
        return;
    }
#endif
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, &status, sizeof(status));
#else
    (void)status;
#endif
}

static void app_show_initial_wifi_info_if_needed(void)
{
    sys_network_status_t status = {0};
    
    if (sys_network_get_status(&status) == 0 &&
        status.mode == SYS_NETWORK_MODE_WIFI &&
        !sys_wifi_has_ap()) {
        app_open_cloud_info(QR_STATUS_NOT_CONNECTED);
    }
}

static bool app_wifi_connect_failed(void)
{
    switch (sys_wifi_get_connect_result()) {
    case SYS_WIFI_CONNECT_RESULT_FAIL_NO_AP:
    case SYS_WIFI_CONNECT_RESULT_FAIL_PASSWORD:
    case SYS_WIFI_CONNECT_RESULT_FAIL_IP:
    case SYS_WIFI_CONNECT_RESULT_FAIL_OTHER:
        return true;
    default:
        return false;
    }
}

static bool app_should_play_netcfg_tone(uint32_t status)
{
    if (status != QR_STATUS_NOT_CONNECTED) {
        return false;
    }

    /* 无保存 AP 的启动提示由 voice_player_network_tone 负责；main 只补充已保存 AP 的连接失败提示。 */
    return sys_wifi_has_ap() && app_wifi_connect_failed();
}

static void app_play_netcfg_tone_once(uint32_t status)
{
    if (s_netcfg_tone_latched || !app_should_play_netcfg_tone(status)) {
        return;
    }

    voice_player_play_tone_url(app_tone_get_url(TONE_ID_70));
    s_netcfg_tone_latched = true;
}

static void voice_system_network_probe_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    s_netcfg_tone_latched = false;
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
    s_boot_probe_fail_prompted = false;
}

static void voice_cloud_auth_failed(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (button_blecfg_is_user_entered() || s_cloud_ever_connected || s_bind_prompted_before_first_cloud_ok) {
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

static void voice_wifi_provision_guard(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    bool mark_boot_probe_prompted = false;
    bool user_entered_blecfg = button_blecfg_is_user_entered();

    if (msg_id == VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL) {
        if (user_entered_blecfg || s_cloud_ever_connected || s_boot_probe_fail_prompted) {
            return;
        }
        mark_boot_probe_prompted = true;
    }

    if (msg_id == VOICE_MSG_WIFI_DISCONNECTED) {
        if (s_cloud_ever_connected && !user_entered_blecfg && !s_runtime_wifi_prompted) {
            uint32_t status = QR_STATUS_NOT_CONNECTED;
            (void)app_should_open_info_by_cloud_state(&status);
            app_open_cloud_info(status);
            s_runtime_wifi_prompted = true;
        }

        if (s_cloud_ever_connected || user_entered_blecfg) {
            if (!user_entered_blecfg) {
                app_play_netcfg_tone_once(QR_STATUS_NOT_CONNECTED);
            }
            return;
        }
    }

    if (user_entered_blecfg) {
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

    uint32_t status = QR_STATUS_CONNECTED;
    if (!app_should_open_info_by_cloud_state(&status)) {
        return;
    }

    LISA_LOGI(TAG, "Cloud unavailable(%u), keep info page on top", (unsigned)status);
    app_open_cloud_info(status);
    app_play_netcfg_tone_once(status);

    if (mark_boot_probe_prompted) {
        s_boot_probe_fail_prompted = true;
    }
}

int main(int argc, char **argv)
{
    extern int boot_watchdog_feed(void);
    boot_watchdog_feed();

    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_system_network_probe_success, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, voice_wifi_provision_guard, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, voice_cloud_auth_failed, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS , voice_cloud_auth_success, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_DISCONNECTED, voice_wifi_provision_guard, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE, voice_wifi_provision_guard, NULL);

    service_led_init();
    service_volume_init();
    service_brightness_init();
    service_button_init();
    service_image_init();
    service_camera_init();

    app_button_init();

#if CONFIG_APPLICATION_UI
    extern int lisa_ui_init(void);
    lisa_ui_init();
#endif

    battery_init();

    app_show_initial_wifi_info_if_needed();


    vTaskDelay(pdMS_TO_TICKS(3000));
    while (1) {
        boot_watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return 0;
}
