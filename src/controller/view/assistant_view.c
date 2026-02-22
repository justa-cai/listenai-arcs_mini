
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
#include "cJSON.h"
#include "lisa_aiui.h"

#include "lisa_log.h"
#include "lvgl.h"
#include "display/lv_img_net_loader.h"
// #include "ui.h"

static const char *TAG = "assistant_view";

workqueue_t *view_workq = NULL;
TimerHandle_t view_timer = NULL;
static bool audio_listen_status = 0;

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

// 前向声明
static char* add_oss_image_params(const char* original_url, int width, int quality, const char* format);

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
// 交互模式映射数组
static const lisaui_userdata_setting_inter_mode_e inter_mode_map[] = {
    [INTER_CONTINUE] = LISAUI_USERDATA_SETTING_INTER_MODE_DUAL,
    [INTER_ONESHOT] = LISAUI_USERDATA_SETTING_INTER_MODE_HALF,
    [INTER_BUTTON] = LISAUI_USERDATA_SETTING_INTER_MODE_KEY,
};

static view_handler_t *view_handler = NULL;

EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_ROLES_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_MCP_EMOJI_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_STANDBY_TEXTS_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_END)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_INTER_MODE_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_ALARM_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_SETTING_BATTERY_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_SHOW)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_HIDE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_OTA_STATE_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_WAKE_WORD_UPDATE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_NET_IMAGE_SHOW)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_NET_IMAGE_HIDE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_MUSIC_ICON_HIDE)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_SHOW_LOADING)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_MUSIC_PLAY_START)
EBUS_MESSAGE_PUB_BY_WORK_DEFINE(LISAUI_EBUS_CH_EVENT_M2U_MUSIC_PLAY_STOP)

/* 0: other status 1: listening */
bool get_audio_listen_status(void)
{
    return audio_listen_status;
}

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
    bool is_image_hide = false;

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
            is_image_hide = true;
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
            is_image_hide = true;
            break;
        case VIEW_EVENT_AUDIO_TRIGGERED_VAD:
            _userdata->inter.remote_state = LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING;
            is_inter_state_update = true;
            break;
        case VIEW_EVENT_AUDIO_PLAY_START:
            // is_image_hide = true;
            break;
        case VIEW_EVENT_TTS_PLAY_START:
            // is_image_hide = true;
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

        if (((_userdata->inter.remote_state == LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE) && (_userdata->inter.local_state == LISAUI_USERDATA_INTER_LOCAL_STATE_RECOGNITION)) || \
            (_userdata->inter.remote_state == LISAUI_USERDATA_INTER_REMOTE_STATE_LISTENING)) {
            audio_listen_status = 1;
        } else {
            audio_listen_status = 0;
        }
    }

    if (is_image_hide) {
        assistant_view_hide_camera_image();
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

    if(event == VIEW_EVENT_AUDIO_PLAY_START){
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_MUSIC_PLAY_START), NULL, 0);
    }

    if(event == VIEW_EVENT_AUDIO_PLAY_STOP){
        workqueue_submit(view_handler->view->workq,
            EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_MUSIC_PLAY_STOP), NULL, 0);
    }

    return 0;
}
int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode)
{
    // 检查是否已经在info页面
    LISA_LOGI(TAG, "change_info_page, mode: %d", mode);
    // extern bool is_info_page_active(void);
    // if (is_info_page_active()) {
    //     printf("Info page is already active, ignoring toggle request\n");
    //     return 0;
    // }

    if (mode >= LISAUI_USERDATA_QRCODE_INTER_UNKNOW_MODE) {
        LISA_LOGE(TAG, "Unknow mode, mode: %d", mode);
        return -1;
    }

    // 使用带锁的用户数据宏定义，确保线程安全地访问用户数据
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
    // 设置二维码交互模式
        _userdata->qrcode_inter.mode = mode;
    }

    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE), NULL, 0);
    // void enter_ble_config(void);
    // enter_ble_config();
    return 0;
}

