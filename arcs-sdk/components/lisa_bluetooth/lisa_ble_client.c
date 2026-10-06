/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_ble_client.h"
#include "lisa_ble_client_internal.h"
#include "sysutils.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

typedef enum {
    CLIENT_OP_NONE = 0,
    CLIENT_OP_DISCOVER,
    CLIENT_OP_READ,
    CLIENT_OP_WRITE,
    CLIENT_OP_SUBSCRIBE,
} client_operation_t;

typedef struct {
    bool initialized;
    client_operation_t operation;
    uint8_t conidx;
    uint16_t handle;
    uint16_t value_handle;
    bool subscribe_enable;
    bool read_truncated;
    uint16_t read_length;
    uint8_t read_value[LISA_BLE_CLIENT_MAX_READ_LEN];
    uint16_t write_length;
    uint8_t write_value[LISA_BLE_CLIENT_MAX_WRITE_LEN];
    lisa_ble_client_callbacks_t callbacks;
    void *user_data;
    const lisa_ble_client_stack_ops_t *ops;
} lisa_ble_client_context_t;

static __psram_bss__ lisa_ble_client_context_t client;

static bool valid_conidx(uint8_t conidx)
{
    return conidx < LISA_BLE_CLIENT_MAX_CONNECTIONS;
}

static int begin_operation(client_operation_t operation, uint8_t conidx,
                           uint16_t handle)
{
    if (!client.initialized) {
        return -EACCES;
    }
    if (!valid_conidx(conidx) || (operation != CLIENT_OP_DISCOVER && handle == 0)) {
        return -EINVAL;
    }
    if (client.operation != CLIENT_OP_NONE) {
        return -EBUSY;
    }

    client.operation = operation;
    client.conidx = conidx;
    client.handle = handle;
    return 0;
}

static void clear_operation(void)
{
    client.operation = CLIENT_OP_NONE;
    client.conidx = 0;
    client.handle = 0;
    client.value_handle = 0;
    client.subscribe_enable = false;
    client.read_truncated = false;
    client.read_length = 0;
    client.write_length = 0;
}

int lisa_ble_client_init(const lisa_ble_client_callbacks_t *callbacks,
                         void *user_data)
{
    const lisa_ble_client_stack_ops_t *ops;
    int ret;

    if (callbacks == NULL) {
        return -EINVAL;
    }
    if (client.initialized) {
        return -EALREADY;
    }

    ops = lisa_ble_client_stack_ops_get();
    if (ops == NULL || ops->init == NULL || ops->deinit == NULL ||
        ops->discover_all == NULL || ops->read == NULL || ops->write == NULL ||
        ops->event_register == NULL || ops->event_unregister == NULL ||
        ops->event_confirm == NULL) {
        return -ENOTSUP;
    }

    ret = ops->init();
    if (ret != 0) {
        return ret;
    }

    memset(&client, 0, sizeof(client));
    client.callbacks = *callbacks;
    client.user_data = user_data;
    client.ops = ops;
    client.initialized = true;
    return 0;
}

int lisa_ble_client_deinit(void)
{
    const lisa_ble_client_stack_ops_t *ops = client.ops;

    if (!client.initialized) {
        return 0;
    }

    client.initialized = false;
    if (ops != NULL && ops->deinit != NULL) {
        ops->deinit();
    }
    memset(&client, 0, sizeof(client));
    return 0;
}

int lisa_ble_client_discover_all(uint8_t conidx)
{
    int ret = begin_operation(CLIENT_OP_DISCOVER, conidx, 0);

    if (ret != 0) {
        return ret;
    }
    ret = client.ops->discover_all(conidx);
    if (ret != 0) {
        clear_operation();
    }
    return ret;
}

int lisa_ble_client_read(uint8_t conidx, uint16_t handle)
{
    int ret = begin_operation(CLIENT_OP_READ, conidx, handle);

    if (ret != 0) {
        return ret;
    }
    client.read_length = 0;
    client.read_truncated = false;
    ret = client.ops->read(conidx, handle);
    if (ret != 0) {
        clear_operation();
    }
    return ret;
}

int lisa_ble_client_write(uint8_t conidx, uint16_t handle,
                          const uint8_t *data, uint16_t length)
{
    int ret;

    if (data == NULL || length == 0) {
        return -EINVAL;
    }
    if (length > LISA_BLE_CLIENT_MAX_WRITE_LEN) {
        return -EMSGSIZE;
    }

    ret = begin_operation(CLIENT_OP_WRITE, conidx, handle);
    if (ret != 0) {
        return ret;
    }
    memcpy(client.write_value, data, length);
    client.write_length = length;
    ret = client.ops->write(conidx, handle, length);
    if (ret != 0) {
        clear_operation();
    }
    return ret;
}

int lisa_ble_client_subscribe(uint8_t conidx, uint16_t value_handle,
                              uint16_t cccd_handle, bool enable)
{
    int ret;

    if (value_handle == 0 || cccd_handle == 0) {
        return -EINVAL;
    }

    ret = begin_operation(CLIENT_OP_SUBSCRIBE, conidx, cccd_handle);
    if (ret != 0) {
        return ret;
    }

    client.value_handle = value_handle;
    client.subscribe_enable = enable;
    client.write_value[0] = enable ? 1U : 0U;
    client.write_value[1] = 0;
    client.write_length = 2;

    if (enable) {
        ret = client.ops->event_register(conidx, value_handle, value_handle);
        if (ret != 0) {
            clear_operation();
            return ret;
        }
    }

    ret = client.ops->write(conidx, cccd_handle, client.write_length);
    if (ret != 0) {
        if (enable) {
            client.ops->event_unregister(conidx, value_handle, value_handle);
        }
        clear_operation();
    }
    return ret;
}

