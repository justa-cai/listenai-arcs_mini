
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sys/time.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "listen_mic_gain.h"
#include "listen_volume.h"

#include "sysheap.h"
#include "workqueue.h"

#include "view_runner.h"
#include "view_events.h"
#include "lisaui_user_data.h"
#include "assistant_view.h"

#include "lisa_log.h"
// #include "ui.h"

static const char *TAG = "assistant_view";

workqueue_t *view_workq = NULL;
TimerHandle_t view_timer = NULL;

#define WEATHER_JSON_MAX_LEN          (4096)
#define MUSIC_TITLE_MAX_LEN           (128)
#define MUSIC_ARTIST_MAX_LEN          (128)
#define ALARM_DATE_TEXT_MAX_LEN       (16)
#define ALARM_TIME_TEXT_MAX_LEN       (16)
#define SKILL_PAGE_MIN_SHOW_DELAY_MS  (1000)
#define VIEW_GET_WIFI_LIST_MAX_NUMBER (128)

#define VIEW_WORKQ_THREAD_PRIORITY   2
#define VIEW_WORKQ_THREAD_STACK_SIZE 4096
#define VIEW_WORKQ_THREAD_NAME       "view_workq"
#define VIEW_WORKQ_QUEUE_LENGTH      64

#define EBUS_MESSAGE_PUB_BY_WORK_DEFINE(event)                                                                         \
    static void update_##event##_work(void *param)                                                                     \
    {                                                                                                                  \
        ebus_message_pub(view_handler->view->ebus_info.base_event_chn, event, NULL, 0);                                \
    }
#define EBUS_MESSAGE_PUB_BY_WORK_DECLARE(event) update_##event##_work

typedef struct {
    char weather_json[WEATHER_JSON_MAX_LEN];
    // lisaui_app_audio_player_state_t music_state;
    // lisaui_app_audio_player_state_t tts_state;
    char music_title[MUSIC_TITLE_MAX_LEN];
    char music_artist[MUSIC_ARTIST_MAX_LEN];
    char alarm_date_text[ALARM_DATE_TEXT_MAX_LEN];
    char alarm_time_text[ALARM_TIME_TEXT_MAX_LEN];
} view_page_info_t;

typedef struct {
    assistant_view_t *view;
    // lisaui_app_standby_emoji_type_e standby_emoji;
    view_page_info_t page_info;

} view_handler_t;

#define DEBUG_USER_DATA_WIFI_INFO()                                                                                    \
    do {                                                                                                               \
        LISA_LOGI(TAG, "+-----------------------------------------+");                                                 \
        LISA_LOGI(TAG, "|          WiFi USER DATA INFO           |");                                                  \
        LISA_LOGI(TAG, "+-----------------------------------------+");                                                 \
        LISA_LOGI(TAG, "| WiFi Enabled:    %-20s |", _userdata->setting.wifi.is_enable ? "YES" : "NO");                \
        LISA_LOGI(TAG, "| Connect State:   %-20d |", _userdata->setting.wifi.connect_info.state);                      \
        LISA_LOGI(TAG, "| Configured SSID: %-20s |", _userdata->setting.wifi.connect_info.cfg.ssid);                   \
        if (_userdata->setting.wifi.p_hotspot_info != NULL) {                                                          \
            LISA_LOGI(TAG, "| Scan State:      %-20d |", _userdata->setting.wifi.p_hotspot_info->state);               \
            LISA_LOGI(TAG, "| Hotspot Count:   %-20d |", _userdata->setting.wifi.p_hotspot_info->number);              \
            LISA_LOGI(TAG, "+-----------------------------------------+");                                             \
            LISA_LOGI(TAG, "|    ID |            SSID             | RSSI  | State |");                                 \
            LISA_LOGI(TAG, "+-----------------------------------------+");                                             \
            for (int i = 0; i < _userdata->setting.wifi.p_hotspot_info->number; i++) {                                 \
                LISA_LOGI(TAG, "| %4d | %-25s | %4d | %4d |", i,                                                       \
                          _userdata->setting.wifi.p_hotspot_info->hotspot[i].ssid,                                     \
                          _userdata->setting.wifi.p_hotspot_info->hotspot[i].rssi,                                     \
                          _userdata->setting.wifi.p_hotspot_info->hotspot[i].state);                                   \
            }                                                                                                          \
        } else {                                                                                                       \
            LISA_LOGI(TAG, "| Hotspot info is NULL                  |");                                               \
        }                                                                                                              \
        LISA_LOGI(TAG, "+-----------------------------------------+");                                                 \
    } while (0)