static void camera_image_show_work(void *param)
{
    ebus_message_pub(view_handler->view->ebus_info.base_event_chn,
                     LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_SHOW,
                     param,
                     sizeof(lisaui_camera_image_params_t));
}

static void net_image_show_work(void *param)
{
    ebus_message_pub(view_handler->view->ebus_info.base_event_chn,
                     LISAUI_EBUS_CH_EVENT_M2U_NET_IMAGE_SHOW,
                     param,
                     sizeof(lisaui_net_image_params_t));
}

static void music_cover_show_work(void *param)
{
    ebus_message_pub(view_handler->view->ebus_info.base_event_chn,
                     LISAUI_EBUS_CH_EVENT_M2U_MUSIC_COVER_SHOW,
                     param,
                     sizeof(lisaui_net_image_params_t));
}

// 显示拍照图片到页面
int assistant_view_show_camera_image(const uint16_t *rgb565_data, uint32_t width, uint32_t height)
{
    if (!rgb565_data) {
        printf("Invalid rgb565_data\n");
        return -1;
    }
    
    // 使用exram_malloc动态分配内存（ebus系统需要动态内存）
    lisaui_camera_image_params_t *params = exram_malloc(32, sizeof(lisaui_camera_image_params_t));
    if (!params) {
        printf("ERROR: Failed to allocate memory for camera image params\n");
        return -1;
    }
    
    params->rgb565_data = rgb565_data;
    params->width = width;
    params->height = height;
    
    printf("DEBUG: Sending camera image event: params=%p, buffer=%p, size=%dx%d, params_size=%zu\n",
           params, rgb565_data, width, height, sizeof(lisaui_camera_image_params_t));
    
    // 使用工作队列提交任务（接收端会用exram_free释放内存）
    int result = workqueue_submit(view_handler->view->workq,
                                  camera_image_show_work,
                                  params,
                                  0);
    
    printf("DEBUG: workqueue_submit result: %d\n", result);
    
    return 0;
}

// 显示网络下载的图片
int assistant_view_show_net_image(const void *img_dsc)
{
    const lv_img_dsc_t *dsc = (const lv_img_dsc_t *)img_dsc;
    if (!dsc) {
        printf("Invalid img_dsc\n");
        return -1;
    }

    lisaui_net_image_params_t *params = exram_malloc(32, sizeof(lisaui_net_image_params_t));
    if (!params) {
        printf("ERROR: Failed to allocate memory for net image params\n");
        return -1;
    }

    params->img_dsc = dsc;

    printf("DEBUG: Sending net image event: params=%p, img_dsc=%p, data_size=%u\n",
           params, dsc, dsc->data_size);

    int result = workqueue_submit(view_handler->view->workq,
                                  net_image_show_work,
                                  params,
                                  0);

    printf("DEBUG: workqueue_submit (net image) result: %d\n", result);

    return 0;
}

// 隐藏拍照图片
int assistant_view_hide_camera_image(void)
{
    // 使用工作队列提交任务，无需参数
    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_HIDE),
        NULL, 0);

    return 0;
}

// 隐藏网络图片（音乐封面等）
int assistant_view_hide_net_image(void)
{
    // 使用工作队列提交任务，无需参数
    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_NET_IMAGE_HIDE),
        NULL, 0);

    return 0;
}

// 隐藏音乐图标
int assistant_view_hide_music_icon(void)
{
    // 使用工作队列提交任务，无需参数
    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_MUSIC_ICON_HIDE),
        NULL, 0);

    return 0;
}

int assistant_view_show_loading(const char *text)
{
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->setting.loading_text) {
            exram_free((void *)_userdata->setting.loading_text);
            _userdata->setting.loading_text = NULL;
        }

        if (text) {
            size_t text_len = strlen(text);
            char *new_text = exram_malloc(4, text_len + 1);
            if (new_text) {
                strncpy(new_text, text, text_len);
                new_text[text_len] = '\0';
                _userdata->setting.loading_text = new_text;
            }
        }
    }

    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_SHOW_LOADING),
        NULL, 0);

    return 0;
}

