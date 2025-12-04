#ifndef __LISAUI_USER_DATA_H__
#define __LISAUI_USER_DATA_H__

#include <stdint.h>
#include <stdbool.h>
#include "lisaui_videoqueue.h"
#include "ebus/ebus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_USERDATA_INTER_ROLE_PRIMARY "小仙"

#define LISAUI_USERDATA_DVP_CAMERA_VIDEO_ATTRIBUTES (0)
#define LISAUI_USERDATA_DVP_CAMERA_VIDEO_WIDTH      (320)
#define LISAUI_USERDATA_DVP_CAMERA_VIDEO_HEIGHT     (240)
#define LISAUI_USERDATA_DVP_CAMERA_VIDEO_SIZE                                                                          \
    (LISAUI_USERDATA_DVP_CAMERA_VIDEO_WIDTH * LISAUI_USERDATA_DVP_CAMERA_VIDEO_HEIGHT * 2)
#define LISAUI_USERDATA_DVP_CAMERA_VIDEO_BUFFER_COUNT (16)

#define LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH 64

#define LISAUI_EBUS_NAME                "lisaui_bus"
#define LISAUI_EBUS_CH_BASE_EVENT_NAME   "ch.base.event"


typedef enum {
    LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE = EBUS_EVENT_CODE_RESERVED+1,
    LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE,
    LISAUI_EBUS_CH_EVENT_M2U_SETTING_BATTERY_UPDATE,
    LISAUI_EBUS_CH_EVENT_M2U_ROLES_UPDATE,
    LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE,
    LISAUI_EBUS_CH_EVENT_M2U_MCP_EMOJI_UPDATE,
    LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP,
    LISAUI_EBUS_CH_EVENT_M2U_INTER_END,
    LISAUI_EBUS_CH_EVENT_M2U_STANDBY_TEXTS_UPDATE,  // 待机文本更新事件
    LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_UPDATE,
    LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_EXIT,
    LISAUI_EBUS_CH_EVENT_U2M_WIFI_CONNECT,
    LISAUI_EBUS_CH_EVENT_U2M_WIFI_DISCONNECT,
    LISAUI_EBUS_CH_EVENT_U2M_WIFI_SCAN,
    LISAUI_EBUS_CH_EVENT_U2M_WIFI_DISABLE,
    LISAUI_EBUS_CH_EVENT_U2M_WIFI_ENABLE,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_VOL_UPDATE,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_LIGHT_UPDATE,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_INTER_MODE_UPDATE,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_CTR_VIDEO_START,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_CTR_VIDEO_STOP,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_TAKE_PHOTO,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_VIDEO_EXIT,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_GAIN_UPDATE,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_MUTE_UPDATE,
    LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE,
    LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE,
    LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_SHOW,
    LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_HIDE,

} lisaui_ebus_ch_event_e;

typedef struct {
    lisaui_ebus_ch_event_e event;
} lisaui_ebus_userdata_message_t;

// 拍照图片参数结构
typedef struct {
    const uint16_t *rgb565_data;  // RGB565图片数据指针
    uint32_t width;                // 图片宽度
    uint32_t height;               // 图片高度
} lisaui_camera_image_params_t;

typedef enum {

    LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE,
    LISAUI_USERDATA_INTER_REMOTE_STATE_LISTENING,
    LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING,
    LISAUI_USERDATA_INTER_REMOTE_STATE_TALKING,

} lisaui_userdata_inter_remote_state_e;

typedef enum {
    LISAUI_USERDATA_TEXT_MODE_APPEND = 0,         // 追加
    LISAUI_USERDATA_TEXT_MODE_OVERWRITE,          // 覆盖
    LISAUI_USERDATA_TEXT_MODE_NONE = 0,           // 无
} lisaui_userdata_text_mode_e;

typedef enum {
    LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE,
    LISAUI_USERDATA_INTER_LOCAL_STATE_WAKEUP,
    LISAUI_USERDATA_INTER_LOCAL_STATE_RECOGNITION,

} lisaui_userdata_inter_local_state_e;