// 定义映射数组
static const lisaui_userdata_setting_wifi_connect_state_e wifi_connect_state_map[] = {
    [VIEW_WIFI_STATE_CONNECTING] = LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTING,
    [VIEW_WIFI_STATE_CONNECTED] = LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTED,
    [VIEW_WIFI_STATE_CONNECT_FAILED] = LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECT_FAILED,
    [VIEW_WIFI_STATE_DISCONNECTED] = LISAUI_USERDATA_WIFI_CONNECT_STATE_DISCONNECTED,
    // 其他状态可以根据需要映射
};
// 定义映射数组
static const lisaui_userdata_setting_wifi_scan_state_e wifi_scan_state_map[] = {
    [VIEW_WIFI_STATE_SCANNING] = LISAUI_USERDATA_WIFI_SCAN_STATE_SCANNING,
    [VIEW_WIFI_STATE_SCANNED] = LISAUI_USERDATA_WIFI_SCAN_STATE_SCANNED,
    // 其他状态可以根据需要映射
};

static view_handler_t *view_handler = NULL;

EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_ROLES_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_MCP_EMOJI_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_END)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_SETTING_BATTERY_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE)
static int setup(void)
{
    return 0;
}

static int update_battery_info_work(view_battery_info_t *battery_info)
{
    if (battery_info == NULL) {
        return -EINVAL;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        _userdata->setting.battery.is_charging = battery_info->is_charging;
        _userdata->setting.battery.usb_status = battery_info->usb_status;
        _userdata->setting.battery.power_percent = battery_info->power_percent;
    }

    workqueue_submit(view_handler->view->workq,
                        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_SETTING_BATTERY_UPDATE), NULL, 0);

    return 0;
}

static int update_event(view_event_e event)
{
    LISA_LOGI(TAG, "update_event: %d", event);
    bool is_inter_state_update = false;
    bool is_roles_update = false;
    bool is_wakeup = false;
    bool is_cloud_end = false;

    if (view_handler == NULL) {
        return -ENODEV;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {

        switch (event) {
        case VIEW_EVENT_AUDIO_IDLE:
            _userdata->inter.local_state = LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE;
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE;
            // _userdata->roles.roles[_userdata->roles.role_idx].emoji = ROLE_EMOJI_BLINK;  // 注释掉自动重置表情
            is_cloud_end = true;
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_AUDIO_INTER_IDLE:
            if (_userdata->inter.remote_state != LISAUI_USERDATA_INTER_REMOTE_STATE_TALKING) {
                _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE;
                is_inter_state_update = true;
            }
            break;
        case VIEW_EVENT_AUDIO_WAKEUP:
            _userdata->inter.local_state = LISAUI_USERDATA_INTER_LOCAL_STATE_WAKEUP;
            if (_userdata->inter.iat_text != NULL) {
                _userdata->inter.iat_text[0] = '\0';
            }
            is_wakeup = true;
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_AUDIO_CONNECT_CLOUD_FAILED:
            _userdata->inter.local_state = LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE;
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE;
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_AUDIO_RECORD_START:
            _userdata->inter.local_state = LISAUI_USERDATA_INTER_LOCAL_STATE_RECOGNITION;
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_LISTENING;
            if (_userdata->inter.iat_text != NULL) {
                _userdata->inter.iat_text[0] = '\0';
            }
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_AUDIO_TRIGGERED_VAD:
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING;
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_AUDIO_PLAY_START:
            break;
        case VIEW_EVENT_TTS_PLAY_START:
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_TALKING;
            is_inter_state_update = true;
            break;

        case VIEW_EVENT_AUDIO_PLAY_STOP:
        case VIEW_EVENT_TTS_PLAY_STOP:
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE;
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_GET_ROLES_SUCCESS:
            _userdata->roles.request_state = 1;
            is_roles_update = true;
            break;
        case VIEW_EVENT_GET_ROLES_FAILED:
            _userdata->roles.request_state = 0;
            is_roles_update = true;
            break;
        case VIEW_EVENT_SESSION_END:
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING;
            is_inter_state_update = true;
            break;
        default:
            break;
        }
    }

    if(is_inter_state_update){
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE), NULL, 0);
    }

    if(is_wakeup){
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP), NULL, 0);
    }

    if(is_cloud_end){
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_INTER_END), NULL, 0);
    }

    if(is_roles_update){
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_ROLES_UPDATE), NULL, 0);
    }

    return 0;
}
int change_info_page(void)
{
    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE), NULL, 0);
    void enter_ble_config(void);
    enter_ble_config();
    return 0;
}

