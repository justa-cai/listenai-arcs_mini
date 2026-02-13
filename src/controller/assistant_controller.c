#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "sysheap.h"
#include "alarm.h"
#include "listen_wifi.h"
#include "listen_volume.h"
#include "lisa_aiui.h"
#if (CONFIG_BATTERY_COLLECTION)
#include "battery.h"
#endif
#include "app_display.h"

#include "assistant_view.h"
#include "assistant_controller.h"
#include "lsc_stream_text.h"
#include "ebus/ebus.h"
#include "lisaui_user_data.h"
#include "led.h"
#include "app_client.h"
#include "cloud/app_cloud.h"
#include "tone.h"
#include "utils/evs_event.h"
#include "ota_manager.h"
#include "show_image.h"

#define TAG "controller"
#include "lisa_log.h"

#define FRONTEND_CTRL_QUEUE_NUMBER (3)
#define BACKEND_CTRL_QUEUE_NUMBER  (3)
#define BACKEND_TASK_PRIORITY      (6)
#define BACKEND_TASK_TACK_SIZE     (2048)
#define FRONTEND_TASK_PRIORITY     (6)
#define FRONTEND_TASK_TACK_SIZE    (1024)

typedef struct {
    bool is_tts_playing;
    bool view_idle_wait_for_tts_finish;
    bool is_sound_playing;
    bool view_idle_wait_for_sound_finish;
    int wifi_scan_retry_count;
} assist_controller_status_t;
typedef struct {
    assistant_view_t *view;
    assist_controller_status_t status;

} assist_controller_t;

typedef struct {
    assistant_controller_event_id_e index;
    uint32_t len;
    void *payload;
} ctr_event_message_t;

typedef struct {
    int (*process_sync)(void *arg, uint32_t len);
    int (*frontend_proc)(ctr_event_message_t *msg);
    int (*backend_proc)(ctr_event_message_t *msg);
} assistant_controller_event_handlers_t;

static QueueHandle_t frontend_ctrl_queue;
static QueueHandle_t backend_ctrl_queue;

static assist_controller_t *assist_controller = NULL;
static char s_ble_config_cnt = 0;

static void frontend_task(void *pvParameters);
static void backend_task(void *pvParameters);

static int ctrl_event_audio_idle_handler(void *arg, uint32_t len);
static int ctrl_event_audio_pre_idle_handler(void *arg, uint32_t len);
static int ctrl_event_audio_wake_handler(void *arg, uint32_t len);
static int ctrl_event_audio_connect_cloud_failed_handler(void *arg, uint32_t len);
static int ctrl_event_audio_start_handler(void *arg, uint32_t len);
static int ctrl_event_audio_vad_end_handler(void *arg, uint32_t len);
static int ctrl_event_session_end_handler(void *arg, uint32_t len);
static int ctrl_event_audio_play_start_handler(void *arg, uint32_t len);
static int ctrl_event_audio_play_stop_handler(void *arg, uint32_t len);
static int ctrl_event_audio_update_music_handler(void *arg, uint32_t len);
static int ctrl_event_cloud_update_iat_text_handler(ctr_event_message_t *msg);
static int ctrl_event_alarm_add_item_handler(void *arg, uint32_t len);
static int ctrl_event_alarm_delete_item_handler(void *arg, uint32_t len);
static int ctrl_event_weather_update_handler(void *arg, uint32_t len);
static int ctrl_event_tts_play_start_handler(void *arg, uint32_t len);
static int ctrl_event_tts_play_finish_handler(void *arg, uint32_t len);
static int ctrl_event_sound_play_start_handler(void *arg, uint32_t len);
static int ctrl_event_sound_play_finish_handler(void *arg, uint32_t len);
static int ctrl_event_cloud_get_roles_success_handler(void *arg, uint32_t len);
static int ctrl_event_cloud_get_roles_failed_handler(void *arg, uint32_t len);

static int ctrl_event_wifi_connected_handler(void *arg, uint32_t len);
static int ctrl_event_wifi_connecting_handler(void *arg, uint32_t len);
static int ctrl_event_wifi_disconnected_handler(void *arg, uint32_t len);
static int ctrl_event_wifi_scanning_handler(void *arg, uint32_t len);
static int ctrl_event_wifi_scanned_handler(void *arg, uint32_t len);
static int ctrl_event_role_emoji_update_handler(void *arg, uint32_t len);
static int ctrl_event_mcp_emoji_update_handler(void *arg, uint32_t len);
static int ctrl_event_reply_text_update_handler(void *arg, uint32_t len);
static int ctrl_event_standby_texts_update_handler(void *arg, uint32_t len);
static int ctrl_event_device_config_update_handler(void *arg, uint32_t len);
static int ctrl_event_outof_limit_error_handler(void *arg, uint32_t len);
static int ctrl_event_battery_info_update_handler(void *arg, uint32_t len);

static int ctrl_event_opt_wifi_connect_handler(ctr_event_message_t *msg);
static int ctrl_event_opt_wifi_disconnect_handler(ctr_event_message_t *msg);
static int ctrl_event_opt_wifi_scan_handler(ctr_event_message_t *msg);
static int ctrl_event_opt_toggle_info_page_handler(ctr_event_message_t *msg);
static int ctrl_event_opt_enter_ble_config_handler(ctr_event_message_t *msg);
static int ctrl_event_opt_exit_ble_config_handler(ctr_event_message_t *msg);
static int ctrl_event_opt_exit_info_page_handler(ctr_event_message_t *msg);

