/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LISA_BLE_CLIENT_INTERNAL_H
#define LISA_BLE_CLIENT_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "lisa_ble_client.h"

typedef struct {
    int (*init)(void);
    void (*deinit)(void);
    int (*discover_all)(uint8_t conidx);
    int (*read)(uint8_t conidx, uint16_t handle);
    int (*write)(uint8_t conidx, uint16_t handle, uint16_t length);
    int (*event_register)(uint8_t conidx, uint16_t start_handle,
                          uint16_t end_handle);
    int (*event_unregister)(uint8_t conidx, uint16_t start_handle,
                            uint16_t end_handle);
    int (*event_confirm)(uint8_t conidx, uint16_t token);
} lisa_ble_client_stack_ops_t;

const lisa_ble_client_stack_ops_t *lisa_ble_client_stack_ops_get(void);

void lisa_ble_client_stack_scan_report(const lisa_ble_scan_report_t *report);
void lisa_ble_client_stack_scan_activity(bool active, uint8_t scan_id,
                                         uint16_t status);
void lisa_ble_client_stack_connection_activity(bool active,
                                               uint8_t activity_id,
                                               uint16_t status);
void lisa_ble_client_stack_conn_params(const lisa_ble_conn_params_t *params);
void lisa_ble_client_stack_connection_failed(uint16_t status);
void lisa_ble_client_stack_disconnected(uint8_t conidx);
void lisa_ble_client_stack_service(uint8_t conidx,
                                   const lisa_ble_gatt_service_t *service);
void lisa_ble_client_stack_discover_complete(uint8_t conidx, uint16_t status);
void lisa_ble_client_stack_read_value(uint8_t conidx, uint16_t handle,
                                      uint16_t offset, const uint8_t *value,
                                      uint16_t length);
void lisa_ble_client_stack_read_complete(uint8_t conidx, uint16_t handle,
                                         uint16_t status);
uint16_t lisa_ble_client_stack_write_value(uint8_t *value,
                                           uint16_t max_length);
void lisa_ble_client_stack_write_complete(uint8_t conidx, uint16_t handle,
                                          uint16_t status);
void lisa_ble_client_stack_notification(uint8_t conidx, uint16_t handle,
                                         const uint8_t *value,
                                         uint16_t length, bool indication,
                                         uint16_t token);

#endif /* LISA_BLE_CLIENT_INTERNAL_H */