static int update_iat_append_text(bool is_refresh, const char *text)
{
    int text_len;
    char *ptr;
    int ret = 0;

    if ((text == NULL) || ((text_len = strlen(text)) == 0)) {
        return -EINVAL;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        exram_free(_userdata->inter.iat_text);
        text_len = strlen(text);
        ptr = exram_malloc(4, text_len + 1);
        if (ptr != NULL) {
            snprintf(ptr, text_len + 1, "%s", text);
            _userdata->inter.iat_text = ptr;
            if (_userdata->inter.reply_text != NULL) {
                _userdata->inter.reply_text[0] = '\0';
            }
        } else {
            ret = -ENOMEM;
        }
    }

    if (ret == 0) {
        workqueue_submit(view_handler->view->workq,
                         EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE), NULL, 0);
    }

    return ret;
}

static int update_weather(const char *json)
{
    /*TODO*/
    return 0;
}

static int update_music(const char *title, const char *artist)
{

    /*TODO*/
    return 0;
}

static int update_alarm(alarm_clock_t *alarm, view_alarm_opt_e opt)
{
    /*TODO*/
    return 0;
}

static const char *wifi_encryption_mode_convert_str(wifi_manager_encryption_mode_t mode)
{
    static const char *wifi_encryption_mode_remap_str[] = {
        "OPEN", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK", "WPA2_ENTERPRISE", "WPA3_PSK", "WPA2_WPA3_PSK",
    };
    if (mode < sizeof(wifi_encryption_mode_remap_str) / sizeof(wifi_encryption_mode_remap_str[0])) {
        return wifi_encryption_mode_remap_str[mode];
    }

    return "UNKNOW";
}
/**
 * Sort hotspots by signal strength (RSSI) and remove duplicated SSIDs
 * Keeps only the strongest signal for each SSID
 *
 * @param src_hotspots 源热点数组
 * @param src_count 源热点数量
 * @param dst_info 目标热点信息结构（已分配内存）
 * @return 处理后的热点数量，负值表示错误
 */
