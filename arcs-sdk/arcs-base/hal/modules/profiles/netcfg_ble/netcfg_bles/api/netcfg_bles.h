/**
****************************************************************************************
*
* @file netcfg_bles.h
*
* @brief NETCFG_BLE Service header
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

#ifndef SRC_BT_PROFILES_NETCFG_BLES_H_
#define SRC_BT_PROFILES_NETCFG_BLES_H_

#include "netcfg_ble.h"
#include "ble_prf.h"

#define NETCFG_BLE_DATA_MAX_LEN            (255)

/// NETCFG_BLE BLE GATT UUIDs
#define BLE_GATT_NETCFG_BLE_SERVICE     (0xe402)
#define BLE_GATT_NETCFG_BLE_DATA_BUFF   (0xe403)
#define BLE_GATT_NETCFG_BLE_STATS       (0xe404)


/// NETCFG_BLE Service Attributes Index
enum
{
    /// service
    NETCFG_BLE_IDX_SVC,

    /// data buffer
    NETCFG_BLE_IDX_DATA_BUFF_CHAR,
    NETCFG_BLE_IDX_DATA_BUFF_VAL,
    NETCFG_BLE_IDX_DATA_BUFF_NTF_CFG,

    /// NETCFG_BLE Status
    NETCFG_BLE_IDX_NETCFG_BLE_STATS_CHAR,
    NETCFG_BLE_IDX_NETCFG_BLE_STATS_VAL,

    NETCFG_BLE_IDX_NB,
};

/*
 * TYPES DEFINITIONS
 ****************************************************************************************
 */
/// DIS server callback set
typedef struct netcfg_bles_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user write data to peer.
     ****************************************************************************************
     */
    uint16_t (*cb_value_set) (uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value);
} netcfg_bles_cb_t;

/// netcfg_ble service environment variable
typedef struct netcfg_bles_env
{
    netcfg_bles_cb_t* p_cb;

    /// service state
    uint16_t state;
    /// GATT user local identifier
    uint8_t user_lid;
    /// HIDS Start Handles
    uint16_t start_hdl;
    /// Notification configuration
    uint16_t ntf_cfg[BLE_CONNECTION_MAX];

    /// netcfg_ble data buffer
    struct netcfg_ble_data data;
} netcfg_bles_env_t;

uint16_t netcfg_bles_get_state(void);
void netcfg_bles_set_state(uint16_t state);
uint16_t netcfg_bles_init(uint8_t sec_lvl, uint8_t user_prio, netcfg_bles_cb_t *p_cb);
uint16_t ble_netcfg_bles_init(netcfg_bles_cb_t *p_cb);
uint16_t ble_netcfg_bles_send_notify(uint8_t conidx, uint16_t item, uint16_t status);
uint16_t ble_netcfg_bles_send_notify_custom_data(uint8_t conidx, uint16_t length, uint8_t *value);

#endif /* SRC_BT_PROFILES_NETCFG_BLES_H_ */