static int ctrl_event_ota_state_update_handler(void *arg, uint32_t len);

static int ctrl_event_wake_word_update_handler(void *arg, uint32_t len);

static assistant_controller_event_handlers_t s_ctrl_event_handlers[] = {
    {ctrl_event_audio_idle_handler, NULL, NULL},                 // CONTROLLER_EVENT_STATE_AUDIO_IDLE
    {ctrl_event_audio_pre_idle_handler, NULL, NULL},             // CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE
    {ctrl_event_audio_wake_handler, NULL, NULL},                 // CONTROLLER_EVENT_STATE_AUDIO_WAKEUP
    {ctrl_event_audio_connect_cloud_failed_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_AUDIO_CONNECT_CLOUD_FAILED
    {ctrl_event_audio_start_handler, NULL, NULL},                // CONTROLLER_EVENT_STATE_AUDIO_RECORD_START
    {NULL, NULL, NULL},                                          // CONTROLLER_EVENT_STATE_AUDIO_RECORD_STOP,
    {ctrl_event_audio_vad_end_handler, NULL, NULL},              // CONTROLLER_EVENT_STATE_VAD_END,
    {ctrl_event_session_end_handler, NULL, NULL},                // CONTROLLER_EVENT_STATE_SESSION_END,
    {ctrl_event_audio_play_start_handler, NULL, NULL},           // CONTROLLER_EVENT_STATE_AUDIO_PLAY_START,
    {ctrl_event_audio_play_stop_handler, NULL, NULL},            // CONTROLLER_EVENT_STATE_AUDIO_PLAY_STOP,
    {ctrl_event_audio_update_music_handler, NULL, NULL},         // CONTROLLER_EVENT_STATE_AUDIO_UPDATE_MUSIC
    {NULL, ctrl_event_cloud_update_iat_text_handler, NULL},      // CONTROLLER_EVENT_STATE_CLOUD_UPDATE_IAT_TEXT,
    {ctrl_event_alarm_add_item_handler, NULL, NULL},             // CONTROLLER_EVENT_STATE_ALARM_ADD_ITEM,
    {ctrl_event_alarm_delete_item_handler, NULL, NULL},          // CONTROLLER_EVENT_STATE_ALARM_DELETE_ITEM,
    {ctrl_event_weather_update_handler, NULL, NULL},             // CONTROLLER_EVENT_STATE_WEATHER_UPDATE,
    {ctrl_event_tts_play_start_handler, NULL, NULL},             // CONTROLLER_EVENT_STATE_TTS_PLAY_START,
    {ctrl_event_tts_play_finish_handler, NULL, NULL},            // CONTROLLER_EVENT_STATE_TTS_PLAY_FINISH
    {ctrl_event_sound_play_start_handler, NULL, NULL},           // CONTROLLER_EVENT_STATE_SOUND_PLAY_START,
    {ctrl_event_sound_play_finish_handler, NULL, NULL},          // CONTROLLER_EVENT_STATE_SOUND_PLAY_FINISH
    {ctrl_event_cloud_get_roles_success_handler, NULL, NULL},    // CONTROLLER_EVENT_STATE_CLOUD_GET_ROLES_SUCCESS
    {ctrl_event_cloud_get_roles_failed_handler, NULL, NULL},     // CONTROLLER_EVENT_STATE_CLOUD_GET_ROLES_FAILED

    {ctrl_event_wifi_connected_handler, NULL, NULL},    // CONTROLLER_EVENT_STATE_WIFI_CONNECTED,
    {ctrl_event_wifi_disconnected_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_WIFI_DISCONNECTED,
    {ctrl_event_wifi_connecting_handler, NULL, NULL},   // CONTROLLER_EVENT_STATE_WIFI_CONNECTING,
    {ctrl_event_wifi_scanning_handler, NULL, NULL},     // CONTROLLER_EVENT_STATE_WIFI_SCANNING,
    {ctrl_event_wifi_scanned_handler, NULL, NULL},      // CONTROLLER_EVENT_STATE_WIFI_SCANNED,
    {ctrl_event_role_emoji_update_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_ROLE_EMOJI_UPDATE,
    {ctrl_event_mcp_emoji_update_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_MCP_EMOJI_UPDATE,
    {ctrl_event_reply_text_update_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_REPLY_TEXT_UPDATE,
    {ctrl_event_standby_texts_update_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_STANDBY_TEXTS_UPDATE,
    {ctrl_event_device_config_update_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_DEVICE_CONFIG_UPDATE,
    {ctrl_event_outof_limit_error_handler, NULL, NULL}, // CONTROLLER_EVENT_STATE_OUTOF_LIMIT_ERROR,
    {ctrl_event_battery_info_update_handler, NULL, NULL}, // CONTROLLER_EVENT_BATTERY_INFO_UPDATE

    {NULL, ctrl_event_opt_wifi_connect_handler, NULL},    // CONTROLLER_EVENT_OPT_WIFI_CONNECT,
    {NULL, ctrl_event_opt_wifi_disconnect_handler, NULL}, // CONTROLLER_EVENT_OPT_WIFI_DISCONNECT,
    {NULL, ctrl_event_opt_wifi_scan_handler, NULL},       // CONTROLLER_EVENT_OPT_WIFI_SCAN,
    {NULL, ctrl_event_opt_toggle_info_page_handler, NULL}, // CONTROLLER_EVENT_OPT_TOGGLE_INFO_PAGE,
    {NULL, ctrl_event_opt_enter_ble_config_handler, NULL}, // CONTROLLER_EVENT_OPT_ENTER_BLE_CONFIG,
    {NULL, ctrl_event_opt_exit_ble_config_handler, NULL},  // CONTROLLER_EVENT_OPT_EXIT_BLE_CONFIG,

    {ctrl_event_ota_state_update_handler, NULL, NULL},  // CONTROLLER_EVENT_OTA_STATE_UPDATE,

    {ctrl_event_wake_word_update_handler, NULL, NULL}, // CONTROLLER_EVENT_WAKE_WORD_UPDATE,

};

static int view_cb_alarm_list_get(alarm_clock_list_t *list, int max_count);
static int view_cb_alarm_delete_by_timestamp(uint64_t timestamp);
static int view_cb_wifi_hotspot_connect(view_wifi_sta_cfg_t *sta_cfg);
static int view_cb_wifi_hotspot_disconnect(view_wifi_sta_cfg_t *sta_cfg);
static int view_cb_wifi_hotspot_scan(void);
static int view_cb_audio_spk_volume_set(int volume);
static int view_cb_audio_spk_volume_get(void);
static int view_cb_display_backlight_set(int backlight);
static int view_cb_display_backlight_get(void);
static int view_cb_cloud_interactive_mode_set(int mode);
static int view_cb_cloud_interactive_mode_get(void);
static int view_cb_battery_info_get(bool *is_charging, uint8_t *power_percent);
static int view_cb_wifi_sta_enable(void);
static int view_cb_wifi_sta_disable(void);
static int view_cb_wifi_rety_cnt_clear(void);

static assistant_view_cbs_t s_view_cbs = {
    .alarm_list_get = view_cb_alarm_list_get,
    .alarm_delete_by_timestamp = view_cb_alarm_delete_by_timestamp,
    .wifi_hotspot_connect = view_cb_wifi_hotspot_connect,
    .wifi_hotspot_disconnect = view_cb_wifi_hotspot_disconnect,
    .wifi_hotspot_scan = view_cb_wifi_hotspot_scan,
    .audio_spk_volume_set = view_cb_audio_spk_volume_set,
    .audio_spk_volume_get = view_cb_audio_spk_volume_get,
    .display_backlight_set = view_cb_display_backlight_set,
    .display_backlight_get = view_cb_display_backlight_get,
    .cloud_interactive_mode_set = view_cb_cloud_interactive_mode_set,
    .cloud_interactive_mode_get = view_cb_cloud_interactive_mode_get,
    .battery_info_get = view_cb_battery_info_get,
    .wifi_sta_enable = view_cb_wifi_sta_enable,
    .wifi_sta_disable = view_cb_wifi_sta_disable,
    .wifi_cnt_clear = view_cb_wifi_rety_cnt_clear,   
};

int assist_controller_init(void)
{
    assist_controller_t *controller;

    if (sizeof(s_ctrl_event_handlers) / sizeof(s_ctrl_event_handlers[0]) != CONTROLLER_EVENT_MAX_NUMBER) {
        LISA_LOGE(TAG, "The number of s_ctrl_event_handlers must be equal to CONTROLLER_EVENT_MAX_NUMBER.");
        return -ENOENT;
    }
    controller = (assist_controller_t *)exram_malloc(4, sizeof(assist_controller_t));
    if (!controller) {
        LISA_LOGE(TAG, "[%s %d]Failed to allocate memory for controller", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    memset(controller, 0, sizeof(assist_controller_t));
    controller->status.wifi_scan_retry_count = 0;
    controller->view = assistant_view_init(&s_view_cbs);
    if (!controller->view) {
        exram_free(controller);
        LISA_LOGE(TAG, "[%s %d]Failed to allocate memory for controller", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    frontend_ctrl_queue = xQueueCreate(FRONTEND_CTRL_QUEUE_NUMBER, sizeof(ctr_event_message_t));
    backend_ctrl_queue = xQueueCreate(BACKEND_CTRL_QUEUE_NUMBER, sizeof(ctr_event_message_t));

    xTaskCreate(frontend_task, "ctr_frontend_task", FRONTEND_TASK_TACK_SIZE, NULL, FRONTEND_TASK_PRIORITY, NULL);

    xTaskCreate(backend_task, "ctr_backend_task", BACKEND_TASK_TACK_SIZE, NULL, BACKEND_TASK_PRIORITY, NULL);

    /* View setup */
    if (controller->view->ops.setup) {
        controller->view->ops.setup();
    }

    assist_controller = controller;

    return 0;
}

int assist_controller_trigger_event(assistant_controller_event_id_e event_id, void *arg, uint32_t len)
{
    int ret = 0;

    if (event_id >= sizeof(s_ctrl_event_handlers) / sizeof(assistant_controller_event_handlers_t)) {
        LISA_LOGI(TAG, "[%s %d]invalid event id %d", __FUNCTION__, __LINE__, event_id);
        return -EINVAL;
    }

    if (s_ctrl_event_handlers[event_id].process_sync) {
        return s_ctrl_event_handlers[event_id].process_sync(arg, len);
    }

    if ((s_ctrl_event_handlers[event_id].frontend_proc != NULL) ||
        (s_ctrl_event_handlers[event_id].backend_proc != NULL)) {
        ctr_event_message_t msg;
        msg.index = event_id;
        msg.len = len;
        msg.payload = exram_malloc(sizeof(uint32_t), len);
        memcpy(msg.payload, arg, len);
        if (s_ctrl_event_handlers[event_id].frontend_proc) {
            ret = xQueueSend(frontend_ctrl_queue, &msg, portMAX_DELAY);
            if (ret != pdPASS) {
                CLOGE(TAG, "Send message frontend failed,ret:%d", ret);
            }
        }

        if (s_ctrl_event_handlers[event_id].backend_proc) {
            ret = xQueueSend(backend_ctrl_queue, &msg, portMAX_DELAY);
            if (ret != pdPASS) {
                CLOGE(TAG, "Send message backend failed,ret:%d", ret);
            }
        }
    }

    return ret;
}

static void frontend_task(void *pvParameters)
{
    int ret;
    ctr_event_message_t msg;

    CLOGI("Controller frontend task started");
    while (1) {
        ret = xQueueReceive(frontend_ctrl_queue, &msg, portMAX_DELAY);
        if (pdPASS != ret) {
            CLOGI("Receive controller frontend queue failed,ret:%d", ret);
            continue;
        }

        if ((msg.index < CONTROLLER_EVENT_MAX_NUMBER) && (s_ctrl_event_handlers[msg.index].frontend_proc != NULL)) {
            ret = s_ctrl_event_handlers[msg.index].frontend_proc(&msg);
            if (ret != 0) {
                CLOGI("Process frontend index %d failed,ret:%d", msg.index, ret);
            }
        }

        if ((msg.payload != NULL) && (msg.len > 0)) {
            exram_free(msg.payload);
        }
    }
}

static void backend_task(void *pvParameters)
{

    int ret;
    ctr_event_message_t msg;

    CLOGI("Controller backend task started");
    while (1) {
        ret = xQueueReceive(backend_ctrl_queue, &msg, portMAX_DELAY);
        if (pdPASS != ret) {
            CLOGI("Receive controller backend queue failed,ret:%d", ret);
            continue;
        }

        if ((msg.index < CONTROLLER_EVENT_MAX_NUMBER) && (s_ctrl_event_handlers[msg.index].backend_proc != NULL)) {
            ret = s_ctrl_event_handlers[msg.index].backend_proc(&msg);
            if (ret != 0) {
                CLOGI("Process backend index %d failed,ret:%d", msg.index, ret);
            }
        }

        if ((msg.payload != NULL) && (msg.len > 0)) {
            exram_free(msg.payload);
        }
    }
}

static int ctrl_event_audio_idle_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_IDLE);
    }

    return 0;
}

/*wait for tts play finish*/
static int ctrl_event_audio_pre_idle_handler(void *arg, uint32_t len)
{
    bool is_enter_idle = false;

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->status.is_tts_playing == true) {
        assist_controller->status.view_idle_wait_for_tts_finish = true;
    } else if (assist_controller->status.is_sound_playing == true) {
        assist_controller->status.view_idle_wait_for_sound_finish = true;
    } else {
        if (assist_controller->view->ops.update_event != NULL) {
            assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_IDLE);
            is_enter_idle = true;
        }
    }

    if (!is_enter_idle) {
        if (assist_controller->view->ops.update_event != NULL) {
            assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_INTER_IDLE);
        }
    }

    return 0;
}

static int ctrl_event_audio_wake_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    
    // 取消文生图等待（重新唤醒表示用户有新的交互）
    show_image_cancel_waiting();
    
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_WAKEUP);
    }
    assist_controller->status.is_sound_playing = false;
    assist_controller->status.is_tts_playing = false;
    assist_controller->status.view_idle_wait_for_tts_finish = false;
    assist_controller->status.view_idle_wait_for_sound_finish = false;

    return 0;
}

static int ctrl_event_audio_connect_cloud_failed_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_CONNECT_CLOUD_FAILED);
    }

    return 0;
}