static int _sort_and_deduplicate_hotspots(wifi_mgr_scan_info_t *src_hotspots, int src_count,
                                          lisaui_userdata_setting_wifi_connect_info_t *connect_info,
                                          lisaui_userdata_setting_wifi_hotspot_info_t *dst_info)
{
    if (src_hotspots == NULL || dst_info == NULL || src_count <= 0) {
        return -EINVAL;
    }

    // 先将所有源热点数据复制到目标数组
    for (int i = 0; i < src_count; i++) {
        snprintf(dst_info->hotspot[i].ssid, sizeof(dst_info->hotspot[i].ssid), "%s", src_hotspots[i].ssid);

        snprintf(dst_info->hotspot[i].bssid, sizeof(dst_info->hotspot[i].bssid), "%s", src_hotspots[i].bssid);

        memset(dst_info->hotspot[i].pwd, 0, sizeof(dst_info->hotspot[i].pwd));
        snprintf(dst_info->hotspot[i].encryption_mode_str, sizeof(dst_info->hotspot[i].encryption_mode_str), "%s",
                  wifi_encryption_mode_convert_str(src_hotspots[i].encryption_mode));
        dst_info->hotspot[i].rssi = src_hotspots[i].rssi;
        dst_info->hotspot[i].channel = src_hotspots[i].channel;
        if ((connect_info != NULL) && (connect_info->state == LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTED) &&
            (strcmp(connect_info->cfg.ssid, src_hotspots[i].ssid) == 0)) {
            dst_info->hotspot[i].state = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTED;
        } else {
            dst_info->hotspot[i].state = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_WITHOUT_CONFIG;
        }
    }

    // 在dst_info中按信号强度(RSSI)从大到小排序
    for (int i = 0; i < src_count - 1; i++) {
        for (int j = 0; j < src_count - i - 1; j++) {
            if (dst_info->hotspot[j].rssi < dst_info->hotspot[j + 1].rssi) {
                // 交换热点数据
                lisaui_userdata_setting_wifi_hotspot_t temp = dst_info->hotspot[j];
                dst_info->hotspot[j] = dst_info->hotspot[j + 1];
                dst_info->hotspot[j + 1] = temp;
            }
        }
    }

    // 去除SSID相同的热点（保留信号最强的）
    int valid_count = 0;

    for (int i = 0; i < src_count; i++) {
        bool is_duplicate = false;

        // 已排好序，只需检查前面的热点是否有相同SSID
        for (int j = 0; j < valid_count; j++) {
            if (strcmp(dst_info->hotspot[i].ssid, dst_info->hotspot[j].ssid) == 0) {
                is_duplicate = true;
                break;
            }
        }

        if ((!is_duplicate) && (dst_info->hotspot[i].ssid[0] != '\0')) {
            // 不是重复的，如果当前位置不是valid_count，需要移动
            if (i != valid_count) {
                dst_info->hotspot[valid_count] = dst_info->hotspot[i];
            }
            valid_count++;
        }
    }

    // 设置有效热点数量
    dst_info->number = valid_count;

    return valid_count;
}

static const uint32_t wifi_state_remap[] = {
    [VIEW_WIFI_STATE_DISCONNECTED] = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_WITHOUT_CONFIG,
    [VIEW_WIFI_STATE_CONNECTING] = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTING,
    [VIEW_WIFI_STATE_CONNECTED] = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTED,
    [VIEW_WIFI_STATE_CONNECT_FAILED] = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_WITHOUT_CONFIG,
};

