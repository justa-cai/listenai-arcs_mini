/**
****************************************************************************************
*
* @file netcfg.c
*
* @brief net config
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "netcfg_ble.h"
#include "netcfg_bles.h"

#include "ble_gap.h"

#include "ble_gatt.h"
#include "ble_prf.h"
#include "lisa_ble_api.h"

#include "nvds_tag_def.h"

#include "btos_al.h"

#define BLE_AUTO_SEND_NET_CFG_SUCESS    (1)

#if !CONFIG_LISA_BLUETOOTH_BUILD_IPC_SERVER
static lisa_ble_netcfg_handler_t s_netcfg_handler;
#else
/* Forward WiFi credentials to the peer image that owns the netcfg handler. */
extern int32_t netcfg_wifi_connect_ipc(const char *ssid, const char *pwd);
#endif

extern void HAL_PMU_Chip_Software_Reset_Enable(void);

#if (BLE_AUTO_SEND_NET_CFG_SUCESS == 1)
static void netcfg_bles_send_connect_status_dummy(uint32_t milli_seconds);
#endif

static void netcfg_ble_wifi_cfg_store(const int8_t *ssid, const int8_t *pwd, const uint8_t *auto_conn)
{
#if CFG_NVS
    nvds_put(NVDS_TAG_WIFI_STA_SSID, NVDS_LEN_WIFI_STA_SSID, ssid);
    if (strlen((const char *)pwd)) {
        nvds_put(NVDS_TAG_WIFI_STA_PWD, NVDS_LEN_WIFI_STA_PWD, pwd);
    } else {
        nvds_del(NVDS_TAG_WIFI_STA_PWD);
    }
    nvds_put(NVDS_TAG_WIFI_STA_AUTOCONN, NVDS_LEN_WIFI_STA_AUTOCONN, auto_conn);
#endif
}

void lisa_ble_netcfg_set_handler(lisa_ble_netcfg_handler_t handler)
{
#if !CONFIG_LISA_BLUETOOTH_BUILD_IPC_SERVER
    /* Local Bluetooth role: keep the handler in this image. */
    s_netcfg_handler = handler;
#else
    /* IPC receiver role forwards WiFi credentials to the peer image. */
    (void)handler;
#endif
}

static int netcfg_ble_wifi_connect(const int8_t *ssid, const int8_t *pwd)
{
    if (!ssid || !pwd)
        return -1;

#if CONFIG_LISA_BLUETOOTH_BUILD_IPC_SERVER
    /* The peer image owns the WiFi netcfg handler. */
    return (int)netcfg_wifi_connect_ipc((const char *)ssid, (const char *)pwd);
#else
    /* Local Bluetooth role handles WiFi connect in this image. */
    if (!s_netcfg_handler)
        return -1;

    return s_netcfg_handler((const char *)ssid, (const char *)pwd);
#endif
}

void netcfg_bles_con_cleanup(uint8_t conidx, uint16_t reason)
{
    if (netcfg_bles_get_state() == NETCFG_BLE_REBOOTING)
        HAL_PMU_Chip_Software_Reset_Enable();
    else
        netcfg_bles_set_state(NETCFG_BLE_IDLE);
}

uint16_t netcfg_ble_notify_wifi(struct netcfg_ble_data *data)
{
    int status = 0;
    uint8_t auto_conn = 1;

#if NETCFG_BLE_DBG
    NETCFG_BLE_LOGD("[%s]: Get ssid = %s, pwd = %s\n", __func__, data->ssid, data->pwd);
#endif

    /* TODO: add task for wifi/ble status exchange instead of synchronous call */
    status = netcfg_ble_wifi_connect((const int8_t *)data->ssid, (const int8_t *)data->pwd);
    if (status)
        return NETCFG_BLE_ERR;

    netcfg_ble_wifi_cfg_store((const int8_t *)data->ssid, (const int8_t *)data->pwd, &auto_conn);

    return NETCFG_BLE_SUCCESS;
}

uint16_t netcfg_bles_profile_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value)
{
    uint16_t sta = NETCFG_BLE_ERR;

    switch (op) {
    case NETCFG_BLE_OP_DONE:
        sta = netcfg_ble_notify_wifi((struct netcfg_ble_data *)p_value);
#if (BLE_AUTO_SEND_NET_CFG_SUCESS == 1)
        netcfg_bles_send_connect_status_dummy(1000);
#endif
        break;
    case NETCFG_BLE_OP_REBOOT:
        ble_gap_disconnect(conidx, 0x13);
        break;
    default:
        break;
    }
    return sta;
}

#if (BLE_AUTO_SEND_NET_CFG_SUCESS == 1)
static void netcfg_bles_send_connect_status_cb(TimerHandle_t time_id)
{
    lisa_ble_netcfg_send_notify(0, 0, 0, 0, NULL);
    btos_timer_cancel(time_id);
}

static void netcfg_bles_send_connect_status_dummy(uint32_t milli_seconds)
{
    btos_timer_creat(TIMER_TYPE_SINGLE, milli_seconds, netcfg_bles_send_connect_status_cb);
}
#endif
