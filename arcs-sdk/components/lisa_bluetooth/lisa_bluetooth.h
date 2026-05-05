/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LISA_BLUETOOTH_H__
#define __LISA_BLUETOOTH_H__

#include <stdint.h>
#include <stdbool.h>
#include <bt_stack_hal.h>

#define MAX_DISCOVERED_DEVICES 10

/// BT Discovery Types
typedef enum
{
    /// General discovery
    GAPM_DISC_TYPE_GEN_DISC = 0,
    /// Limited discovery
    GAPM_DISC_TYPE_LIM_DISC,
} gapm_disc_type_e;

/// Bluetooth device discovery information
typedef struct {
    gap_bdaddr_t addr;          //< Device address
    uint16_t clk_off;           //< Clock offset
    int8_t rssi;                //< RSSI value
    uint8_t mode;               //< Discovery mode
    uint32_t cod;               //< Class of device
    uint8_t name_len;           //< Device name length
    uint8_t name[248];          //< Device name
} lisa_bt_discovery_info_t;

/**
 * @brief Bluetooth device discovery callback function type
 * @param info Device discovery information
 */
typedef void (*lisa_bt_discovery_callback_t)(const lisa_bt_discovery_info_t *info);

/**
 * @brief BLE stack enable complete callback type.
 *        Called after BLE stack initialization and built-in profiles are ready.
 *        User should register custom GATT services in this callback.
 *
 * @param status 0 on success, non-zero on failure
 */
typedef void (*lisa_bluetooth_enable_cmp_cb_t)(uint16_t status);

/**
 * @brief BLE connection indication callback type.
 *        Called when a BLE device connects.
 *
 * @param conidx    Connection index
 * @param conhdl    Connection handle
 * @param peer_addr Peer device address
 */
typedef void (*lisa_ble_conn_cb_t)(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr);

/**
 * @brief BLE disconnection indication callback type.
 *        Called when a BLE device disconnects.
 *
 * @param conidx Connection index
 * @param conhdl Connection handle
 * @param reason HCI disconnect reason code
 */
typedef void (*lisa_ble_disc_cb_t)(uint8_t conidx, uint16_t conhdl, uint16_t reason);

/**
 * @brief BLE bond indication callback type.
 *        Called when a BLE pairing/bonding event occurs.
 *
 * @param conidx Connection index
 * @param info   Bond event type (high byte of raw status from stack):
 *               GAP_PAIRING_SUCCEED(0), GAP_PAIRING_FAILED(1),
 *               GAP_LINK_ENCRYPTED(10), GAP_LINK_ENCRYPT_REQ(11), etc.
 * @param value  Event-specific value (low byte of raw status)
 */
typedef void (*lisa_ble_bond_cb_t)(uint8_t conidx, uint8_t info, uint8_t value);

/**
 * @brief BLE passkey/key request callback type.
 *        Called when the BLE stack requests a passkey during pairing.
 *        If no callback is registered, a default passkey (123456) is used.
 *
 * @param conidx  Connection index
 * @param key_type Key type requested by the stack
 * @param passkey  Passkey value proposed by the stack (may be 0)
 */
typedef void (*lisa_ble_key_req_cb_t)(uint8_t conidx, uint8_t key_type, uint32_t passkey);

/**
 * @brief Initialize bluetooth stack.
 *
 * @param cb  Optional callback invoked when BLE stack initialization completes.
 *            This is the proper place to register custom GATT services.
 *            Pass NULL if no callback is needed.
 * @return 0 on success, non-zero on failure
 */
int lisa_bluetooth_init(lisa_bluetooth_enable_cmp_cb_t cb);

/**
 * @brief Notify user that BLE stack initialization is complete.
 *        Used internally by the BT stack. Invokes the callback registered via lisa_bluetooth_init().
 *
 * @param status 0 on success, non-zero on failure
 */
void lisa_ble_notify_enable_cmp(uint16_t status);

/**
 * @brief Notify user of a BLE connection event.
 *        Used internally by bt_ble_user.c to forward connection indications.
 *
 * @param conidx    Connection index
 * @param conhdl    Connection handle
 * @param peer_addr Peer device address
 */
void lisa_ble_notify_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr);

/**
 * @brief Notify user of a BLE disconnection event.
 *        Used internally by bt_ble_user.c to forward disconnection indications.
 *
 * @param conidx Connection index
 * @param conhdl Connection handle
 * @param reason HCI disconnect reason code
 */
void lisa_ble_notify_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason);

/**
 * @brief Notify user of a BLE bond/pairing event.
 *        Used internally by bt_ble_user.c to forward bond indications.
 *
 * @param conidx Connection index
 * @param info   Bond event type (high byte of raw status)
 * @param value  Event-specific value (low byte of raw status)
 */
void lisa_ble_notify_bond(uint8_t conidx, uint8_t info, uint8_t value);

/**
 * @brief Notify user of a BLE passkey request.
 *        Used internally by bt_ble_user.c to forward key requests.
 *        If no user callback is registered, returns false so the caller
 *        can apply a default passkey.
 *
 * @param conidx  Connection index
 * @param key_type Key type
 * @param passkey  Passkey value from stack
 * @return true if user callback was invoked, false if no callback registered
 */
