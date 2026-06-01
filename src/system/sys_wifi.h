#pragma once

#include <stdbool.h>

typedef enum {
    SYS_WIFI_CONNECT_RESULT_SUCCESS       = 0x00,
    SYS_WIFI_CONNECT_RESULT_FAIL_NO_AP    = 0x01,
    SYS_WIFI_CONNECT_RESULT_FAIL_PASSWORD = 0x02,
    SYS_WIFI_CONNECT_RESULT_FAIL_IP       = 0x03,
    SYS_WIFI_CONNECT_RESULT_FAIL_OTHER    = 0xFF,
} sys_wifi_connect_result_t;

#ifdef __cplusplus
extern "C" {
#endif

int sys_wifi_init(void);
int sys_wifi_start(bool autoconnect);
int sys_wifi_stop(void);
int sys_wifi_connect(const char *ssid, const char *pwd, const char *bssid);
sys_wifi_connect_result_t sys_wifi_get_connect_result(void);
int sys_wifi_save_ap(const char *ssid, const char *pwd, const char *bssid);
int sys_wifi_clear_saved_aps(void);
bool sys_wifi_get_signal_quality(int *rssi);
bool sys_wifi_is_ready(void);
bool sys_wifi_is_started(void);
bool sys_wifi_is_connected(void);
bool sys_wifi_has_ap(void);

#ifdef __cplusplus
}
#endif