void lisa_ble_client_stack_scan_report(const lisa_ble_scan_report_t *report)
{
    if (client.initialized && report != NULL && client.callbacks.scan_report != NULL) {
        client.callbacks.scan_report(report, client.user_data);
    }
}

void lisa_ble_client_stack_scan_activity(bool active, uint8_t scan_id,
                                         uint16_t status)
{
    if (client.initialized && client.callbacks.scan_activity != NULL) {
        client.callbacks.scan_activity(active, scan_id, status,
                                       client.user_data);
    }
}

void lisa_ble_client_stack_connection_activity(bool active,
                                               uint8_t activity_id,
                                               uint16_t status)
{
    if (client.initialized && client.callbacks.connection_activity != NULL) {
        client.callbacks.connection_activity(active, activity_id, status,
                                             client.user_data);
    }
}

void lisa_ble_client_stack_conn_params(const lisa_ble_conn_params_t *params)
{
    if (client.initialized && params != NULL && client.callbacks.conn_params != NULL) {
        client.callbacks.conn_params(params, client.user_data);
    }
}

void lisa_ble_client_stack_connection_failed(uint16_t status)
{
    if (client.initialized && client.callbacks.connection_failed != NULL) {
        client.callbacks.connection_failed(status, client.user_data);
    }
}

void lisa_ble_client_stack_disconnected(uint8_t conidx)
{
    if (client.initialized && client.operation != CLIENT_OP_NONE &&
        client.conidx == conidx) {
        clear_operation();
    }
}

void lisa_ble_client_stack_service(uint8_t conidx,
                                   const lisa_ble_gatt_service_t *service)
{
    if (client.initialized && client.operation == CLIENT_OP_DISCOVER &&
        client.conidx == conidx && service != NULL &&
        client.callbacks.service != NULL) {
        client.callbacks.service(conidx, service, client.user_data);
    }
}

void lisa_ble_client_stack_discover_complete(uint8_t conidx, uint16_t status)
{
    if (!client.initialized || client.operation != CLIENT_OP_DISCOVER ||
        client.conidx != conidx) {
        return;
    }

    clear_operation();
    if (client.callbacks.discover_complete != NULL) {
        client.callbacks.discover_complete(conidx, status, client.user_data);
    }
}

void lisa_ble_client_stack_read_value(uint8_t conidx, uint16_t handle,
                                      uint16_t offset, const uint8_t *value,
                                      uint16_t length)
{
    uint16_t copy_length;

    if (!client.initialized || client.operation != CLIENT_OP_READ ||
        client.conidx != conidx || client.handle != handle || value == NULL) {
        return;
    }
    if (offset >= LISA_BLE_CLIENT_MAX_READ_LEN) {
        client.read_truncated = true;
        return;
    }

    copy_length = length;
    if ((uint32_t)offset + copy_length > LISA_BLE_CLIENT_MAX_READ_LEN) {
        copy_length = LISA_BLE_CLIENT_MAX_READ_LEN - offset;
        client.read_truncated = true;
    }
    memcpy(client.read_value + offset, value, copy_length);
    if ((uint16_t)(offset + copy_length) > client.read_length) {
        client.read_length = offset + copy_length;
    }
}

void lisa_ble_client_stack_read_complete(uint8_t conidx, uint16_t handle,
                                         uint16_t status)
{
    uint16_t length;
    bool truncated;

    if (!client.initialized || client.operation != CLIENT_OP_READ ||
        client.conidx != conidx || client.handle != handle) {
        return;
    }

    length = client.read_length;
    truncated = client.read_truncated;
    clear_operation();
    if (client.callbacks.read_complete != NULL) {
        client.callbacks.read_complete(conidx, handle, status,
                                       client.read_value, length, truncated,
                                       client.user_data);
    }
}

uint16_t lisa_ble_client_stack_write_value(uint8_t *value,
                                           uint16_t max_length)
{
    if (!client.initialized ||
        (client.operation != CLIENT_OP_WRITE &&
         client.operation != CLIENT_OP_SUBSCRIBE) ||
        value == NULL || max_length < client.write_length) {
        return 0;
    }

    memcpy(value, client.write_value, client.write_length);
    return client.write_length;
}

void lisa_ble_client_stack_write_complete(uint8_t conidx, uint16_t handle,
                                          uint16_t status)
{
    client_operation_t operation;
    uint16_t value_handle;
    bool enable;

    if (!client.initialized ||
        (client.operation != CLIENT_OP_WRITE &&
         client.operation != CLIENT_OP_SUBSCRIBE) ||
        client.conidx != conidx || client.handle != handle) {
        return;
    }

    operation = client.operation;
    value_handle = client.value_handle;
    enable = client.subscribe_enable;
    clear_operation();

    if (operation == CLIENT_OP_SUBSCRIBE && (!enable || status != 0)) {
        client.ops->event_unregister(conidx, value_handle, value_handle);
    }
    if (client.callbacks.write_complete != NULL) {
        client.callbacks.write_complete(conidx, handle, status,
                                        client.user_data);
    }
}

void lisa_ble_client_stack_notification(uint8_t conidx, uint16_t handle,
                                         const uint8_t *value,
                                         uint16_t length, bool indication,
                                         uint16_t token)
{
    if (!client.initialized || value == NULL) {
        return;
    }

    if (client.callbacks.notification != NULL) {
        client.callbacks.notification(conidx, handle, value, length,
                                      indication, client.user_data);
    }
    client.ops->event_confirm(conidx, token);
}