int assistant_view_notify_interactive_mode_update(lisa_aiui_interactive_mode_e mode)
{

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        switch (mode) {
        case INTER_ONESHOT:
            _userdata->setting.inter_mode = LISAUI_USERDATA_SETTING_INTER_MODE_HALF;
            break;
        case INTER_CONTINUE:
            _userdata->setting.inter_mode = LISAUI_USERDATA_SETTING_INTER_MODE_DUAL;
            break;
        case INTER_BUTTON:
            _userdata->setting.inter_mode = LISAUI_USERDATA_SETTING_INTER_MODE_KEY;
            break;
        default:
            break;
        }
    }

    return workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_INTER_MODE_UPDATE),
        NULL, 0);
}

int assistant_view_notify_alarm_update(bool has_alarm)
{
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        _userdata->setting.has_alarm = has_alarm;
    }

    return workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_ALARM_UPDATE),
        NULL, 0);
}

// 更新并显示二维码
int assistant_view_update_qrcode(const char *url, const char *message, const char *err_code)
{
    if (!url) {
        LISA_LOGE(TAG, "assistant_view_update_qrcode: url is NULL");
        return -EINVAL;
    }

    LISA_LOGI(TAG, "assistant_view_update_qrcode: url=%s, message=%s, err_code=%s",
              url, message ? message : "(none)", err_code ? err_code : "(none)");

    // 根据err_code或默认使用CONFIGURE_DEVICE模式
    lisaui_userdata_qrcode_inter_mode_e mode = LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE;

    if (err_code) {
        // 根据错误码决定显示模式
        if (strcmp(err_code, "quota") == 0 || strcmp(err_code, "OutOfLimit") == 0) {
            mode = LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA;
        } else if (strcmp(err_code, "network") == 0) {
            mode = LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK;
        }
    }else if (strstr(message, "VIP") != NULL) {
        mode = LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_ANNUAL_VIP;
    }else if (strstr(message, "图片绘制中") != NULL) {
        mode = LISAUI_USERDATA_QRCODE_INTER_VIEW_IMAGE;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata) {
        // 根据模式设置不同的字段
        char **target_url = NULL;
        char **target_message = NULL;

        switch (mode) {
            case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK:
                target_url = &_userdata->qrcode_inter.network_url;
                target_message = &_userdata->qrcode_inter.network_label_text;
                break;
            case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE:
                target_url = &_userdata->qrcode_inter.device_url;
                target_message = &_userdata->qrcode_inter.device_label_text;
                break;
            case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_ANNUAL_VIP:
                target_url = &_userdata->qrcode_inter.vip_url;
                target_message = &_userdata->qrcode_inter.vip_label_text;
                break;
            case LISAUI_USERDATA_QRCODE_INTER_VIEW_IMAGE:
                target_url = &_userdata->qrcode_inter.view_image_url;
                target_message = &_userdata->qrcode_inter.view_image_label_text;
                break;
            case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA:
                target_url = &_userdata->qrcode_inter.quota_url;
                target_message = &_userdata->qrcode_inter.quota_label_text;
                break;
            default:
                LISA_LOGE(TAG, "Unknown qrcode mode: %d", mode);
                return -EINVAL;
        }

        // 清理旧的URL
        if (*target_url) {
            exram_free(*target_url);
            *target_url = NULL;
        }

        // 设置新的URL（添加OSS处理参数和N:前缀）
        char *url_with_params = add_oss_image_params(url, 168, 100, "jpg");
        if (url_with_params) {
            size_t url_len = strlen(url_with_params);
            *target_url = exram_malloc(4, url_len + 3);  // +2 for "N:" +1 for '\0'
            if (*target_url) {
                snprintf(*target_url, url_len + 3, "N:%s", url_with_params);
            }
            exram_free(url_with_params);
        }

        // 清理旧的消息文本
        if (*target_message) {
            exram_free(*target_message);
            *target_message = NULL;
        }

        // 设置新的消息文本
        if (message && strlen(message) > 0) {
            size_t msg_len = strlen(message);
            *target_message = exram_malloc(4, msg_len + 1);
            if (*target_message) {
                strncpy(*target_message, message, msg_len);
                (*target_message)[msg_len] = '\0';
            }
        }
    }

    // 切换到二维码页面
    change_info_page(mode);

    LISA_LOGI(TAG, "QR code updated successfully, mode=%d", mode);
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

    // Note: Removed assistant_view_hide_camera_image() call here.
    // Image hiding should be handled by the specific image display functions:
    // - lisa_ui_llm_primary_show_camera_image() hides previous images when showing camera
    // - lisa_ui_llm_primary_show_music_cover() hides camera image when showing music cover
    // Hiding images here would incorrectly hide music covers that should persist.

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

static int update_music(const char *title, const char *artist, const char *image_url, const lv_img_dsc_t* image_dsc)
{
    if (!title || !artist) {
        LISA_LOGE(TAG, "%s: invalid parameters", __FUNCTION__);
        return -EINVAL;
    }

    LISA_LOGI(TAG, "%s: title=%s, artist=%s, image_url=%s, image_dsc=%p", __FUNCTION__, title, artist, image_url ? image_url : "NULL", image_dsc);

    // 如果有图片描述符，直接显示音乐封面和标题
    if (image_dsc) {
        LISA_LOGI(TAG, "Displaying music cover with title: %s", title);

        lisaui_net_image_params_t *params = exram_malloc(32, sizeof(lisaui_net_image_params_t));
        if (!params) {
            LISA_LOGE(TAG, "Failed to allocate memory for music cover params");
            return -ENOMEM;
        }

        params->img_dsc = image_dsc;
        params->music_title = title;

        // 使用工作队列提交任务
        int result = workqueue_submit(view_handler->view->workq,
                                      music_cover_show_work,
                                      params,
                                      0);

        if (result != 0) {
            LISA_LOGE(TAG, "Failed to submit music cover show work: %d", result);
            exram_free(params);
            return -EIO;
        }

        LISA_LOGI(TAG, "Music cover display submitted successfully");
    } else {
        // 没有图片，隐藏之前显示的图片
        LISA_LOGI(TAG, "No image provided, hiding music cover");
        assistant_view_hide_camera_image();
    }

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

                _userdata->setting.wifi.p_hotspot_info->state = wifi_connect_state_map[info->state];//wifi_connect_state_map
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

static int update_standby_texts(const char *json_data) {
    if (!json_data) {
        LISA_LOGE(TAG, "[%s] json_data is NULL", __FUNCTION__);
        return -EINVAL;
    }
    
    LISA_LOGI(TAG, "[%s] Updating standby texts with JSON: %s", __FUNCTION__, json_data);
    
    // 解析 banner JSON 数据
    cJSON *banner = cJSON_Parse(json_data);
    if (!banner) {
        LISA_LOGE(TAG, "[%s] Invalid JSON", __FUNCTION__);
        return -EINVAL;
    }
    
    // 获取 resources 数组
    cJSON *resources = cJSON_GetObjectItem(banner, "resources");
    if (!resources || !cJSON_IsArray(resources)) {
        LISA_LOGE(TAG, "[%s] Invalid resources array", __FUNCTION__);
        cJSON_Delete(banner);
        return -EINVAL;
    }
    
    // 获取 interval_ms
    cJSON *interval_ms_item = cJSON_GetObjectItem(banner, "interval_ms");
    uint32_t interval_ms = 3000;  // 默认3秒
    if (interval_ms_item && cJSON_IsNumber(interval_ms_item)) {
        interval_ms = (uint32_t)interval_ms_item->valueint;
    }
    
    int resources_count = cJSON_GetArraySize(resources);
    if (resources_count < 0 || resources_count > 10) {
        LISA_LOGE(TAG, "[%s] Invalid resources count: %d", __FUNCTION__, resources_count);
        cJSON_Delete(banner);
        return -EINVAL;
    }
    
    // 提取文本数组
    const char *texts[10];  // 最大数量
    int valid_count = 0;
    
    for (int i = 0; i < resources_count && i < 10; i++) {
        cJSON *resource = cJSON_GetArrayItem(resources, i);
        if (resource) {
            cJSON *text_item = cJSON_GetObjectItem(resource, "text");
            if (text_item && cJSON_IsString(text_item) && strlen(text_item->valuestring) > 0) {
                texts[valid_count] = text_item->valuestring;
                LISA_LOGI(TAG, "[%s] Found standby text: %s", __FUNCTION__, texts[valid_count]);
                valid_count++;
            } else if (text_item && cJSON_IsString(text_item)) {
                LISA_LOGW(TAG, "[%s] Skipping empty text at index %d", __FUNCTION__, i);
            }
        }
    }
    
    if (valid_count == 0) {
        LISA_LOGE(TAG, "[%s] No valid texts found", __FUNCTION__);
        lisaui_userdata_clear_standby_texts();
        cJSON_Delete(banner);
        goto exit;
    }

    int ret = lisaui_userdata_set_standby_texts(texts, valid_count, interval_ms, true);

    cJSON_Delete(banner);
    
    if (ret != 0) {
        LISA_LOGE(TAG, "[%s] Failed to set standby texts, ret=%d", __FUNCTION__, ret);
        return ret;
    }
    
    LISA_LOGI(TAG, "[%s] Successfully set %d standby texts with interval %dms", 
             __FUNCTION__, valid_count, interval_ms);
exit:
    // 触发待机文本更新事件，通知 UI 层数据已更新
    workqueue_submit(view_handler->view->workq,
        EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_STANDBY_TEXTS_UPDATE), 
        NULL, 0);

    return 0;
}

// 辅助函数：为URL添加阿里云OSS图片处理参数
static char* add_oss_image_params(const char* original_url, int width, int quality, const char* format) {
    if (!original_url || !format) {
        LISA_LOGE(TAG, "Invalid parameters for OSS image params");
        return NULL;
    }
    
    // 检查width和quality参数范围
    if (width <= 0 || width > 2048) {
        LISA_LOGW(TAG, "Width %d out of range, using default 200", width);
        width = 200;
    }
    
    if (quality <= 0 || quality > 100) {
        LISA_LOGW(TAG, "Quality %d out of range, using default 100", quality);
        quality = 100;
    }
    
    // 找到URL中的查询参数位置（'?'字符）
    const char *query_start = strchr(original_url, '?');
    size_t base_url_len;
    
    if (query_start) {
        // 如果URL中已有参数，只取基础URL部分
        base_url_len = query_start - original_url;
    } else {
        // 如果URL中没有参数，使用完整URL长度
        base_url_len = strlen(original_url);
    }
    
    // 构建OSS处理参数字符串
    // ?x-oss-process=image/resize,m_pad,w_200,limit_0,color_000000/quality,q_100/format,jpg
    char oss_params[256];
    snprintf(oss_params, sizeof(oss_params), 
             "?x-oss-process=image/resize,m_pad,w_%d,limit_0,color_000000/quality,q_%d/format,%s",
             width, quality, format);
    
    // 计算新URL的总长度
    size_t params_len = strlen(oss_params);
    size_t new_len = base_url_len + params_len;
    
    // 分配内存
    char *new_url = exram_malloc(4, new_len + 1);
    if (!new_url) {
        LISA_LOGE(TAG, "Failed to allocate memory for URL with OSS params");
        return NULL;
    }
    
    // 拼接基础URL和新参数
    snprintf(new_url, new_len + 1, "%.*s%s", (int)base_url_len, original_url, oss_params);
    
    LISA_LOGI(TAG, "Added OSS params: %s -> %s", original_url, new_url);
    
    return new_url;
}

static int update_device_config(const char *json_data) {
    if (!json_data) {
        LISA_LOGE(TAG, "[%s] json_data is NULL", __FUNCTION__);
        return -EINVAL;
    }
    
    LISA_LOGI(TAG, "[%s] Updating device config with JSON: %s", __FUNCTION__, json_data);
    
    // 解析 role_config JSON 数据
    cJSON *role_config = cJSON_Parse(json_data);
    if (!role_config) {
        LISA_LOGE(TAG, "[%s] Invalid JSON", __FUNCTION__);
        return -EINVAL;
    }
    
    // 获取文本信息
    cJSON *text_item = cJSON_GetObjectItem(role_config, "text");
    const char *text = NULL;
    if (text_item && cJSON_IsString(text_item)) {
        text = text_item->valuestring;
    }
    
    // 获取图片URL信息
    cJSON *image_url_item = cJSON_GetObjectItem(role_config, "image_url");
    const char *image_url = NULL;
    if (image_url_item && cJSON_IsString(image_url_item)) {
        image_url = image_url_item->valuestring;
    }
    
    LISA_LOGI(TAG, "[%s] Device config - text: %s, image_url: %s", 
             __FUNCTION__, text ? text : "NULL", image_url ? image_url : "NULL");
    
    // 更新用户数据中的设备配置信息
    LISAUI_USERDATA_WITH_LOCK(_userdata) {
        // 清理之前的设备配置文本
        if (_userdata->qrcode_inter.device_label_text) {
            exram_free(_userdata->qrcode_inter.device_label_text);
            _userdata->qrcode_inter.device_label_text = NULL;
        }
        
        // 设置新的设备配置文本
        if (text && strlen(text) > 0) {
            size_t text_len = strlen(text);
            _userdata->qrcode_inter.device_label_text = exram_malloc(4, text_len + 1);
            if (_userdata->qrcode_inter.device_label_text) {
                strncpy(_userdata->qrcode_inter.device_label_text, text, text_len);
                _userdata->qrcode_inter.device_label_text[text_len] = '\0';
            }
        }
        
        // 清理之前的设备配置URL
        if (_userdata->qrcode_inter.device_url) {
            exram_free(_userdata->qrcode_inter.device_url);
            _userdata->qrcode_inter.device_url = NULL;
        }
        
        // 设置新的设备配置URL（添加"N:"前缀）
        if (image_url && strlen(image_url) > 0) {
            // 为URL添加OSS图片处理参数 (宽度168, 质量100, 格式jpg)
            char *url_with_params = add_oss_image_params(image_url, 168, 100, "jpg");
            if (url_with_params) {
                size_t url_len = strlen(url_with_params);
                _userdata->qrcode_inter.device_url = exram_malloc(4, url_len + 3); // +2 for "N:" +1 for '\0'
                if (_userdata->qrcode_inter.device_url) {
                    snprintf(_userdata->qrcode_inter.device_url, url_len + 3, "N:%s", url_with_params);
                }
                exram_free(url_with_params); // 释放临时URL
            }
        }
    }
    
    cJSON_Delete(role_config);
    
    LISA_LOGI(TAG, "[%s] Successfully updated device config", __FUNCTION__);
    
    return 0;
}

static int update_outof_limit_error(const char *result_item_json)
{
    if (!result_item_json) {
        LISA_LOGE(TAG, "[%s] result_item_json is NULL", __FUNCTION__);
        return -EINVAL;
    }
    
    LISA_LOGI(TAG, "[%s] Processing OutOfLimit error from result_item: %s", __FUNCTION__, result_item_json);
    
    // 解析完整的 result_item JSON 数据
    cJSON *result_item = cJSON_Parse(result_item_json);
    if (!result_item) {
        LISA_LOGE(TAG, "[%s] Invalid JSON", __FUNCTION__);
        return -EINVAL;
    }
    
    // 获取错误信息
    cJSON *error = cJSON_GetObjectItem(result_item, "error");
    if (!error) {
        LISA_LOGE(TAG, "[%s] No error object found", __FUNCTION__);
        cJSON_Delete(result_item);
        return -EINVAL;
    }
    
    cJSON *message = cJSON_GetObjectItem(error, "message");
    if (!message || !cJSON_IsString(message)) {
        LISA_LOGE(TAG, "[%s] Invalid error message", __FUNCTION__);
        cJSON_Delete(result_item);
        return -EINVAL;
    }
    
    // 获取URL信息
    cJSON *url = cJSON_GetObjectItem(result_item, "url");
    
    LISA_LOGI(TAG, "[%s] Error message: %s", __FUNCTION__, message->valuestring);
    if (url && cJSON_IsString(url)) {
        LISA_LOGI(TAG, "[%s] Error URL: %s", __FUNCTION__, url->valuestring);
    }
    
    // 更新用户数据，设置二维码交互模式为配额页面
    // OutOfLimit 错误通常需要显示二维码引导用户处理
    LISAUI_USERDATA_WITH_LOCK(_userdata) {
        // 将错误消息保存到配额模式专用的 label 文本中进行显示
        if (_userdata->qrcode_inter.quota_label_text) {
            exram_free(_userdata->qrcode_inter.quota_label_text);
            _userdata->qrcode_inter.quota_label_text = NULL;
        }
        
        size_t msg_len = strlen(message->valuestring);
        _userdata->qrcode_inter.quota_label_text = exram_malloc(4, msg_len + 1);
        if (_userdata->qrcode_inter.quota_label_text) {
            strncpy(_userdata->qrcode_inter.quota_label_text, message->valuestring, msg_len);
            _userdata->qrcode_inter.quota_label_text[msg_len] = '\0';
        }
        
        // 清理之前的配额模式 URL
        if (_userdata->qrcode_inter.quota_url) {
            exram_free(_userdata->qrcode_inter.quota_url);
            _userdata->qrcode_inter.quota_url = NULL;
        }
        
        // 如果有URL，保存到配额模式专用的URL字段
        if (url && cJSON_IsString(url) && url->valuestring) {
            // 为URL添加OSS图片处理参数 (宽度168, 质量100, 格式jpg)
            char *url_with_params = add_oss_image_params(url->valuestring, 168, 100, "jpg");
            if (url_with_params) {
                size_t url_len = strlen(url_with_params);
                // 为URL添加"N:"前缀，表示网络URL格式
                _userdata->qrcode_inter.quota_url = exram_malloc(4, url_len + 3);  // +2 for "N:" +1 for '\0'
                if (_userdata->qrcode_inter.quota_url) {
                    snprintf(_userdata->qrcode_inter.quota_url, url_len + 3, "N:%s", url_with_params);
                }
                exram_free(url_with_params); // 释放临时URL
            }
        }
    }
    
    cJSON_Delete(result_item);
    
    LISA_LOGI(TAG, "[%s] Successfully updated OutOfLimit error info", __FUNCTION__);
    
    // 切换到配额相关的二维码页面
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA);
    
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

static int update_ota_state(ota_state_t *state)
{
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        memcpy(&_userdata->setting.ota, state, sizeof(ota_state_t));
    }

    workqueue_submit(view_handler->view->workq,
                     EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_OTA_STATE_UPDATE), NULL, 0);

    return 0;
}

