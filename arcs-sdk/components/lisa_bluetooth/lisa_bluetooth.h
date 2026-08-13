/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LISA_BLUETOOTH_H__
#define __LISA_BLUETOOTH_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <bt_stack_hal.h>

#ifndef CONFIG_LISA_BLUETOOTH_MAX_DISCOVERED_DEVICES
#define CONFIG_LISA_BLUETOOTH_MAX_DISCOVERED_DEVICES 10
#endif

#define MAX_DISCOVERED_DEVICES CONFIG_LISA_BLUETOOTH_MAX_DISCOVERED_DEVICES
#define BT_PAIRED_NAME_MAX_LEN 248
#define BT_PAIRED_MAX_COUNT 16

typedef enum {
    BT_PAIRED_TRANSPORT_UNKNOWN = 0,
    BT_PAIRED_TRANSPORT_BLE = 1,
    BT_PAIRED_TRANSPORT_CLASSIC = 2,
} bt_paired_transport_t;

typedef struct {
    gap_bdaddr_t addr;
    uint8_t transport;
    uint8_t name_len;
    uint8_t name[BT_PAIRED_NAME_MAX_LEN];
} bt_paired_info_t;

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
 * @brief BT Classic inquiry stop callback function type
 */
typedef void (*lisa_bt_inquiry_stop_callback_t)(void);

/**
 * @brief BLE stack enable complete callback type.
 *        Called after BLE stack initialization and built-in profiles are ready.
 *        User should register custom GATT services in this callback.
 *
 * @param status 0 on success, non-zero on failure
 */
typedef void (*lisa_bluetooth_enable_cmp_cb_t)(uint16_t status);

/**
 * @brief Bluetooth close complete callback type.
 *
 * @param status 0 on success, non-zero if close was forced by timeout/failure
 */
typedef void (*lisa_bluetooth_close_cmp_cb_t)(uint8_t status);

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
 * @brief Configure local Bluetooth address before stack initialization.
 *
 * @param addr Local address, addr_type 0=public/1=random
 * @return 0 on success, -1 on failure, -2 on invalid parameter, -3 if stack init has started
 */
int lisa_bluetooth_set_local_addr(const gap_bdaddr_t *addr);

/**
 * @brief Initialize the bluetooth stack synchronously.
 *
 *        This creates the BT/BLE tasks and waits up to five seconds for the
 *        configured stack to become ready. A successful return guarantees that
 *        bluetooth APIs can be used immediately. The optional BLE callback is
 *        still invoked from the BT task when BLE initialization completes.
 *
 *        Calling it again updates the BLE callback. While initialization is in
 *        progress it returns -EINPROGRESS; otherwise it returns the cached
 *        success or failure. A timeout does not restart
 *        the stack, and a later completion updates the cached result.
 *
 * @param cb  Optional callback invoked when BLE stack initialization completes.
 *            This is the proper place to register custom GATT services.
 *            Pass NULL if no callback is needed.
 * @return 0 on success, -ENOMEM if the completion object cannot be created,
 *         -EINPROGRESS if another caller is initializing the stack,
 *         -ETIMEDOUT if initialization does not complete in time, or -1 on
 *         other failures
 */
int lisa_bluetooth_init(lisa_bluetooth_enable_cmp_cb_t cb);

/**
 * @brief Open bluetooth APIs after they were closed.
 *
 *        The stack must already be initialized by lisa_bluetooth_init().
 *        Normal reopen only enables classic page scan (BT_GAP_PSCAN_EN) and
 *        marks bluetooth APIs available again. If close is still waiting for
 *        disconnect/inquiry callbacks, open is deferred and completed after
 *        close finishes.
 *
 * @return 0 on success or deferred-open accepted, non-zero on failure
 */
int lisa_bluetooth_open(void);

/**
 * @brief Close bluetooth APIs.
 *
 *        Starts disconnecting the current BLE/BT Classic connection if present,
 *        stops BT Classic inquiry/page scan, and immediately makes public
 *        bluetooth APIs fail. The final closed state is reached asynchronously
 *        after pending disconnect/inquiry callbacks, or after an internal timeout.
 *
 * @return 0 on success, non-zero on failure to enqueue close operations
 */
int lisa_bluetooth_close(void);

/**
 * @brief Register callback invoked after the lower Bluetooth stack is fully closed.
 *
 * @param cb Callback to register, or NULL to unregister
 */
void lisa_bluetooth_register_close_cmp_cb(lisa_bluetooth_close_cmp_cb_t cb);

/**
 * @brief Query whether bluetooth APIs are currently open.
 *
 * @return true if bluetooth APIs are available, false after close
 */
bool lisa_bluetooth_is_opened(void);

/**
 * @brief Notify user that BLE stack initialization is complete.
 *        Used internally by the BT stack. Invokes the callback registered via lisa_bluetooth_init().
 *
 * @param status 0 on success, non-zero on failure
 */
void lisa_ble_notify_enable_cmp(uint16_t status);

/**
 * @brief Notify Lisa Bluetooth that the classic stack is ready after open.
 *        Used internally by bt_classic_user.c after BT_STATE_OPENED.
 *
 * @param status 0 on success, non-zero on failure
 */
void lisa_bluetooth_notify_classic_enabled(uint16_t status);

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
 * @brief Start BT Classic inquiry and stop automatically after timeout.
 *        The controller response limit is MAX_DISCOVERED_DEVICES.
 * @param mode Discovery mode
 * @param timeout_ms Inquiry timeout in milliseconds, must be non-zero
 * @return 0 on success, -1 on failure, -2 on invalid parameter
 */