static int ctrl_event_audio_start_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_RECORD_START);
    }

    return 0;
}

static int ctrl_event_audio_vad_end_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_TRIGGERED_VAD);
    }

    return 0;
}

static int ctrl_event_session_end_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_SESSION_END);
    }

    return 0;
}

static int ctrl_event_audio_play_start_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_PLAY_START);
    }

    return 0;
}

static int ctrl_event_audio_play_stop_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_PLAY_STOP);
    }

    return 0;
}

static int ctrl_event_audio_update_music_handler(void *arg, uint32_t len)
{
    assistant_controller_event_update_music_payload_t *payload =
        (assistant_controller_event_update_music_payload_t *)arg;

    LISA_LOGI(TAG, "%s, name:%s, artist:%s", __FUNCTION__, payload->name, payload->artist);
    if (assist_controller->view->ops.update_music != NULL) {
        assist_controller->view->ops.update_music(payload->name, payload->artist);
    }
    return 0;
}

static int ctrl_event_cloud_update_iat_text_handler(ctr_event_message_t *msg)
{

    LISA_LOGI(TAG, "[%s] %s", __FUNCTION__, (char *)msg->payload);
    if (assist_controller->view->ops.update_iat_append_text != NULL) {
        assist_controller->view->ops.update_iat_append_text(true, (char *)msg->payload);
    }

    return 0;
}

