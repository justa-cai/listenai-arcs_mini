/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LISA_BLE_CLIENT_STACK_ABI_H
#define LISA_BLE_CLIENT_STACK_ABI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    LISA_STACK_GATT_SERVICE_COMPLETE = 0,
    LISA_STACK_GATT_SERVICE_START = 1,
    LISA_STACK_GATT_SERVICE_END = 2,
    LISA_STACK_GATT_SERVICE_CONTINUE = 3,
};

enum {
    LISA_STACK_GATT_UUID_16 = 0,
    LISA_STACK_GATT_UUID_32 = 1,
    LISA_STACK_GATT_UUID_128 = 2,
};

enum {
    LISA_STACK_GATT_DISCOVER_PRIMARY_ALL = 0,
    LISA_STACK_GATT_DISCOVER_PRIMARY_BY_UUID = 1,
    LISA_STACK_GATT_DISCOVER_SECONDARY_ALL = 2,
    LISA_STACK_GATT_DISCOVER_SECONDARY_BY_UUID = 3,
};

enum {
    LISA_STACK_GATT_ATT_NONE = 0,
    LISA_STACK_GATT_ATT_PRIMARY_SERVICE = 1,
    LISA_STACK_GATT_ATT_SECONDARY_SERVICE = 2,
    LISA_STACK_GATT_ATT_INCLUDED_SERVICE = 3,
    LISA_STACK_GATT_ATT_CHARACTERISTIC = 4,
    LISA_STACK_GATT_ATT_VALUE = 5,
    LISA_STACK_GATT_ATT_DESCRIPTOR = 6,
};

enum {
    LISA_STACK_GATT_WRITE = 0,
    LISA_STACK_GATT_WRITE_NO_RESPONSE = 1,
    LISA_STACK_GATT_WRITE_SIGNED = 2,
};

enum {
    LISA_STACK_GATT_EVENT_NOTIFY = 0,
    LISA_STACK_GATT_EVENT_INDICATE = 1,
};

typedef union {
    struct {
        uint16_t start_handle;
        uint16_t end_handle;
    } service;
    struct {
        uint16_t value_handle;
        uint8_t properties;
        uint8_t reserved;
    } characteristic;
} lisa_stack_gatt_attribute_info_t;

typedef struct {
    uint8_t att_type;
    uint8_t uuid_type;
    uint8_t uuid[16];
    lisa_stack_gatt_attribute_info_t info;
} lisa_stack_gatt_svc_att_t;

typedef struct {
    void (*discover_complete)(uint8_t conidx, uint8_t user_lid,
                              uint16_t dummy, uint16_t status);
    void (*read_complete)(uint8_t conidx, uint8_t user_lid,
                          uint16_t dummy, uint16_t status);
    void (*write_complete)(uint8_t conidx, uint8_t user_lid,
                           uint16_t dummy, uint16_t status);
    void (*attribute_value_get)(uint8_t conidx, uint8_t user_lid,
                                uint16_t token, uint16_t dummy,
                                uint16_t handle, uint16_t offset,
                                uint16_t max_length);
    void (*service)(uint8_t conidx, uint8_t user_lid, uint16_t dummy,
                    uint16_t handle, uint8_t discovery_info,
                    uint8_t attribute_count,
                    const lisa_stack_gatt_svc_att_t *attributes);
    void (*service_info)(uint8_t conidx, uint8_t user_lid, uint16_t dummy,
                         uint16_t start_handle, uint16_t end_handle,
                         uint8_t uuid_type, const uint8_t *uuid);
    void (*included_service)(uint8_t conidx, uint8_t user_lid,
                             uint16_t dummy, uint16_t include_handle,
                             uint16_t start_handle, uint16_t end_handle,
                             uint8_t uuid_type, const uint8_t *uuid);
    void (*characteristic)(uint8_t conidx, uint8_t user_lid,
                           uint16_t dummy, uint16_t handle,
                           uint16_t value_handle, uint8_t properties,
                           uint8_t uuid_type, const uint8_t *uuid);
    void (*descriptor)(uint8_t conidx, uint8_t user_lid, uint16_t dummy,
                       uint16_t handle, uint8_t uuid_type,
                       const uint8_t *uuid);
    void (*attribute_value)(uint8_t conidx, uint8_t user_lid,
                            uint16_t dummy, uint16_t handle,
                            uint16_t offset, void *buffer);
    void (*attribute_event)(uint8_t conidx, uint8_t user_lid,
                            uint16_t token, uint8_t event_type,
                            bool complete, uint16_t handle, void *buffer);
    void (*service_changed)(uint8_t conidx, uint8_t user_lid,
                            bool out_of_sync, uint16_t start_handle,
                            uint16_t end_handle);
} lisa_stack_gatt_cli_cb_t;

_Static_assert(sizeof(lisa_stack_gatt_svc_att_t) == 22,
               "GATT service attribute ABI mismatch");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(lisa_stack_gatt_cli_cb_t) == 48,
               "GATT client callback ABI mismatch");
#endif

uint16_t gatt_user_cli_register(uint16_t preferred_mtu, uint8_t priority,
                                const lisa_stack_gatt_cli_cb_t *callbacks,
                                uint8_t *user_lid);
uint16_t ble_gatt_user_unregister(uint8_t user_lid);
uint16_t ble_gatt_cli_discover_svc(uint8_t conidx, uint8_t user_lid,
                                   uint16_t dummy, uint8_t discovery_type,
                                   bool full, uint16_t start_handle,
                                   uint16_t end_handle, uint8_t uuid_type,
                                   const uint8_t *uuid);
uint16_t ble_gatt_cli_read(uint8_t conidx, uint8_t user_lid, uint16_t dummy,
                           uint16_t handle, uint16_t offset, uint16_t length);
uint16_t prf_gatt_write(uint8_t conidx, uint8_t user_lid, uint16_t dummy,
                        uint8_t write_type, uint16_t handle, uint16_t length,
                        const uint8_t *value);
uint16_t ble_gatt_cli_event_register(uint8_t conidx, uint8_t user_lid,
                                     uint16_t start_handle,
                                     uint16_t end_handle);
uint16_t ble_gatt_cli_event_unregister(uint8_t conidx, uint8_t user_lid,
                                       uint16_t start_handle,
                                       uint16_t end_handle);
uint16_t ble_gatt_cli_att_event_cfm(uint8_t conidx, uint8_t user_lid,
                                    uint16_t token);
uint16_t ble_co_buf_data_len(void *buffer);
uint8_t *ble_co_buf_data(void *buffer);

#endif /* LISA_BLE_CLIENT_STACK_ABI_H */