static int update_wifi_state(view_wifi_info_t *info)
{
    int ret = 0;
    if (info == NULL) {
        return -EINVAL;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        _userdata->setting.wifi.is_enable = info->is_enable;
        
        if(true == info->is_enable){

            if ((info->state == VIEW_WIFI_STATE_CONNECTING) 
                || (info->state == VIEW_WIFI_STATE_CONNECTED) 
                ||(info->state == VIEW_WIFI_STATE_CONNECT_FAILED) 
                || (info->state == VIEW_WIFI_STATE_DISCONNECTED)) {

                _userdata->setting.wifi.connect_info.state = wifi_connect_state_map[info->state];
                snprintf(_userdata->setting.wifi.connect_info.cfg.ssid,
                        sizeof(_userdata->setting.wifi.connect_info.cfg.ssid), "%s", info->sta_cfg.ssid);
                snprintf(_userdata->setting.wifi.connect_info.cfg.bssid,
                        sizeof(_userdata->setting.wifi.connect_info.cfg.bssid), "%s", info->sta_cfg.bssid);
                snprintf(_userdata->setting.wifi.connect_info.cfg.pwd, sizeof(_userdata->setting.wifi.connect_info.cfg.pwd),
                        "%s", info->sta_cfg.pwd);

                /* Update ap list*/
                if(_userdata->setting.wifi.p_hotspot_info != NULL){
                    for(int ii=0;ii< _userdata->setting.wifi.p_hotspot_info->number;ii++){
                        if(strcmp(_userdata->setting.wifi.p_hotspot_info->hotspot[ii].ssid,info->sta_cfg.ssid)==0){
                            _userdata->setting.wifi.p_hotspot_info->hotspot[ii].state = wifi_state_remap[info->state];
                        }
                        else if(_userdata->setting.wifi.p_hotspot_info->hotspot[ii].state == LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTED){
                            _userdata->setting.wifi.p_hotspot_info->hotspot[ii].state = LISAUI_USERDATA_WIFI_HOTSPOT_STATE_WITH_CONFIG;
                        }
                    }
                }
                LISA_LOGI(TAG,"Update_wifi_state connect_info cfg ssid:%s,bssid:%s,pwd:%s,state:%d", 
                        info->sta_cfg.ssid, 
                        info->sta_cfg.bssid, 
                        info->sta_cfg.pwd, 
                        info->state);
            }

            else if(info->state == VIEW_WIFI_STATE_SCANNING){
                _userdata->setting.wifi.p_hotspot_info = exram_realloc(_userdata->setting.wifi.p_hotspot_info,
                    sizeof(lisaui_userdata_setting_wifi_hotspot_info_t));

                _userdata->setting.wifi.p_hotspot_info->state = wifi_connect_state_map[info->state];
                _userdata->setting.wifi.p_hotspot_info->number = 0;
            }
            else if (info->state == VIEW_WIFI_STATE_SCANNED) {

                // Free existing memory if already allocated
                _userdata->setting.wifi.p_hotspot_info = exram_realloc(_userdata->setting.wifi.p_hotspot_info,
                    sizeof(lisaui_userdata_setting_wifi_hotspot_info_t) +
                    sizeof(lisaui_userdata_setting_wifi_hotspot_t) * info->hotspot_list.count);
                memset(_userdata->setting.wifi.p_hotspot_info, 0,
                       sizeof(lisaui_userdata_setting_wifi_hotspot_info_t) +
                           sizeof(lisaui_userdata_setting_wifi_hotspot_t) * info->hotspot_list.count);
                // 使用独立函数处理热点排序和去重
                // 设置WiFi扫描状态
                _userdata->setting.wifi.p_hotspot_info->state = wifi_scan_state_map[info->state];

                // 排序和去重热点列表
                int count = _sort_and_deduplicate_hotspots(info->hotspot_list.hotspots, info->hotspot_list.count,
                                                        &_userdata->setting.wifi.connect_info,
                                                        _userdata->setting.wifi.p_hotspot_info);
                if (count < 0) {
                    // 出错处理
                    ret = count;
                }
            }
        }
        else{
            _userdata->setting.wifi.connect_info.state = LISAUI_USERDATA_WIFI_CONNECT_STATE_DISCONNECTED;
            if(_userdata->setting.wifi.p_hotspot_info){
                exram_free(_userdata->setting.wifi.p_hotspot_info);
                _userdata->setting.wifi.p_hotspot_info = NULL;
            }
        }
       
        DEBUG_USER_DATA_WIFI_INFO();
    }

    if (ret == 0) {
        
        workqueue_submit(view_handler->view->workq,
                         EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE), NULL, 0);
    }
    return ret;
}

typedef struct{
    const char *str;
    role_emoji_e emoji;

}role_emoji_remap_t;

static role_emoji_remap_t emoji_remap[] = {
    {
        .str = "love",
        .emoji = ROLE_EMOJI_LOVE,   // 1
    },
    {
        .str = "sad",
        .emoji = ROLE_EMOJI_SAD,    // 2
    },
    {
        .str = "laugh",
        .emoji = ROLE_EMOJI_LAUGH,  // 3
    },
    {
        .str = "squint",
        .emoji = ROLE_EMOJI_SQUINT, // 4
    },
    {
        .str = "angry",
        .emoji = ROLE_EMOJI_ANGRY,  // 5
    },
    {
        .str = "eye",
        .emoji = ROLE_EMOJI_EYE,    // 6
    },
    {
        .str = "blink",
        .emoji = ROLE_EMOJI_BLINK,  // 7
    },
    {
        .str = "happy",
        .emoji = ROLE_EMOJI_HAPPY,  // 8
    },
    {
        .str = "cute",
        .emoji = ROLE_EMOJI_CUTE,   // 9
    },
};