typedef enum{
    ROLE_EMOJI_UNKNOW = 0,
    ROLE_EMOJI_LOVE,
    ROLE_EMOJI_SAD,
    ROLE_EMOJI_LAUGH,
    ROLE_EMOJI_SQUINT,
    ROLE_EMOJI_ANGRY,
    ROLE_EMOJI_EYE,
    ROLE_EMOJI_BLINK,
    ROLE_EMOJI_HAPPY,
    ROLE_EMOJI_CUTE,
    ROLE_EMOJI_MAX,
}role_emoji_e;

typedef struct {
    char *name;
    char *model;
    void *img_main;
    void *img_commu;
    uint8_t is_emoji;
    role_emoji_e emoji;
    char *prompt;
} role_info_t;

typedef struct {
    role_info_t *roles;
    int roles_count;
    int role_idx;
    int request_state;/*0:unknown,1:success,2:failed*/
} roles_t;

typedef struct {
    lisaui_userdata_inter_remote_state_e remote_state;
    lisaui_userdata_inter_local_state_e local_state;
    char *iat_text;
    char *reply_text;
} lisaui_userdata_inter_info_t;

typedef enum {

    LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTING,
    LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTED,
    LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECT_FAILED,
    LISAUI_USERDATA_WIFI_CONNECT_STATE_DISCONNECTED,

} lisaui_userdata_setting_wifi_connect_state_e;

typedef struct {
    char ssid[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH + 1];  /* Target AP SSID */
    char bssid[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH + 1]; /* Target AP bssid (MAC address) */
    char pwd[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH + 1];
} lisaui_userdata_setting_wifi_connect_cfg_t;

typedef struct {
    lisaui_userdata_setting_wifi_connect_state_e state;
    lisaui_userdata_setting_wifi_connect_cfg_t cfg;
} lisaui_userdata_setting_wifi_connect_info_t;

typedef enum {
    LISAUI_USERDATA_WIFI_SCAN_STATE_SCANNING,
    LISAUI_USERDATA_WIFI_SCAN_STATE_SCANNED,

} lisaui_userdata_setting_wifi_scan_state_e;
typedef enum {
    LISAUI_USERDATA_WIFI_HOTSPOT_STATE_WITHOUT_CONFIG,
    LISAUI_USERDATA_WIFI_HOTSPOT_STATE_WITH_CONFIG,
    LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTING,
    LISAUI_USERDATA_WIFI_HOTSPOT_STATE_CONNECTED,

} lisaui_userdata_setting_wifi_hotspot_state_t;

typedef struct {

    char ssid[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH + 1];                /* Target AP SSID */
    char bssid[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH + 1];               /* Target AP bssid (MAC address) */
    char encryption_mode_str[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH + 1]; /* Target AP encryption mode */
    char pwd[LISAUI_USERDATA_WIFI_STRING_MAX_LENGTH];
    int rssi;        /* Neighboring AP rssi */
    uint8_t channel; /* state */
    lisaui_userdata_setting_wifi_hotspot_state_t state;
} lisaui_userdata_setting_wifi_hotspot_t;

typedef struct {

    lisaui_userdata_setting_wifi_scan_state_e state;
    int number;
    lisaui_userdata_setting_wifi_hotspot_t hotspot[];

} lisaui_userdata_setting_wifi_hotspot_info_t;

typedef struct {
    bool is_enable;

    lisaui_userdata_setting_wifi_hotspot_info_t *p_hotspot_info;
    lisaui_userdata_setting_wifi_connect_info_t connect_info;
} lisaui_userdata_setting_wifi_t;

typedef enum {
    LISAUI_USERDATA_SETTING_INTER_MODE_HALF,
    LISAUI_USERDATA_SETTING_INTER_MODE_DUAL,
    LISAUI_USERDATA_SETTING_INTER_MODE_KEY,
} lisaui_userdata_setting_inter_mode_e;

typedef struct {
    bool is_charging;
    uint8_t usb_status;
    uint8_t power_percent;
} lisaui_userdata_battery_info_t;

typedef struct {
    lisaui_userdata_setting_wifi_t wifi;
    lisaui_userdata_setting_inter_mode_e inter_mode;
    lisaui_userdata_battery_info_t battery;
    int dis_light_percent;
    int volume_percent;
    bool mic_is_mute;
    int mic_gain;/*0-100*/

} lisaui_userdata_setting_t;

