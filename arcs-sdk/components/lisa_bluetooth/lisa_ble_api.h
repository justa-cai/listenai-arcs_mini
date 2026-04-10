/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file lisa_ble_api.h
 * @brief Unified BLE application API.
 *        Works transparently on AP core (local), CP core (IPC), or single-core.
 */

#ifndef __LISA_BLE_API_H__
#define __LISA_BLE_API_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief BLE advertising type
 */
enum lisa_ble_adv_type {
    LISA_BLE_ADV_GEN = 0,
    LISA_BLE_ADV_GEN_PAIRED,
    LISA_BLE_ADV_DIR,
    LISA_BLE_ADV_DIR_HDC,
};

/**
 * @brief BLE address type
 */
typedef struct {
    uint8_t addr[6];
    uint8_t addr_type;
} lisa_ble_addr_t;

/**
 * @brief Start BLE advertising
 *
 * @param adv_id   Advertising set ID
 * @param adv_type Advertising type @see enum lisa_ble_adv_type
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_adv_start(uint8_t adv_id, uint8_t adv_type);

/**
 * @brief Stop BLE advertising
 *
 * @param adv_id Advertising set ID
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_adv_stop(uint8_t adv_id);

/**
 * @brief Start BLE scan with default parameters
 *
 * @param scan_id Scan ID
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_scan_start(uint8_t scan_id);

/**
 * @brief Stop BLE scan
 *
 * @param scan_id Scan ID
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_scan_stop(uint8_t scan_id);

/**
 * @brief Set BLE scan parameters
 *
 * @param type      Scan type
 * @param phy       Scan PHY
 * @param scan_intv Scan interval
 * @param scan_win  Scan window
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_scan_param(uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win);

/**
 * @brief Initiate BLE connection
 *
 * @param addr          Peer address
 * @param phy           Connection PHY
 * @param conn_intv_min Minimum connection interval
 * @param conn_intv_max Maximum connection interval
 * @param conn_latency  Connection latency
 * @param conn_super_to Supervision timeout
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_connect(lisa_ble_addr_t *addr, uint8_t phy,
                        uint16_t conn_intv_min, uint16_t conn_intv_max,
                        uint16_t conn_latency, uint16_t conn_super_to);

/**
 * @brief Disconnect BLE connection
 *
 * @param conidx Connection index
 * @param reason Disconnect reason
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_disconnect(uint8_t conidx, uint8_t reason);

/**
 * @brief Update BLE connection parameters
 *
 * @param conidx       Connection index
 * @param conn_intv_min Minimum connection interval
 * @param conn_intv_max Maximum connection interval
 * @param latency      Connection latency
 * @param super_to     Supervision timeout
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_conn_update(uint8_t conidx,
                            uint16_t conn_intv_min, uint16_t conn_intv_max,
                            uint16_t latency, uint16_t super_to);

/**
 * @brief Send HID report via HOGP profile
 *
 * @param conidx     Connection index
 * @param report_idx HID report index
 * @param length     Data length
 * @param value      Data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t *value);

/**
 * @brief Send voice data via BLE
 *
 * @param conidx     Connection index
 * @param report_idx Report index
 * @param length     Data length
 * @param value      Data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t *value);

/**
 * @brief Send BLE net config notification
 *
 * @param conidx Connection index
 * @param op     Operation code
 * @param state  Status
 * @param length Data length
 * @param value  Data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_ble_netcfg_send_notify(uint8_t conidx, uint8_t op, uint8_t state, uint8_t length, uint8_t *value);

/**
 * @brief Callback for handling WiFi credentials received via BLE netcfg.
 *
 * @param ssid WiFi SSID (null-terminated, max 36 bytes)
 * @param pwd  WiFi password (null-terminated, max 64 bytes)
 * @return 0 on success, non-zero on failure
 */
typedef int (*lisa_ble_netcfg_handler_t)(const char *ssid, const char *pwd);

/**
 * @brief Register handler for BLE network config WiFi connect events.
 *
 * When BLE netcfg profile receives WiFi credentials from a peer device,
 * the registered handler will be called instead of directly invoking
 * WiFi stack APIs. This decouples lisa_bluetooth from WiFi dependencies.
 *
 * @param handler WiFi connect handler, or NULL to clear
 */
void lisa_ble_netcfg_set_handler(lisa_ble_netcfg_handler_t handler);

#endif /* __LISA_BLE_API_H__ */
