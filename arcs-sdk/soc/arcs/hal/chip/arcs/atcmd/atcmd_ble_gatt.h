/**
 ****************************************************************************************
 *
 * @file atcmd_ble_gatt.h
 *
 * @brief BLE GATT Header
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef _ATCMD_BLE_GATT_H_
#define _ATCMD_BLE_GATT_H_

#define BT_STACK_BLE_HOGPD_HID_MAX_COUNT     (20)

void atcmd_ble_gatt_init(void);
void atcmd_ble_gatt_hogpd_reinit(void);
#endif /* _ATCMD_BLE_GATT_H_ */
