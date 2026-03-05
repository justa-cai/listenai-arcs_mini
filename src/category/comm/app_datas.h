#ifndef __VOICE_APP_DATAS_H__
#define __VOICE_APP_DATAS_H__

enum {
    VOICE_WORK_MODE_VOICE_WAKEUP = (1 << 0),
    VOICE_WORK_MODE_BUTTON_WAKEUP = (1 << 1),
};

enum {
    DEVICE_MODE_PROD = 0,
    DEVICE_MODE_STAGING = 1,
    DEVICE_MODE_INTEGRATION = 2,
};

struct app_datas {
    char pid[64];                    /* 产品ID */
    char sid[64];                    /* 产品密钥 */
    char did[64];                    /* 设备ID */
    uint8_t wifi_connected;          /* 是否已经连接到WiFi */
    uint8_t network_connected;       /* 是否已经连接到网络 */
    uint8_t voice_cloud_connected;   /* 是否已经连接到云端 */
    uint8_t auth_failed;
    uint8_t full_duplex;             /* 是否开启全双工 */
    uint32_t full_duplex_timeout_ms; /* 全双工超时时间 */
    uint8_t device_mode;            /* 使用 正式/测试/研发 环境 */
    uint8_t can_wakeup;
    uint8_t voice_work_mode; /* BIT0: 支持语音唤醒, BIT1: 支持按鍵喚醒 */
    uint8_t oneshot;
    char host[128];
    char host_staging[128];
    char host_integration[128];
    char token_url[128];
    char token_url_staging[128];
    char token_url_integration[128];
    char music_active_url[128];
    char music_tranlink_url[128];
    char wakeup_prompt[64];
    char port[16];
    char scheme[16];
} __attribute__((packed));

struct app_datas *get_app_datas(void);
int app_datas_init(void);

#endif
