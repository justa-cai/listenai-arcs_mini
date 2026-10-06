/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LISA_BLE_CLIENT_H
#define LISA_BLE_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "lisa_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISA_BLE_CLIENT_MAX_CONNECTIONS 3U
#define LISA_BLE_CLIENT_MAX_WRITE_LEN 512U
#define LISA_BLE_CLIENT_MAX_READ_LEN 512U

typedef enum {
    LISA_BLE_UUID_16 = 2,
    LISA_BLE_UUID_32 = 4,
    LISA_BLE_UUID_128 = 16,
} lisa_ble_uuid_type_t;

typedef struct {
    uint8_t type;
    uint8_t value[16];
} lisa_ble_uuid_t;

typedef struct {
    lisa_ble_addr_t addr;
    int8_t rssi;
    uint8_t flags;
    uint8_t length;
    const uint8_t *data;
} lisa_ble_scan_report_t;

typedef struct {
    uint8_t conidx;
    uint16_t interval;
    uint16_t latency;
    uint16_t supervision_timeout;
} lisa_ble_conn_params_t;

typedef enum {
    LISA_BLE_GATT_ATTRIBUTE_UNKNOWN = 0,
    LISA_BLE_GATT_ATTRIBUTE_INCLUDED_SERVICE,
    LISA_BLE_GATT_ATTRIBUTE_CHARACTERISTIC,
    LISA_BLE_GATT_ATTRIBUTE_DESCRIPTOR,
} lisa_ble_gatt_attribute_kind_t;

typedef struct {
    uint16_t handle;
    uint16_t value_handle;
    uint8_t properties;
    uint8_t kind;
    lisa_ble_uuid_t uuid;
} lisa_ble_gatt_attribute_t;

typedef struct {
    uint16_t start_handle;
    uint16_t end_handle;
    bool primary;
    bool truncated;
    lisa_ble_uuid_t uuid;
    uint8_t attribute_count;
    const lisa_ble_gatt_attribute_t *attributes;
} lisa_ble_gatt_service_t;

typedef struct {
    void (*scan_report)(const lisa_ble_scan_report_t *report, void *user_data);
    void (*scan_activity)(bool active, uint8_t scan_id, uint16_t status,
                          void *user_data);
    void (*connection_activity)(bool active, uint8_t activity_id,
                                uint16_t status, void *user_data);
    void (*conn_params)(const lisa_ble_conn_params_t *params, void *user_data);
    void (*connection_failed)(uint16_t status, void *user_data);
    void (*service)(uint8_t conidx, const lisa_ble_gatt_service_t *service,
                    void *user_data);
    void (*discover_complete)(uint8_t conidx, uint16_t status,
                              void *user_data);
    void (*read_complete)(uint8_t conidx, uint16_t handle, uint16_t status,
                          const uint8_t *value, uint16_t length,
                          bool truncated, void *user_data);
    void (*write_complete)(uint8_t conidx, uint16_t handle, uint16_t status,
                           void *user_data);
    void (*notification)(uint8_t conidx, uint16_t handle,
                         const uint8_t *value, uint16_t length,
                         bool indication, void *user_data);
} lisa_ble_client_callbacks_t;

int lisa_ble_client_init(const lisa_ble_client_callbacks_t *callbacks,
                         void *user_data);
int lisa_ble_client_deinit(void);
int lisa_ble_client_discover_all(uint8_t conidx);
int lisa_ble_client_read(uint8_t conidx, uint16_t handle);
int lisa_ble_client_write(uint8_t conidx, uint16_t handle,
                          const uint8_t *data, uint16_t length);
int lisa_ble_client_subscribe(uint8_t conidx, uint16_t value_handle,
                              uint16_t cccd_handle, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* LISA_BLE_CLIENT_H */