static int ctrl_event_alarm_add_item_handler(void *arg, uint32_t len)
{
    alarm_clock_t alarm;

    LISA_LOGI(TAG, "[%s] %lld", __FUNCTION__, *(uint64_t *)arg);
    if (assist_controller->view->ops.update_alarm != NULL) {

        alarm.index = 0;
        alarm.is_enable = true;
        alarm.is_repeat_per_day = false;
        alarm.timestamp = *(uint64_t *)arg;
        assist_controller->view->ops.update_alarm(&alarm, VIEW_ALARM_OPT_ADD);
    }

    return 0;
}

static int ctrl_event_alarm_delete_item_handler(void *arg, uint32_t len)
{
    alarm_clock_t alarm;

    LISA_LOGI(TAG, "[%s] %lld", __FUNCTION__, *(uint64_t *)arg);
    if (assist_controller->view->ops.update_alarm != NULL) {

        alarm.index = 0;
        alarm.is_enable = true;
        alarm.is_repeat_per_day = false;
        alarm.timestamp = *(uint64_t *)arg;
        assist_controller->view->ops.update_alarm(&alarm, VIEW_ALARM_OPT_DELETE);
    }

    return 0;
}

static int ctrl_event_weather_update_handler(void *arg, uint32_t len)
{
    if (assist_controller->view->ops.update_weather != NULL) {
        assist_controller->view->ops.update_weather((char *)arg);
    }
    return 0;
}