bool lisa_ble_notify_key_req(uint8_t conidx, uint8_t key_type, uint32_t passkey);

int lisa_bluetooth_inquiry_start(gapm_disc_type_e mode, uint8_t max_count);

/**
 * @brief Register bluetooth device discovery callback
 * @param callback Callback function pointer, set to NULL to unregister
 */
void lisa_bluetooth_register_discovery_callback(lisa_bt_discovery_callback_t callback);

/**
 * @brief Connect to a bluetooth device by name
 * @param name Bluetooth device name
 * @return 0 on success, -1 if device not found, -2 on invalid parameter
 */
int lisa_bluetooth_connect_by_name(const char *name);

/**
 * @brief Connect to a bluetooth device by index in the discovered device list
 * @param index Index of the device in the discovered device list
 * @return 0 on success, -1 if device not found, -2 on invalid parameter
 */
int lisa_bluetooth_connect_by_index(uint8_t index);

/**
 * @brief Get the discovered device list
 * @param list Pointer to receive the device list
 * @param count Pointer to receive the device count
 * @return 0 on success, -1 on error
 */
int lisa_bluetooth_get_discovered_devices(const lisa_bt_discovery_info_t **list, uint8_t *count);

/**
 * @brief Clear the discovered device list
 */
void lisa_bluetooth_clear_discovered_devices(void);

/**
 * @brief Register BLE connection callback
 * @param cb Callback function pointer, set to NULL to unregister
 */
void lisa_ble_register_conn_cb(lisa_ble_conn_cb_t cb);

/**
 * @brief Register BLE disconnection callback
 * @param cb Callback function pointer, set to NULL to unregister
 */
void lisa_ble_register_disc_cb(lisa_ble_disc_cb_t cb);

/**
 * @brief Register BLE bond indication callback
 * @param cb Callback function pointer, set to NULL to unregister
 */
void lisa_ble_register_bond_cb(lisa_ble_bond_cb_t cb);

/**
 * @brief Register BLE passkey request callback
 * @param cb Callback function pointer, set to NULL to unregister.
 *           When NULL, the stack will use a default passkey (123456).
 */
void lisa_ble_register_key_req_cb(lisa_ble_key_req_cb_t cb);

/**
 * @brief Confirm a BLE passkey during pairing.
 *        Should be called from the key_req callback to provide the passkey.
 *
 * @param conidx  Connection index
 * @param accept  1 to accept, 0 to reject
 * @param passkey Passkey value (6-digit decimal, max 999999)
 */
void lisa_ble_key_confirm(uint8_t conidx, uint8_t accept, uint32_t passkey);

/* ======== BT Classic callbacks ======== */

/**
 * @brief BT Classic connection indication callback type.
 */
typedef void (*lisa_bt_classic_conn_cb_t)(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr);

/**
 * @brief BT Classic disconnection indication callback type.
 */
typedef void (*lisa_bt_classic_disc_cb_t)(uint8_t conidx, uint16_t conhdl, uint16_t reason);

/**
 * @brief BT Classic AVRCP key press callback type.
 * @param conidx Connection index
 * @param key_id AVRCP key ID (BT_AVRCP_KEY_ID_xxx)
 */
typedef void (*lisa_bt_classic_avrcp_cb_t)(uint8_t conidx, uint8_t key_id);

/**
 * @brief Register BT Classic connection callback
 */
void lisa_bt_classic_register_conn_cb(lisa_bt_classic_conn_cb_t cb);

/**
 * @brief Register BT Classic disconnection callback
 */
void lisa_bt_classic_register_disc_cb(lisa_bt_classic_disc_cb_t cb);

/**
 * @brief Register BT Classic AVRCP key press callback
 */
void lisa_bt_classic_register_avrcp_cb(lisa_bt_classic_avrcp_cb_t cb);

/**
 * @brief BT profile types
 */
enum lisa_bt_profile {
    LISA_BT_PROFILE_A2DP = 0,
    LISA_BT_PROFILE_HFP  = 1,
};

/**
 * @brief BT Classic profile connection/disconnection callback type.
 * @param conidx    Connection index
 * @param profile   Profile type (LISA_BT_PROFILE_A2DP or LISA_BT_PROFILE_HFP)
 * @param connected true=connected, false=disconnected
 */
typedef void (*lisa_bt_classic_profile_cb_t)(uint8_t conidx, int profile, bool connected);

/**
 * @brief Register BT Classic profile connection callback
 */
void lisa_bt_classic_register_profile_cb(lisa_bt_classic_profile_cb_t cb);

/* Internal notify functions (called from bt_classic_user.c) */
void lisa_bt_classic_notify_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr);
void lisa_bt_classic_notify_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason);
void lisa_bt_classic_notify_avrcp_key(uint8_t conidx, uint8_t key_id);
void lisa_bt_classic_notify_profile(uint8_t conidx, int profile, bool connected);

#endif /* __LISA_BLUETOOTH_H__ */