static int update_role_emoji(const char *emoji_string) {
    if (!emoji_string) {
        LISA_LOGE(TAG, "[%s] emoji_string is NULL", __FUNCTION__);
        return -EINVAL;
    }
    
    LISA_LOGI(TAG, "[%s] Updating emoji to: %s", __FUNCTION__, emoji_string);
    
    // 查找匹配的表情字符串
    role_emoji_e emoji = ROLE_EMOJI_UNKNOW;
    for (int i = 0; i < sizeof(emoji_remap) / sizeof(emoji_remap[0]); i++) {
        if (strcmp(emoji_string, emoji_remap[i].str) == 0) {
            emoji = emoji_remap[i].emoji;
            break;
        }
    }
    
    if (emoji == ROLE_EMOJI_UNKNOW) {
        LISA_LOGE(TAG, "[%s] Unknown emoji string: %s", __FUNCTION__, emoji_string);
        return -EINVAL;
    }
    
    bool update_emoji = false;
    LISAUI_USERDATA_WITH_LOCK(_userdata) {
        if (_userdata->roles.roles[_userdata->roles.role_idx].emoji != emoji) {
            update_emoji = true;
            _userdata->roles.roles[_userdata->roles.role_idx].emoji = emoji;
            LISA_LOGI(TAG, "[%s] Emoji updated to: %d", __FUNCTION__, emoji);
        } else {
            // 即使是相同表情也强制触发更新，支持重复设置
            update_emoji = true;
            LISA_LOGI(TAG, "[%s] Same emoji %d, but forcing update for repeat setting", __FUNCTION__, emoji);
        }
    }
    
    if (update_emoji) {
        LISA_LOGI(TAG, "[%s] Triggering emoji update event", __FUNCTION__);
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE), 
            NULL, 0);
    }

    return 0;
}

static int update_mcp_emoji(const char *emoji_string) {
    if (!emoji_string) {
        LISA_LOGE(TAG, "[%s] emoji_string is NULL", __FUNCTION__);
        return -EINVAL;
    }
    
    LISA_LOGI(TAG, "[%s] Updating MCP emoji to: %s", __FUNCTION__, emoji_string);
    
    // 查找匹配的表情字符串
    role_emoji_e emoji = ROLE_EMOJI_UNKNOW;
    for (int i = 0; i < sizeof(emoji_remap) / sizeof(emoji_remap[0]); i++) {
        if (strcmp(emoji_string, emoji_remap[i].str) == 0) {
            emoji = emoji_remap[i].emoji;
            break;
        }
    }
    
    if (emoji == ROLE_EMOJI_UNKNOW) {
        LISA_LOGE(TAG, "[%s] Unknown MCP emoji string: %s", __FUNCTION__, emoji_string);
        return -EINVAL;
    }
    
    bool update_emoji = false;
    LISAUI_USERDATA_WITH_LOCK(_userdata) {
        if (_userdata->roles.roles[_userdata->roles.role_idx].emoji != emoji) {
            update_emoji = true;
            _userdata->roles.roles[_userdata->roles.role_idx].emoji = emoji;
            LISA_LOGI(TAG, "[%s] MCP Emoji updated to: %d", __FUNCTION__, emoji);
        } else {
            // 即使是相同表情也强制触发更新，支持重复设置
            update_emoji = true;
            LISA_LOGI(TAG, "[%s] Same MCP emoji %d, but forcing update for repeat setting", __FUNCTION__, emoji);
        }
    }
    
    if (update_emoji) {
        LISA_LOGI(TAG, "[%s] Triggering MCP emoji update event", __FUNCTION__);
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_MCP_EMOJI_UPDATE), 
            NULL, 0);
    }

    return 0;
}
static int update_reply_text(const char *text, lisaui_userdata_text_mode_e mode)
{
    int ret = 0;
    int text_len;
    int old_text_len = 0;
    char *ptr;

    if ((text == NULL) || (strlen(text) == 0)) {
        return -EINVAL;
    }
    
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        if (NULL != _userdata->inter.reply_text)
        {
            old_text_len = strlen(_userdata->inter.reply_text);
        }

        if (mode == LISAUI_USERDATA_TEXT_MODE_APPEND) {
            text_len = strlen(text) + old_text_len;
        } else {
            text_len = strlen(text);
            old_text_len = 0;
        }
        // LISA_LOGI(TAG, "update_reply_text old_text_len %d, text_len %d mode: %d", old_text_len, text_len, mode);
        ptr = exram_malloc(4, text_len + 1);
        if (ptr != NULL) {
            if (mode == LISAUI_USERDATA_TEXT_MODE_APPEND && old_text_len > 0) {
                memcpy(ptr, _userdata->inter.reply_text, old_text_len);
            }
            snprintf(ptr + old_text_len, text_len - old_text_len + 1, "%s", text);
            exram_free(_userdata->inter.reply_text);
            _userdata->inter.reply_text = ptr;
        } else {
            ret = -ENOMEM;
        }
        // LISA_LOGI(TAG, "update_reply_text text %s, text:%s", _userdata->inter.reply_text, text);
    }


    if (ret == 0) {
        workqueue_submit(view_handler->view->workq,
                         EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE), NULL, 0);
    }

    return ret;
}

