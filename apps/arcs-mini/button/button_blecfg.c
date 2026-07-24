#include "FreeRTOS.h"
#include "task.h"

#define TAG "button.blecfg"

#include "app_ble_common.h"
#include "apps/llm/models/model_qrcode.h"
#include "kv_user.h"
#include "lisa_log.h"
#include "service_alarm.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "voice_player_comm.h"

#include "button_blecfg.h"
#include "button_camera_preview.h"

#define BUTTON_BLECFG_TASK_STACK_SIZE 3072U
#define BUTTON_BLECFG_TASK_PRIORITY   5U

static volatile bool s_task_running;
static bool s_user_entered;

static void button_blecfg_show_wifi_emoji(void)
{
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    static const char wifi_emoji[] = "wifi";

    voice_msg_pub(VOICE_MSG_CLOUD_EMOJI, (void *)wifi_emoji, sizeof(wifi_emoji));
#endif
}

static void button_blecfg_open_page(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    if (status == QR_STATUS_NOT_CONNECTED) {
        button_blecfg_show_wifi_emoji();
        return;
    }
#endif
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, &status, sizeof(status));
#else
    (void)status;
#endif
}

static void button_blecfg_prompt_page(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
    if (status == QR_STATUS_NOT_CONNECTED) {
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_70));
    }
    button_blecfg_open_page(status);
#else
    (void)status;
#endif
}

static void button_blecfg_task(void *arg)
{
    sys_network_status_t status = {0};
    bool status_ok;

    (void)arg;

    if (!button_camera_preview_wait_exit()) {
        LISA_LOGW(TAG, "Timed out waiting camera preview to exit before BLE config");
    }

    status_ok = sys_network_get_status(&status) == 0;

    LISA_LOGI(TAG, "power button multi click, enter BLE config");
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
    button_blecfg_prompt_page(QR_STATUS_NOT_CONNECTED);

    s_task_running = false;
    vTaskDelete(NULL);
}

static void button_blecfg_on_cloud_connected(void *unused, uint32_t msg_id,
                                              void *data, uint32_t len,
                                              void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    s_user_entered = false;
}

int button_blecfg_init(void)
{
    int ret;

    s_user_entered = false;
    ret = voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED,
                        button_blecfg_on_cloud_connected,
                        NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to subscribe cloud connected event, ret=%d", ret);
    }
    return ret;
}

bool button_blecfg_start(void)
{
    BaseType_t ret;

    button_blecfg_show_wifi_emoji();

    if (s_task_running) {
        LISA_LOGI(TAG, "BLE config enter already in progress");
        return true;
    }

#if CONFIG_LISA_MODEM
    {
        sys_network_status_t net_status = {0};

        if (sys_network_get_status(&net_status) == 0 &&
            net_status.mode == SYS_NETWORK_MODE_MODEM) {
            if (net_status.connected) {
                LISA_LOGI(TAG, "4G online, show binding QR");
                button_blecfg_prompt_page(QR_STATUS_BIND);
                return false;
            }
            LISA_LOGI(TAG, "4G offline, switch to WiFi mode for BLE config");
            sys_network_request_mode(SYS_NETWORK_MODE_WIFI, true);
        }
    }
#endif

    s_task_running = true;
    s_user_entered = true;
    ret = xTaskCreate(button_blecfg_task,
                      "ble_netcfg",
                      BUTTON_BLECFG_TASK_STACK_SIZE,
                      NULL,
                      BUTTON_BLECFG_TASK_PRIORITY,
                      NULL);
    if (ret != pdPASS) {
        s_task_running = false;
        s_user_entered = false;
        LISA_LOGE(TAG, "Failed to create BLE config task");
        return false;
    }

    return true;
}

bool button_blecfg_is_user_entered(void)
{
    return s_user_entered;
}
