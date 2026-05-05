#ifndef __VOICE_APP_DATAS_H__
#define __VOICE_APP_DATAS_H__

#include <stdbool.h>
#include <stdint.h>

#include "sys_network_manager.h"

enum {
    VOICE_WORK_MODE_VOICE_WAKEUP = (1 << 0),
    VOICE_WORK_MODE_BUTTON_WAKEUP = (1 << 1),
};

enum {
    DEVICE_MODE_PROD = 0,
    DEVICE_MODE_STAGING = 1,
    DEVICE_MODE_INTEGRATION = 2,
};

typedef enum {
    APP_INTERACTION_MODE_FULL_DUPLEX = 0,
    APP_INTERACTION_MODE_MULTI_NO_INTERRUPT,
    APP_INTERACTION_MODE_SINGLE,
    APP_INTERACTION_MODE_MAX,
} app_interaction_mode_t;

static inline bool app_interaction_mode_is_valid(int mode)
{
    return mode >= APP_INTERACTION_MODE_FULL_DUPLEX && mode < APP_INTERACTION_MODE_MAX;
}

static inline bool app_interaction_mode_is_continuous(int mode)
{
    return mode == APP_INTERACTION_MODE_FULL_DUPLEX || mode == APP_INTERACTION_MODE_MULTI_NO_INTERRUPT;
}

static inline bool app_interaction_mode_supports_barge_in(int mode)
{
    return mode == APP_INTERACTION_MODE_FULL_DUPLEX;
}

struct app_datas {
    char pid[64];                    /* 产品ID */
    char sid[64];                    /* 产品密钥 */
    char did[64];                    /* 设备ID */
    uint8_t wifi_connected;          /* 是否已经连接到WiFi */
    uint8_t modem_connected;         /* 是否已经连接到modem */
    uint8_t network_connected;       /* 是否已经连接到网络 */
    uint8_t voice_cloud_connected;   /* 是否已经连接到云端 */
    uint8_t network_mode;            /* sys_network_mode_t */
    uint8_t active_bearer;           /* sys_network_bearer_t */
    uint8_t network_switching;       /* 网络切换中 */
    uint8_t auth_failed;
    uint8_t int_mode;                /* 交互模式：0全双工可打断/1全双工不可打断/2单工 */
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
