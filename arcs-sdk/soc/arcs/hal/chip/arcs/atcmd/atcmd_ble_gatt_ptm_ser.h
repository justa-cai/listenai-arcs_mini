/**
****************************************************************************************
*
* @file atcmd_ble_gatt_ptm_ser.h
*
* @brief BLE Production Test Mode (PTM) GATT Service header
*
* Copyright (C) ListenAI 2020-2099
*
****************************************************************************************
*/

#ifndef ATCMD_BLE_GATT_PTM_SER_H_
#define ATCMD_BLE_GATT_PTM_SER_H_

#include <stdbool.h>
#include <stdint.h>

/// Maximum payload length aligned with default ATT MTU (247 bytes)
#define PTM_DATA_MAX_LEN            (247)

typedef void (*ptm_data_rx_cb_t)(uint8_t conidx, const uint8_t *data, uint16_t length);

uint16_t ptm_init(uint8_t sec_lvl, uint8_t user_prio);
uint16_t ptm_send_notify(uint8_t conidx, const uint8_t *data, uint16_t length);
void ptm_register_rx_callback(ptm_data_rx_cb_t cb);
bool ptm_is_notify_enabled(uint8_t conidx);

#endif /* ATCMD_BLE_GATT_PTM_SER_H_ */

