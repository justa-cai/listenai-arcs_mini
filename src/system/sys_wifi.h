#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int sys_wifi_init(void);
int sys_wifi_start(bool autoconnect);
int sys_wifi_stop(void);
int sys_wifi_connect(const char *ssid, const char *pwd, const char *bssid);
int sys_wifi_save_ap(const char *ssid, const char *pwd, const char *bssid);
int sys_wifi_clear_saved_aps(void);
bool sys_wifi_get_signal_quality(int *rssi);
bool sys_wifi_is_ready(void);
bool sys_wifi_is_started(void);
bool sys_wifi_is_connected(void);
bool sys_wifi_get_force_provision(void);
void sys_wifi_set_force_provision(bool enabled);
bool sys_wifi_get_user_force_provision(void);
void sys_wifi_set_user_force_provision(bool enabled);

#ifdef __cplusplus
}
#endif
