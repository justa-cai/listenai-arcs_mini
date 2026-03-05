#ifndef __MODEL_WIFI_H__
#define __MODEL_WIFI_H__

#include <stdint.h>
#include <stdbool.h>

#define WIFI_SSID_MAX_LEN    32
#define WIFI_BSSID_MAX_LEN   18
#define WIFI_PWD_MAX_LEN     64
#define WIFI_MAX_SCAN_APS    20

typedef enum {
    MODEL_WIFI_AUTH_AUTO = 0,
    MODEL_WIFI_AUTH_OPEN,
    MODEL_WIFI_AUTH_WEP,
    MODEL_WIFI_AUTH_WPA_PSK,
    MODEL_WIFI_AUTH_WPA2_PSK,
    MODEL_WIFI_AUTH_WPA_WPA2_PSK,
    MODEL_WIFI_AUTH_WPA2_ENTERPRISE,
    MODEL_WIFI_AUTH_WPA3_PSK,
    MODEL_WIFI_AUTH_WPA2_WPA3_PSK,
    MODEL_WIFI_AUTH_UNKNOWN,
    MODEL_WIFI_AUTH_MAX,
} model_wifi_encryption_mode_t;

typedef enum {
    MODEL_WIFI_STATUS_CONNECTED = 0,
    MODEL_WIFI_STATUS_CONNECTING,
    MODEL_WIFI_STATUS_DISCONNECTED,
    MODEL_WIFI_STATUS_UNKNOWN,
} model_wifi_status_t;

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char bssid[WIFI_BSSID_MAX_LEN];
    char pwd[WIFI_PWD_MAX_LEN];
    int channel;
    int rssi;
    model_wifi_encryption_mode_t encryption_mode;
} model_wifi_sta_config_t;

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char bssid[WIFI_BSSID_MAX_LEN];
    int channel;
    int rssi;
    model_wifi_encryption_mode_t encryption_mode;
} model_wifi_scan_info_t;

struct model_wifi_cb {
    void (*on_connected)(model_wifi_sta_config_t *sta_info, void *arg);
    void (*on_disconnected)(int reason, void *arg);
    void (*on_connecting)(void *arg);
    void (*on_connection_failed)(int reason, void *arg);
    void (*on_scan_done)(model_wifi_scan_info_t *aps_info, int ap_num, void *arg);
    void (*on_scan_failed)(int reason, void *arg);
};

int model_wifi_init(void);
int model_wifi_deinit(void);

int model_wifi_cb_register(const struct model_wifi_cb *cb, void *arg);
int model_wifi_cb_unregister(const struct model_wifi_cb *cb);

int model_wifi_scan_start(void);
model_wifi_status_t model_wifi_get_status(void);

int model_wifi_connect(const char *ssid, const char *pwd, const char *bssid);
int model_wifi_disconnect(void);

int model_wifi_get_connected_info(model_wifi_sta_config_t *sta_info);

#endif