static int ctrl_event_tts_play_start_handler(void *arg, uint32_t len)
{
    LISA_LOGI(TAG, "%s", __FUNCTION__);

    assist_controller->status.is_tts_playing = true;

    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_TTS_PLAY_START);
    }
}

static int ctrl_event_tts_play_finish_handler(void *arg, uint32_t len)
{

    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_TTS_PLAY_STOP);
    }
    if (true == assist_controller->status.view_idle_wait_for_tts_finish) {
        if (assist_controller->view->ops.update_event != NULL) {
            assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_IDLE);
        }
    }
    assist_controller->status.view_idle_wait_for_tts_finish = false;
    assist_controller->status.is_tts_playing = false;
    return 0;
}

static int ctrl_event_sound_play_start_handler(void *arg, uint32_t len)
{
    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (!assist_controller) {
        return -1;
    }
    assist_controller->status.is_sound_playing = true;
}

static int ctrl_event_sound_play_finish_handler(void *arg, uint32_t len)
{
    if (!assist_controller) {
        return -1;
    }
    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (true == assist_controller->status.view_idle_wait_for_sound_finish) {
        if (assist_controller->view->ops.update_event != NULL) {
            assist_controller->view->ops.update_event(VIEW_EVENT_AUDIO_IDLE);
        }
    }
    assist_controller->status.view_idle_wait_for_sound_finish = false;
    assist_controller->status.is_sound_playing = false;
}

static int ctrl_event_cloud_get_roles_success_handler(void *arg, uint32_t len)
{
    if (!assist_controller) {
        return -1;
    }
    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_GET_ROLES_SUCCESS);
    }
}

static int ctrl_event_cloud_get_roles_failed_handler(void *arg, uint32_t len)
{
    if (!assist_controller) {
        return -1;
    }
    LISA_LOGI(TAG, "%s", __FUNCTION__);
    if (assist_controller->view->ops.update_event != NULL) {
        assist_controller->view->ops.update_event(VIEW_EVENT_GET_ROLES_FAILED);
    }
}

static int ctrl_event_wifi_connected_handler(void *arg, uint32_t len)
{
    view_wifi_info_t *info = (view_wifi_info_t *)arg;

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_wifi_state) {
        assist_controller->view->ops.update_wifi_state(info);
    }
    if (assist_controller) {
        assist_controller->status.wifi_scan_retry_count = 0;
    }
    s_ble_config_cnt = 0;

    return 0;
}

static int ctrl_event_wifi_connecting_handler(void *arg, uint32_t len)
{
    view_wifi_info_t *info = (view_wifi_info_t *)arg;

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_wifi_state) {
        assist_controller->view->ops.update_wifi_state(info);
    }

    return 0;
}

static int ctrl_event_wifi_disconnected_handler(void *arg, uint32_t len)
{
    view_wifi_info_t *info = (view_wifi_info_t *)arg;

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_wifi_state) {
        assist_controller->view->ops.update_wifi_state(info);
    }

    return 0;
}

static int ctrl_event_wifi_scanning_handler(void *arg, uint32_t len)
{
    view_wifi_info_t *info = (view_wifi_info_t *)arg;

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_wifi_state) {
        assist_controller->view->ops.update_wifi_state(info);
    }

    return 0;
}

static int ctrl_event_wifi_scanned_handler(void *arg, uint32_t len)
{
    view_wifi_info_t *info = (view_wifi_info_t *)arg;

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_wifi_state) {
        assist_controller->view->ops.update_wifi_state(info);
    }

    // 增加扫描重试计数器
    assist_controller->status.wifi_scan_retry_count++;
    LISA_LOGI(TAG, "WiFi rety scan count: %d", assist_controller->status.wifi_scan_retry_count);

    // 如果重试5次仍未成功，显示信息页面
    if (assist_controller->status.wifi_scan_retry_count >= 5) {
        assist_controller->status.wifi_scan_retry_count = 0;
            
            // 触发信息页面显示 - 发送页面切换事件
            if (assist_controller && assist_controller->view) {
                // 发布页面切换事件
                change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK);
                if (s_ble_config_cnt < 2) {
                    s_ble_config_cnt++;
                    enter_ble_config(true);
                }     
        }
    }


    return 0;
}