static int update_wake_word(const char *wake_word)
{
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        snprintf(_userdata->setting.wake_word, sizeof(_userdata->setting.wake_word), "%s", wake_word);
    }

    workqueue_submit(view_handler->view->workq,
                     EBUS_MESSAGE_PUB_BY_WORK_DECLARE(LISAUI_EBUS_CH_EVENT_M2U_WAKE_WORD_UPDATE), NULL, 0);

    return 0;
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
    
    // 从 lisa_aiui 获取当前交互模式并同步到 _userdata
    lisa_aiui_interactive_mode_e current_inter_mode = lisa_aiui_get_interactive_mode();
    
    LISAUI_USERDATA_WITH_LOCK(_userdata){
        _userdata->setting.volume_percent = listen_get_volume();
        // _userdata->setting.mic_is_mute = listen_mic_mute_get(); 
        _userdata->setting.mic_is_mute = false; 
        _userdata->setting.mic_gain = listen_mic_gain_get();
        _userdata->setting.inter_mode = inter_mode_map[current_inter_mode];
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
    view->ops.update_standby_texts = update_standby_texts;
    view->ops.update_device_config = update_device_config;
    view->ops.update_outof_limit_error = update_outof_limit_error;
    view->ops.update_battery_status = update_battery_info_work;
    view->ops.update_ota_state = update_ota_state;
    view->ops.update_wake_word = update_wake_word;

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