typedef struct{
    uint32_t width;
    uint32_t height;
    uint32_t size;
    uint8_t data[0];
} lisaui_userdata_photo_t;

typedef enum {
    LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK,
    LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE,
    LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA,
    LISAUI_USERDATA_QRCODE_INTER_UNKNOW_MODE,
} lisaui_userdata_qrcode_inter_mode_e;

typedef struct {
    lisaui_userdata_qrcode_inter_mode_e mode;
    
    // 网络配置模式的文本和URL
    char *network_label_text;
    char *network_url;
    
    // 设备配置模式的文本和URL
    char *device_label_text;
    char *device_url;
    
    // 配额页面模式的文本和URL
    char *quota_label_text;
    char *quota_url;
} lisaui_userdata_qrcode_inter_t;

// 待机文本轮换配置
#define LISAUI_USERDATA_STANDBY_TEXT_MAX_COUNT 10
#define LISAUI_USERDATA_STANDBY_TEXT_MAX_LENGTH 128

typedef struct {
    bool is_enabled;                    // 是否启用云端待机文本轮换
    uint32_t interval_ms;               // 轮换间隔时间（毫秒）
    uint32_t text_count;                // 文本数量
    char texts[LISAUI_USERDATA_STANDBY_TEXT_MAX_COUNT][LISAUI_USERDATA_STANDBY_TEXT_MAX_LENGTH]; // 文本内容
} lisaui_userdata_standby_texts_t;

typedef struct {
    lisaui_userdata_inter_info_t inter;
    lisaui_userdata_setting_t setting;
    lisaui_videoqueue_t *camera_video;
    roles_t roles;
    lisaui_userdata_photo_t *photo;
    lisaui_userdata_qrcode_inter_t qrcode_inter;
    lisaui_userdata_standby_texts_t standby_texts;  // 待机文本轮换配置
} lisaui_userdata_t;

#define LISAUI_USERDATA_WITH_LOCK(data_ptr)                                                                            \
    for (lisaui_userdata_t *data_ptr = lisaui_userdata_acquire(), *_once = data_ptr; _once;                            \
         lisaui_userdata_release(), _once = NULL)


/**
 * @brief Initialize the user data subsystem
 * 
 * This function initializes all required user data structures and resources.
 * Must be called before any other user data functions are used.
 * 
 * @return 0 on success, negative error code on failure
 */
int lisaui_data_init(void);

/**
 * @brief Acquire access to the user data
 * 
 * This function acquires the mutex for the user data and returns a pointer
 * to the global user data structure. It should be paired with a call to
 * lisaui_userdata_release() when access is no longer needed.
 * 
 * @return Pointer to the user data structure, or NULL if not initialized
 */
lisaui_userdata_t *lisaui_userdata_acquire(void);

/**
 * @brief Release access to the user data
 * 
 * This function releases the mutex for the user data, allowing other threads
 * to access it. It must be called after every successful call to
 * lisaui_userdata_acquire().
 */
void lisaui_userdata_release(void);

/**
 * @brief Get the camera video queue instance
 * 
 * This function provides thread-safe access to the global camera video queue.
 * It's designed to be used by components that need to push or pop video frames
 * from the camera stream.
 * 
 * @return Pointer to the camera video queue, or NULL if not initialized
 */
lisaui_videoqueue_t *lisaui_userdata_get_camera_videoqueue(void);

/**
 * @brief Set standby texts configuration
 * 
 * This function sets the standby texts configuration that will be used for text rotation
 * on the primary page. This is typically called when cloud configuration is received.
 * 
 * @param texts Array of text strings
 * @param count Number of texts in the array
 * @param interval_ms Rotation interval in milliseconds
 * @param enable Whether to enable standby text rotation
 * @return 0 on success, negative error code on failure
 */
int lisaui_userdata_set_standby_texts(const char **texts, uint32_t count, 
                                     uint32_t interval_ms, bool enable);

/**
 * @brief Clear standby texts configuration
 * 
 * This function clears the standby texts configuration and disables text rotation.
 * After calling this, the system will fall back to using role prompt text.
 * 
 * @return 0 on success, negative error code on failure
 */
int lisaui_userdata_clear_standby_texts(void);

#ifdef __cplusplus
}
#endif
#endif /* __LISAUI_USER_DATA_H__ */