static int ctrl_event_role_emoji_update_handler(void *arg, uint32_t len)
{
    LISA_LOGI(TAG, "[%s] Received emoji update event, arg: %p, len: %u", __func__, arg, len);
    
    if (arg) {
        LISA_LOGI(TAG, "Emoji ID: %s", (const char *)arg);
    } else {
        LISA_LOGE(TAG, "Error: emoji ID is NULL");
        return -EINVAL;
    }

    if (!assist_controller) {
        LISA_LOGE(TAG, "Error: assist_controller is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view) {
        LISA_LOGE(TAG, "Error: assist_controller->view is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view->ops.update_role_emoji) {
        LISA_LOGE(TAG, "Error: update_role_emoji callback is not set");
        return -ENOTSUP;
    }
    
    LISA_LOGI(TAG, "Calling update_role_emoji with: %s", (const char *)arg);
    assist_controller->view->ops.update_role_emoji((const char *)arg);
    LISA_LOGI(TAG, "update_role_emoji called successfully");

    return 0;
}

static int ctrl_event_mcp_emoji_update_handler(void *arg, uint32_t len)
{
    LISA_LOGI(TAG, "[%s] Received MCP emoji update event, arg: %p, len: %u", __func__, arg, len);
    
    if (arg) {
        LISA_LOGI(TAG, "MCP Emoji ID: %s", (const char *)arg);
    } else {
        LISA_LOGE(TAG, "Error: MCP emoji ID is NULL");
        return -EINVAL;
    }

    if (!assist_controller) {
        LISA_LOGE(TAG, "Error: assist_controller is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view) {
        LISA_LOGE(TAG, "Error: assist_controller->view is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view->ops.update_mcp_emoji) {
        LISA_LOGE(TAG, "Error: update_mcp_emoji callback is not set");
        return -ENOTSUP;
    }
    
    LISA_LOGI(TAG, "Calling update_mcp_emoji with: %s", (const char *)arg);
    assist_controller->view->ops.update_mcp_emoji((const char *)arg);
    LISA_LOGI(TAG, "update_mcp_emoji called successfully");

    return 0;
}

static int ctrl_event_battery_info_update_handler(void *arg, uint32_t len)
{

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_battery_status) {
        assist_controller->view->ops.update_battery_status((view_battery_info_t *)arg);
    }

    return 0;
}

static void text_request_cb(int evt, const char *data, void *user)
{
    static bool is_done = true;
    // LISA_LOGI(TAG, "[%s] evt=%d, datas:%s", __FUNCTION__, evt, (char *)data);

    if (!assist_controller || !assist_controller->view || !assist_controller->view->ops.update_reply_text) {
        LISA_LOGE(TAG, "update_reply_text is NULL");
        return;
    }

    if (evt == SSE_EVT_DATA) {
        if (strlen(data) == 0) {
            return;
        }

        if (is_done) {
            is_done = false;
            assist_controller->view->ops.update_reply_text(data, LISAUI_USERDATA_TEXT_MODE_OVERWRITE);
        } else {
            assist_controller->view->ops.update_reply_text(data, LISAUI_USERDATA_TEXT_MODE_APPEND);
        }
    } else if (evt == SSE_EVT_DONE || evt == SSE_EVT_ABORT) {
        is_done = true;
        if (data != NULL && strlen(data) > 0) {
            assist_controller->view->ops.update_reply_text(data, LISAUI_USERDATA_TEXT_MODE_APPEND);
        }
    }
}

static int ctrl_event_reply_text_update_handler(void *arg, uint32_t len)
{
    struct lsc_stream_text_request_ctx *text_req_ctx = NULL;

    // LISA_LOGI(TAG, "get reply text url %s", (char *)arg);
    text_req_ctx = lsc_stream_text_request_new(arg, text_request_cb, NULL);
    if (text_req_ctx) {
        int err = lsc_stream_text_request_start(text_req_ctx, 10);
        if (err) {
            LISA_LOGE(TAG, "lsc_stream_text_request failed, err:%d", err);
        }
        lsc_stream_text_request_delete(text_req_ctx);
        text_req_ctx = NULL;
    } else {
        LISA_LOGE(TAG, "lsc_stream_text_request_ctx new failed");
    }
    return 0;
}

int assist_controller_set_mcp_emoji(const char *emoji_name)
{
    if (!emoji_name) {
        LISA_LOGE(TAG, "MCP emoji name is NULL");
        return -EINVAL;
    }
    
    LISA_LOGI(TAG, "Setting MCP emoji: %s", emoji_name);
    
    // 通过MCP表情事件触发表情更新
    return assist_controller_trigger_event(CONTROLLER_EVENT_STATE_MCP_EMOJI_UPDATE, 
                                          (void*)emoji_name, 
                                          strlen(emoji_name) + 1);
}

static int ctrl_event_opt_wifi_connect_handler(ctr_event_message_t *msg)
{
    view_wifi_sta_cfg_t *view_sta_cfg = (view_wifi_sta_cfg_t *)msg->payload;
    wifi_mgr_sta_config_t cfg;
    int ret;

    LISA_LOGI(TAG, "[%s] ssid:%s,pwd:%c%c***", __FUNCTION__, view_sta_cfg->ssid, view_sta_cfg->pwd[0],
              view_sta_cfg->pwd[1]);
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", view_sta_cfg->ssid);
    snprintf(cfg.pwd, sizeof(cfg.pwd), "%s", view_sta_cfg->pwd);

    ret = wifi_mgr_sta_connect(&cfg, false);

    return ret;
}

static int ctrl_event_opt_wifi_disconnect_handler(ctr_event_message_t *msg)
{
    int ret;

    ret = wifi_mgr_sta_disconnect(false);

    return ret;
}

static int ctrl_event_opt_wifi_scan_handler(ctr_event_message_t *msg)
{

    (void *)msg;
    view_wifi_info_t info = {
        .is_enable = true,
        .state = VIEW_WIFI_STATE_SCANNING,
        .hotspot_list =
            {
                .count = 0,
            },
    };
    /*Update scanning state*/
    info.is_enable = wifi_mgr_sta_is_enable();
    assist_controller->view->ops.update_wifi_state(&info);

    /*Start scan,result by callback*/
    wifi_mgr_scan_info_t *ap_info;
    ap_info = exram_malloc(4, sizeof(wifi_mgr_scan_info_t) * WIFI_HOTSPOT_LIST_MAX_NUMBER);
    wifi_mgr_scan_ap(ap_info, WIFI_HOTSPOT_LIST_MAX_NUMBER, false);
    exram_free(ap_info);

    return 0;
}

static int view_cb_alarm_list_get(alarm_clock_list_t *list, int max_count)
{
    struct ls_alarm *alarm;
    struct ls_alarm *el;
    int count = 0;

    alarm = ls_alarm_get();

    for (el = alarm; el != NULL; el = el->next) {
        list->alarms[count].index = count;
        list->alarms[count].is_enable = false;
        list->alarms[count].is_repeat_per_day = false;
        list->alarms[count].timestamp = el->timestamp;

        if (++count >= max_count) {
            break;
        }
    }
    list->count = count;
    LISA_LOGI(TAG, "%s count:%d", __FUNCTION__, count);

    return count;
}

static int view_cb_alarm_delete_by_timestamp(uint64_t timestamp)
{
    LISA_LOGI(TAG, "View alarm delete by timestamp:%lld", timestamp);
    ls_alarm_delete_by_timestamp(timestamp);

    return 0;
}

// static int view_cb_wifi_hotspot_list_get(view_wifi_hotspot_list_t *list, int max_count)
// {
//     wifi_hotspot_list_t *h_list;
//     int num;

//     if ((list == NULL) || (max_count == 0)) {
//         return -EINVAL;
//     }

//     h_list = ls_wifi_get_hotspot_list();
//     if (h_list == NULL) {
//         return -EIO;
//     }

//     for (num = 0; (num < h_list->number) && (num < max_count); num++) {
//         snprintf(list->hotspots[num].ssid, sizeof(list->hotspots[num].ssid), "%s", h_list->hotspot[num].info.ssid);
//         snprintf(list->hotspots[num].bssid, sizeof(list->hotspots[num].bssid), "%s",
//         h_list->hotspot[num].info.bssid); memset(list->hotspots[num].pwd,0,sizeof(list->hotspots[num].pwd));
//         list->hotspots[num].rssi = h_list->hotspot[num].info.rssi;
//         list->hotspots[num].channel = h_list->hotspot[num].info.channel;
//         snprintf(list->hotspots[num].encryption_mode_str, sizeof(list->hotspots[num].encryption_mode_str), "%s",
//                  wifi_encryption_mode_convert_str(h_list->hotspot[num].info.encryption_mode));
//         if(h_list->hotspot[num].state == LS_WIFI_HOTSPOT_STATE_CONNECTING){
//             list->hotspots[num].state = VIEW_WIFI_STATE_CONNECTING;
//         }
//         else if(h_list->hotspot[num].state == LS_WIFI_HOTSPOT_STATE_CONNECTED){
//             list->hotspots[num].state = VIEW_WIFI_STATE_CONNECTED;
//         }
//         else{
//             list->hotspots[num].state = VIEW_WIFI_STATE_DISCONNECTED;
//         }
//     }

//     list->count = num;

//     return 0;
// }

static int view_cb_wifi_hotspot_connect(view_wifi_sta_cfg_t *sta_cfg)
{
    assist_controller_trigger_event(CONTROLLER_EVENT_OPT_WIFI_CONNECT, sta_cfg, sizeof(view_wifi_sta_cfg_t));

    return 0;
}

static int view_cb_wifi_hotspot_disconnect(view_wifi_sta_cfg_t *sta_cfg)
{
    assist_controller_trigger_event(CONTROLLER_EVENT_OPT_WIFI_DISCONNECT, sta_cfg, sizeof(view_wifi_sta_cfg_t));

    return 0;
}

static int view_cb_wifi_hotspot_scan(void)
{
    assist_controller_trigger_event(CONTROLLER_EVENT_OPT_WIFI_SCAN, NULL, 0);
    return 0;
}

static int view_cb_audio_spk_volume_set(int volume)
{
    LISA_LOGI(TAG, "Set spk volume:%d", volume);
    listen_set_volume(volume);

    return 0;
}

static int view_cb_audio_spk_volume_get(void)
{
    int vol;

    vol = listen_get_volume();
    LISA_LOG(TAG, "Get spk volume:%d", vol);

    return vol;
}

static int view_cb_display_backlight_set(int backlight)
{

    LISA_LOGI(TAG, "Set display backlight:%d%%", backlight);
    if (backlight > 100) {
        backlight = 100;
    }
    app_display_set_brightness(backlight);

    return 0;
}

static int view_cb_display_backlight_get(void)
{
    int backlight;

    backlight = app_display_get_brightness();
    LISA_LOGI(TAG, "Get display backlight:%d%%", backlight);

    return backlight;
}

static int view_cb_cloud_interactive_mode_set(int mode)
{
    LISA_LOG(TAG, "Set cloud interactive mode:%d", mode);
    if (mode == 0) {
        lisa_aiui_set_interactive_mode(INTER_ONESHOT);
    } else if (mode == 1) {
        lisa_aiui_set_interactive_mode(INTER_CONTINUE);
    } else if (mode == 2) {
        lisa_aiui_set_interactive_mode(INTER_BUTTON);
    } else {
        LISA_LOGE(TAG, "%s unknown mode:%d", mode);
    }

    return 0;
}

static int view_cb_cloud_interactive_mode_get(void)
{
    int mode;

    mode = lisa_aiui_get_interactive_mode();
    LISA_LOG(TAG, "Get cloud interactive mode:%d", mode);

    return mode;
}

static int view_cb_battery_info_get(bool *is_charging, uint8_t *power_percent)
{
    *is_charging = false;
    *power_percent = 100;

    return 0;
}

static int view_cb_wifi_sta_enable(void)
{
    LISA_LOG(TAG, "%s", __FUNCTION__);
    return wifi_mgr_sta_enable();
}

static int view_cb_wifi_sta_disable(void)
{
    LISA_LOG(TAG, "%s", __FUNCTION__);
    wifi_mgr_sta_disable();
    view_wifi_info_t info = {0};

    /*Update wifi dev state*/
    info.is_enable = wifi_mgr_sta_is_enable();
    assist_controller->view->ops.update_wifi_state(&info);
}
static int view_cb_wifi_rety_cnt_clear(void)
{
    if (assist_controller) {
        assist_controller->status.wifi_scan_retry_count = 0;
    }
    
    return 0;
}


static int ctrl_event_opt_toggle_info_page_handler(ctr_event_message_t *msg)
{
    LISA_LOGI(TAG, "Toggle info page event received");
    app_client_t *client = app_client_get_instance();

    if (!app_cloud_is_connected()) {
        /* server is not connected, play tone */
        LISA_LOGW(TAG, "server is not connected");
        
        if (client && client->sound_player) {
            listen_soundplayer_play(client->sound_player, TONE_ID_64, 0);
        }
        return 0;
    }

    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);

    if (client && client->sound_player) {
        listen_soundplayer_play(client->sound_player, TONE_ID_104, 0);
    }
    
    return 0;
}

static int ctrl_event_standby_texts_update_handler(void *arg, uint32_t len)
{
    if (arg == NULL || len == 0) {
        LISA_LOGE(TAG, "Invalid standby texts data");
        return -1;
    }

    LISA_LOGI(TAG, "Standby texts update: %s", (char *)arg);
    
    if (!assist_controller) {
        LISA_LOGE(TAG, "Error: assist_controller is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view) {
        LISA_LOGE(TAG, "Error: assist_controller->view is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view->ops.update_standby_texts) {
        LISA_LOGE(TAG, "Error: update_standby_texts callback is not set");
        return -ENOTSUP;
    }

    assist_controller->view->ops.update_standby_texts((const char *)arg);
    LISA_LOGI(TAG, "update_standby_texts called successfully");

    return 0;
}

static int ctrl_event_device_config_update_handler(void *arg, uint32_t len)
{
    if (arg == NULL || len == 0) {
        LISA_LOGE(TAG, "Invalid device config data");
        return -1;
    }

    LISA_LOGI(TAG, "Device config update: %s", (char *)arg);
    
    if (!assist_controller) {
        LISA_LOGE(TAG, "Error: assist_controller is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view) {
        LISA_LOGE(TAG, "Error: assist_controller->view is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view->ops.update_device_config) {
        LISA_LOGE(TAG, "Error: update_device_config callback is not set");
        return -ENOTSUP;
    }

    assist_controller->view->ops.update_device_config((const char *)arg);
    LISA_LOGI(TAG, "update_device_config called successfully");

    return 0;
}

static int ctrl_event_outof_limit_error_handler(void *arg, uint32_t len)
{
    if (arg == NULL || len == 0) {
        LISA_LOGE(TAG, "Invalid OutOfLimit error data");
        return -1;
    }

    LISA_LOGI(TAG, "OutOfLimit error received: %s", (char *)arg);
    
    if (!assist_controller) {
        LISA_LOGE(TAG, "Error: assist_controller is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view) {
        LISA_LOGE(TAG, "Error: assist_controller->view is NULL");
        return -EINVAL;
    }
    
    if (!assist_controller->view->ops.update_outof_limit_error) {
        LISA_LOGE(TAG, "Error: update_outof_limit_error callback is not set");
        return -ENOTSUP;
    }

    assist_controller->view->ops.update_outof_limit_error((const char *)arg);
    LISA_LOGI(TAG, "update_outof_limit_error called successfully");

    return 0;
}

static int ctrl_event_opt_enter_ble_config_handler(ctr_event_message_t *msg)
{
    LISA_LOGI(TAG, "Enter BLE config event received");

    extern int play_start_config_net_audio(void);
    play_start_config_net_audio();
    app_led_blink(200, 200);
    return 0;
}

static int ctrl_event_opt_exit_ble_config_handler(ctr_event_message_t *msg)
{
    LISA_LOGI(TAG, "Exit BLE config event received");
    
    extern int play_config_net_success_audio(void);
    play_config_net_success_audio();
    app_led_on();
    return 0;
}

static int ctrl_event_ota_state_update_handler(void *arg, uint32_t len)
{
    ota_state_t *state = (ota_state_t *)arg;

    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_ota_state) {
        assist_controller->view->ops.update_ota_state(state);
    }

    return 0;
}

static int ctrl_event_wake_word_update_handler(void *arg, uint32_t len)
{
    if (assist_controller && assist_controller->view && assist_controller->view->ops.update_wake_word) {
        assist_controller->view->ops.update_wake_word((const char *)arg);
    }

    return 0;
}