int lisa_bluetooth_inquiry_start_timed(gapm_disc_type_e mode, uint32_t timeout_ms);

/**
 * @brief Stop BT Classic inquiry
 * @return 0 on success, -1 on failure
 */
int lisa_bluetooth_inquiry_stop(void);

/**
 * @brief Register BT Classic inquiry stop callback
 * @param callback Callback function pointer, set to NULL to unregister
 */
void lisa_bluetooth_register_inquiry_stop_callback(lisa_bt_inquiry_stop_callback_t callback);

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
 * @brief Connect to a bluetooth device by address
 * @param addr Bluetooth device address. For BT Classic, addr_type is usually 0.
 * @return 0 on success, non-zero on failure, -2 on invalid parameter
 */
int lisa_bluetooth_connect_by_addr(const gap_bdaddr_t *addr);

/**
 * @brief Disconnect from a bluetooth device by address
 * @param addr Bluetooth device address
 * @return 0 on success, -1 if device is not connected, -2 on invalid parameter
 */
int lisa_bluetooth_disconnect_by_addr(const gap_bdaddr_t *addr);

/**
 * @brief Disconnect from a bluetooth device by name in the discovered device list
 * @param name Bluetooth device name
 * @return 0 on success, -1 if device not found or not connected, -2 on invalid parameter
 */
int lisa_bluetooth_disconnect_by_name(const char *name);

/**
 * @brief Disconnect from a bluetooth device by index in the discovered device list
 * @param index Index of the device in the discovered device list
 * @return 0 on success, -1 if device is not connected, -2 on invalid parameter
 */
int lisa_bluetooth_disconnect_by_index(uint8_t index);

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
 * @brief Set a fallback display name for the next paired record update.
 * @param addr Bluetooth device address
 * @param name Non-empty display name from the UI/scan result
 * @return 0 on success, -1 on error
 */
int lisa_bluetooth_set_pending_paired_name(const gap_bdaddr_t *addr, const char *name);

/**
 * @brief Get paired Bluetooth devices.
 * @param list Output array, or NULL with max_count 0 to query count only
 * @param max_count Maximum output item count
 * @param out_count Actual paired device count
 * @return 0 on success, -EINVAL on invalid parameter, -ENOSPC if list is too small
 */
int bt_paired_list_get(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count);

/**
 * @brief Get paired device name by address.
 * @param addr Paired device address
 * @param name Output string buffer
 * @param name_len Output string buffer length
 * @return 0 on success, -ENOENT if not paired, -ENODATA if name is unavailable
 */
int bt_paired_name_get(const gap_bdaddr_t *addr, char *name, size_t name_len);

/**
 * @brief Remove paired device. Passing NULL removes all pairing records.
 * @param addr Paired device address, or NULL to remove all
 * @return 0 on success, negative errno-style value on error
 */
int bt_paired_remove(const gap_bdaddr_t *addr);

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
 * @brief BT Classic connection failure callback type.
 */
typedef void (*lisa_bt_classic_conn_fail_cb_t)(uint8_t actv_id, int16_t status);

/**
 * @brief BT Classic link authentication failure callback type.
 */
typedef void (*lisa_bt_classic_link_auth_fail_cb_t)(uint8_t conidx, uint8_t reason);

/**
 * @brief BT Classic AVRCP key press callback type.
 * @param conidx Connection index
 * @param key_id AVRCP key ID (BT_AVRCP_KEY_ID_xxx)
 */
typedef void (*lisa_bt_classic_avrcp_cb_t)(uint8_t conidx, uint8_t key_id);

/**
 * @brief BT Classic AVRCP notify indication callback type.
 * @param conidx Connection index
 * @param c_r Command/response flag from AVRCP notify indication
 * @param event_id AVRCP notify event ID (BT_AVRCP_NOTIFI_xxx)
 * @param event_value AVRCP notify event value
 */
typedef void (*lisa_bt_classic_avrcp_notify_cb_t)(uint8_t conidx, uint8_t c_r, uint8_t event_id, uint8_t event_value);

/**
 * @brief Register BT Classic connection callback
 */
void lisa_bt_classic_register_conn_cb(lisa_bt_classic_conn_cb_t cb);

/**
 * @brief Register BT Classic disconnection callback
 */
void lisa_bt_classic_register_disc_cb(lisa_bt_classic_disc_cb_t cb);

/**
 * @brief Register BT Classic connection failure callback
 */
void lisa_bt_classic_register_conn_fail_cb(lisa_bt_classic_conn_fail_cb_t cb);

/**
 * @brief Register BT Classic link authentication failure callback
 */
void lisa_bt_classic_register_link_auth_fail_cb(lisa_bt_classic_link_auth_fail_cb_t cb);

/**
 * @brief Register BT Classic AVRCP key press callback
 */
void lisa_bt_classic_register_avrcp_cb(lisa_bt_classic_avrcp_cb_t cb);

/**
 * @brief Register BT Classic AVRCP notify indication callback
 */
void lisa_bt_classic_register_avrcp_notify_cb(lisa_bt_classic_avrcp_notify_cb_t cb);

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
void lisa_bt_classic_notify_avrcp_event(uint8_t conidx, uint8_t c_r, uint8_t event_id, uint8_t event_value);
void lisa_bt_classic_notify_profile(uint8_t conidx, int profile, bool connected);

#endif /* __LISA_BLUETOOTH_H__ */
