#ifndef __MRPC_UTILS_S2M_API_MSG_H__
#define __MRPC_UTILS_S2M_API_MSG_H__
#include <stdint.h>
#include "ls_err.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_ls_read_temp_voltage_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    float vout;
} mrpc_ls_read_temp_voltage_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_ls_get_mac_from_nvs_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t mac_addr[6];
} mrpc_ls_get_mac_from_nvs_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_ls_get_mac_customized_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint8_t mac_addr[6];
} mrpc_ls_get_mac_customized_resp_t;

/* BLE netcfg WiFi connect: AP forwards WiFi credentials to CP */
#define MRPC_NETCFG_SSID_MAX_LEN    36
#define MRPC_NETCFG_PWD_MAX_LEN     64

typedef struct
{
    struct mrpc_req_msg hdr;
    char ssid[MRPC_NETCFG_SSID_MAX_LEN];
    char pwd[MRPC_NETCFG_PWD_MAX_LEN];
} mrpc_netcfg_wifi_connect_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_netcfg_wifi_connect_resp_t;

#endif //__MRPC_UTILS_S2M_API_MSG_H__