static int _bus_init(assistant_view_t *view)
{
    ebus_handle_t *bus_handle;

    ebus_init();

    bus_handle = ebus_create(LISAUI_EBUS_NAME);
    if (bus_handle == NULL) {
        LISA_LOGE(TAG, "Create bus(%s) failed.", LISAUI_EBUS_NAME);
        return -EIO;
    }
    view->ebus_info.handle = bus_handle;
    view->ebus_info.base_event_chn = ebus_chn_create_attach(bus_handle, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    view_event_init(view);
    return 0;
}

void assistant_view_userdata_load(void){
    lisaui_data_init();
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        _userdata->setting.volume_percent = listen_get_volume();
        // _userdata->setting.mic_is_mute = listen_mic_mute_get(); 
        _userdata->setting.mic_is_mute = false; 
        _userdata->setting.mic_gain = listen_mic_gain_get();
    }
}

assistant_view_t *assistant_view_init(assistant_view_cbs_t *cbs)
{

    assistant_view_t *view = NULL;
    view_handler_t *handler;

    handler = (view_handler_t *)exram_malloc(4, sizeof(view_handler_t));
    if (!handler) {
        return NULL;
    }
    memset(handler, 0, sizeof(view_handler_t));

    view = (assistant_view_t *)exram_malloc(4, sizeof(assistant_view_t));
    if (!view) {
        return NULL;
    }

    memset(view, 0, sizeof(assistant_view_t));
    if (cbs != NULL) {
        memcpy(&view->cbs, cbs, sizeof(assistant_view_cbs_t));
    }
    handler->view = view;

    view->ops.setup = setup;
    view->ops.update_event = update_event;
    view->ops.update_iat_append_text = update_iat_append_text;
    view->ops.update_weather = update_weather;
    view->ops.update_music = update_music;
    view->ops.update_alarm = update_alarm;
    view->ops.update_wifi_state = update_wifi_state;
    view->ops.update_role_emoji = update_role_emoji;
    view->ops.update_mcp_emoji = update_mcp_emoji;
    view->ops.update_reply_text = update_reply_text;
    view->ops.update_battery_status = update_battery_info_work;

    view_setting_init(view);

    view_workq = workqueue_create(VIEW_WORKQ_THREAD_NAME, VIEW_WORKQ_THREAD_PRIORITY, VIEW_WORKQ_QUEUE_LENGTH,
                                  VIEW_WORKQ_THREAD_STACK_SIZE);

    view->workq = view_workq;
    // view_timer = xTimerCreate("view_timer",       // 定时器名称
    //                           pdMS_TO_TICKS(10),  // 延时时间（1秒）
    //                           pdTRUE,             // 自动重载（周期性任务）
    //                           (void *)0,          // 参数传递
    //                           view_timer_callback // 回调函数
    // );
    // xTimerStart(view_timer, 0); // 启动定时器

    view_handler = handler;


    _bus_init(view);
    view_runner_start(view);
    return view;
}
