#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_NETWORK_MODE_WIFI = 0,
    SYS_NETWORK_MODE_MODEM = 1,
} sys_network_mode_t;

typedef enum {
    SYS_NETWORK_BEARER_NONE = 0,
    SYS_NETWORK_BEARER_WIFI,
    SYS_NETWORK_BEARER_MODEM,
} sys_network_bearer_t;

typedef struct {
    sys_network_mode_t mode;
    sys_network_bearer_t active_bearer;
    bool connected;
    bool switching;
    bool wifi_available;
    bool wifi_connected;
    bool modem_connected;
} sys_network_status_t;

int sys_network_manager_init(bool wifi_available);
int sys_network_request_mode(sys_network_mode_t mode, bool persist);
int sys_network_connect_wifi(const char *ssid, const char *pwd, const char *bssid);
int sys_network_toggle_mode(bool persist);
void sys_network_report_probe_result(bool connected);
int sys_network_get_status(sys_network_status_t *status);
bool sys_network_get_signal_quality(int *rssi, int *ber);

#ifdef __cplusplus
}
#endif
