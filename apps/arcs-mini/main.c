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
#include "wifi_manager/wifi_manager.h"
#include "app_datas.h"
#include "tone.h"
#include "app_player.h"
#include "pa_manager.h"
#include "voice_player_comm.h"
#include "power/power_manager.h"
#include "lisa_display.h"
#include "battery/battery.h"
#include "lisa_ui_nav_scr.h"
#include "voice_cloud.h"

extern uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);

void network_reset(void)
{
    LOGI("network reset running...");
#if CONFIG_WIFI_MANAGER
    wifi_mgr_sta_disconnect(true);
    wifi_mgr_sta_config_t list[16] = {0};
    int count = 0;
    do {
        count = wifi_mgr_storage_search_ap(list, (int)(sizeof(list) / sizeof(list[0])), SEARCH_ALL, NULL);
        if (count > 0) {
            int del_count = count;
            if (del_count > (int)(sizeof(list) / sizeof(list[0]))) {
                del_count = (int)(sizeof(list) / sizeof(list[0]));
            }
            for (int i = 0; i < del_count; i++) {
                wifi_mgr_storage_delete_ap(&list[i]);
            }
        }
    } while (count > (int)(sizeof(list) / sizeof(list[0])));
#endif
}


void factory_reset(void)
{
    LOGI("factory reset running...");
    lisa_kv_clear();
    network_reset();
    LOGI("factory reset dwone.");
    LOGI("The system will restart after 300ms.");
    app_player_play(tone_player, app_tone_get_url(TONE_ID_103));
    vTaskDelay(2000);
    extern void sys_platform_sw_full_reset(void);
    sys_platform_sw_full_reset();
}

static void voice_system_network_probe_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    // 这里可以初始化需要网络的服务
}

static void voice_cloud_auth_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    service_alarm_init();
}

static void button_changed(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    voice_msg_button_evt_t *evt = (voice_msg_button_evt_t *)data;

    service_image_waiting_cancel();

    if (evt->button_id != 0) {
        return;
    }

    switch (evt->action) {
        case VOICE_MSG_BUTTON_ACTION_CLICK:
        {
            /* 单击：响铃时稍后提醒；非主页先回主页；主页则触发按键唤醒 */
            if (alarm_ring_is_active()) {
                LISA_LOGI(TAG, "Single click: alarm ringing, handle snooze");
                alarm_handle_snooze();
                alarm_ring_stop();
                break;
            }

            if (lisa_ui_nav_scr_get_top_id() != 0) {
                LISA_LOGI(TAG, "Single click: not on home page, navigating home");
                lisa_ui_nav_scr_nav_to(0);
                break;
            } else {
                LISA_LOGI(TAG, "Single click: wakeup trigger");
                if (model_voice_tts_is_playing()) {
                    LISA_LOGI(TAG, "Single click: TTS playing, stop it");
                    app_player_stop(tts_player);
                    break;
                }
                if (model_voice_cloud_is_running()) {
                    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
                } else {
                    const char keyword[] = "xiao ling xiao ling";
                    voice_msg_pub(VOICE_MSG_WAKEUP_KEYWORD, (void *)keyword, sizeof(keyword));
                }
            }
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
        {
            LISA_LOGI(TAG, "power button double click, image recognition");
            voice_msg_pub(VOICE_MSG_BUTTON_IMAGE_RECOGNITION, NULL, 0);
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
            /* 四击 */
            break;
        }
        case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
        case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
        case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
        {
            struct app_datas *app_datas = get_app_datas();
            /* 连击 >=5：进入 BLE 配网并打开信息页 */
            LISA_LOGI(TAG, "power button multi click, enter BLE config");
            service_alarm_deinit();
            voice_cloud_disconnect();
            if (app_datas != NULL && !app_datas->wifi_connected) {
                app_player_play(tone_player, app_tone_get_url(TONE_ID_70));
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            network_reset();
            app_ble_adv_start(0, 1);
            voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, NULL, 0);
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
            /* 长按：响铃时停止并生成下一个；否则拍照识图 */
            if (alarm_ring_is_active()) {
                LISA_LOGI(TAG, "Double click: alarm ringing, stop and generate next");
                alarm_ring_stop();
                alarm_handle_stop_and_next();
                break;
            }
            
            /* 长按：关机（USB 供电时忽略）*/
            if (power_is_usb_plugged()) {
                LISA_LOGI(TAG, "USB connected, ignore long press shutdown");
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

    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_system_network_probe_success, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS , voice_cloud_auth_success, NULL);
    voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, button_changed, NULL);

    service_led_init();
    service_volume_init();
    service_brightness_init();
    service_button_init();
    service_image_init();
    #if !CONFIG_4G_MODULE
    service_camera_init();
    #endif


#if CONFIG_APPLICATION_UI
    extern int lisa_ui_init(void);
    lisa_ui_init();
#endif

    battery_init();

#if CONFIG_WIFI_MANAGER
    wifi_mgr_sta_config_t list[8] = {0};
    int count = wifi_mgr_storage_search_ap(list, (int)(sizeof(list) / sizeof(list[0])), SEARCH_ALL, NULL);
    if (count == 0) {
        voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, NULL, 0);
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
