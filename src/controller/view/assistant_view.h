#ifndef __ASSISTANT_VIEW_H__
#define __ASSISTANT_VIEW_H__

#include <stdint.h>
#include <stdbool.h>
#include "wifi_manager/wifi_manager.h"
#include "view_setting.h"
#include "workqueue.h"
#include "ebus/ebus.h"
#include "lisaui_user_data.h"
#include "ota_manager.h"
#include "lisa_aiui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIEW_WIFI_AP_SSID_MAX_LEN            (32)
#define VIEW_WIFI_AP_BSSID_MAX_LEN           (32)
#define VIEW_WIFI_AP_PWD_MAX_LEN             (32)
#define VIEW_WIFI_AP_ENCRYPTION_MODE_MAX_LEN (16)

typedef struct {
    int index;
    bool is_enable;
    bool is_repeat_per_day;
    uint64_t timestamp;
} alarm_clock_t;

typedef struct {
    int count;
    alarm_clock_t alarms[0];
} alarm_clock_list_t;

typedef enum {

    VIEW_EVENT_AUDIO_IDLE,
    VIEW_EVENT_AUDIO_INTER_IDLE,
    VIEW_EVENT_AUDIO_WAKEUP,
    VIEW_EVENT_AUDIO_CONNECT_CLOUD_FAILED,
    VIEW_EVENT_AUDIO_RECORD_START,
    VIEW_EVENT_AUDIO_TRIGGERED_VAD,
    VIEW_EVENT_AUDIO_PLAY_START,
    VIEW_EVENT_AUDIO_PLAY_STOP,
    VIEW_EVENT_TTS_PLAY_START,
    VIEW_EVENT_TTS_PLAY_STOP,
    VIEW_EVENT_GET_ROLES_SUCCESS,
    VIEW_EVENT_GET_ROLES_FAILED,
    VIEW_EVENT_SESSION_END,

} view_event_e;

typedef enum {

    VIEW_WIFI_STATE_CONNECTING,
    VIEW_WIFI_STATE_CONNECTED,
    VIEW_WIFI_STATE_CONNECT_FAILED,
    VIEW_WIFI_STATE_DISCONNECTED,
    VIEW_WIFI_STATE_SCANNING,
    VIEW_WIFI_STATE_SCANNED,

} view_wifi_state_e;

typedef struct {
    char ssid[VIEW_WIFI_AP_SSID_MAX_LEN + 1];   /* Target AP SSID */
    char bssid[VIEW_WIFI_AP_BSSID_MAX_LEN + 1]; /* Target AP bssid (MAC address) */
    char pwd[VIEW_WIFI_AP_PWD_MAX_LEN + 1];
} view_wifi_sta_cfg_t;

typedef struct {
    bool is_charging;
    uint8_t usb_status;
    uint8_t power_percent;
} view_battery_info_t;

typedef struct {

    int count;
    wifi_mgr_scan_info_t hotspots[0];

} view_wifi_hotspot_list_t;

typedef struct {
    int is_enable;
    view_wifi_state_e state;
    union {
        view_wifi_sta_cfg_t sta_cfg;
        view_wifi_hotspot_list_t hotspot_list;
    };
} view_wifi_info_t;

typedef enum {
    VIEW_ALARM_OPT_ADD,
    VIEW_ALARM_OPT_DELETE
} view_alarm_opt_e;

typedef struct {

    int (*setup)(void);
    int (*update_event)(view_event_e event);
    int (*update_iat_append_text)(bool is_refresh, const char *text);
    int (*update_weather)(const char *json);
    int (*update_music)(const char *title, const char *artist);
    int (*update_alarm)(alarm_clock_t *alarm, view_alarm_opt_e opt);
    int (*update_wifi_state)(view_wifi_info_t *info);
    int (*update_role_emoji)(const char * emoji);
    int (*update_mcp_emoji)(const char * emoji);
    int (*update_battery_status)(view_battery_info_t *battery_info);
    int (*update_reply_text)(const char *text, lisaui_userdata_text_mode_e mode);
    int (*update_standby_texts)(const char *json_data);
    int (*update_device_config)(const char *json_data);
    int (*update_outof_limit_error)(const char *error_json);
    int (*update_ota_state)(ota_state_t *state);
    int (*update_wake_word)(const char *wake_word);

} assistant_view_ops_t;

typedef struct {

    int (*alarm_list_get)(alarm_clock_list_t *list, int max_count);
    int (*alarm_delete_by_timestamp)(uint64_t timestamp);
    int (*wifi_hotspot_connect)(view_wifi_sta_cfg_t *sta_cfg);
    int (*wifi_hotspot_disconnect)(view_wifi_sta_cfg_t *sta_cfg);
    int (*wifi_hotspot_scan)(void);
    int (*audio_spk_volume_set)(int volume); /* 百分比 */
    int (*audio_spk_volume_get)(void);
    int (*display_backlight_set)(int backlight); /* 百分比 */
    int (*display_backlight_get)(void);
    int (*cloud_interactive_mode_set)(int mode); /*0:半工,1:双工,2:按键*/
    int (*cloud_interactive_mode_get)(void);     /*0:半工,1:双工,2:按键*/
    int (*battery_info_get)(bool *is_charging, uint8_t *power_percent);

    int (*wifi_sta_enable)(void);
    int (*wifi_sta_disable)(void);
    int (*wifi_cnt_clear)(void); 
} assistant_view_cbs_t;

typedef struct {

    assistant_view_ops_t ops;
    assistant_view_cbs_t cbs;
    view_setting_t setting;
    struct {
        ebus_handle_t *handle;
        ebus_chn_t *base_event_chn;
    } ebus_info;
    workqueue_t *workq;
} assistant_view_t;

bool get_audio_listen_status(void);
void assistant_view_userdata_load(void);
assistant_view_t *assistant_view_init(assistant_view_cbs_t *cbs);

// 显示拍照图片到页面
int assistant_view_show_camera_image(const uint16_t *rgb565_data, uint32_t width, uint32_t height);

// 显示网络下载的图片（img_dsc 指向 lv_img_dsc_t）
int assistant_view_show_net_image(const void *img_dsc);

// 隐藏拍照图片
int assistant_view_hide_camera_image(void);

int assistant_view_show_loading(const char *text);

// 通知交互模式变更
int assistant_view_notify_interactive_mode_update(lisa_aiui_interactive_mode_e mode);

// 通知闹钟状态变更
int assistant_view_notify_alarm_update(bool has_alarm);

/**
 * @brief 获取电池图标显示状态
 *
 * 此函数用于查询当前是否应该显示电池图标。
 * 当ADC采样电压低于500mV时，认为电池未连接或电压异常，
 * 不应显示电池图标，以避免误导用户。
 *
 * @return true  - 应该显示电池图标（ADC电压 >= 500mV）
 * @return false - 不应显示电池图标（ADC电压 < 500mV，可能电池未连接）
 */
bool get_show_battery_status(void);
// 更新并显示二维码
int assistant_view_update_qrcode(const char *url, const char *message, const char *err_code);
/**
 * @brief 获取电池图标显示状态
 *
 * 此函数用于查询当前是否应该显示电池图标。
 * 当ADC采样电压低于500mV时，认为电池未连接或电压异常，
 * 不应显示电池图标，以避免误导用户。
 *
 * @return true  - 应该显示电池图标（ADC电压 >= 500mV）
 * @return false - 不应显示电池图标（ADC电压 < 500mV，可能电池未连接）
 */
bool get_show_battery_status(void);

#ifdef __cplusplus
}
#endif

#endif /* assist_view.h */
