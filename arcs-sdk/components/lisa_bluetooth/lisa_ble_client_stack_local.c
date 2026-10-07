/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_ble_client_internal.h"
#include "lisa_ble_client_stack_abi.h"
#include "sysutils.h"

#include <errno.h>
#include <string.h>

#define CLIENT_GATT_INVALID_USER 0xffU
#define CLIENT_GATT_MIN_HANDLE 0x0001U
#define CLIENT_GATT_MAX_HANDLE 0xffffU
#define CLIENT_GATT_MAX_ATTRIBUTES 64U
#define CLIENT_GATT_ATTRIBUTE_NOT_FOUND 0x000aU

typedef struct {
    uint8_t user_lid;
    bool discovering_secondary;
    bool service_active;
    lisa_ble_gatt_service_t service;
    lisa_ble_gatt_attribute_t attributes[CLIENT_GATT_MAX_ATTRIBUTES];
} local_stack_context_t;

static __psram_data__ local_stack_context_t local = {
    .user_lid = CLIENT_GATT_INVALID_USER,
};

static int stack_status(uint16_t status)
{
    return status == 0 ? 0 : -(int)status;
}

static uint8_t uuid_length(uint8_t stack_type)
{
    static const uint8_t lengths[] = {2, 4, 16};

    return stack_type < (sizeof(lengths) / sizeof(lengths[0]))
               ? lengths[stack_type]
               : 0;
}

static void copy_uuid(lisa_ble_uuid_t *destination, uint8_t stack_type,
                      const uint8_t *uuid)
{
    uint8_t length = uuid_length(stack_type);

    memset(destination, 0, sizeof(*destination));
    if (length == 0 || uuid == NULL) {
        return;
    }
    destination->type = length;
    memcpy(destination->value, uuid, length);
}

static void reset_service(void)
{
    memset(&local.service, 0, sizeof(local.service));
    memset(local.attributes, 0, sizeof(local.attributes));
    local.service.attributes = local.attributes;
    local.service_active = false;
}

static void append_attribute(uint16_t handle,
                             const lisa_stack_gatt_svc_att_t *source)
{
    lisa_ble_gatt_attribute_t *destination;

    if (source->att_type == LISA_STACK_GATT_ATT_PRIMARY_SERVICE ||
        source->att_type == LISA_STACK_GATT_ATT_SECONDARY_SERVICE) {
        local.service.primary =
            source->att_type == LISA_STACK_GATT_ATT_PRIMARY_SERVICE;
        copy_uuid(&local.service.uuid, source->uuid_type, source->uuid);
        return;
    }
    if (source->att_type == LISA_STACK_GATT_ATT_VALUE ||
        source->att_type == LISA_STACK_GATT_ATT_NONE) {
        return;
    }
    if (local.service.attribute_count >= CLIENT_GATT_MAX_ATTRIBUTES) {
        local.service.truncated = true;
        return;
    }

    destination = &local.attributes[local.service.attribute_count++];
    destination->handle = handle;
    copy_uuid(&destination->uuid, source->uuid_type, source->uuid);

    switch (source->att_type) {
    case LISA_STACK_GATT_ATT_INCLUDED_SERVICE:
        destination->kind = LISA_BLE_GATT_ATTRIBUTE_INCLUDED_SERVICE;
        destination->value_handle = source->info.service.start_handle;
        break;
    case LISA_STACK_GATT_ATT_CHARACTERISTIC:
        destination->kind = LISA_BLE_GATT_ATTRIBUTE_CHARACTERISTIC;
        destination->value_handle =
            source->info.characteristic.value_handle;
        destination->properties = source->info.characteristic.properties;
        break;
    case LISA_STACK_GATT_ATT_DESCRIPTOR:
        destination->kind = LISA_BLE_GATT_ATTRIBUTE_DESCRIPTOR;
        break;
    default:
        destination->kind = LISA_BLE_GATT_ATTRIBUTE_UNKNOWN;
        break;
    }
}

static void on_discover_complete(uint8_t conidx, uint8_t user_lid,
                                 uint16_t dummy, uint16_t status)
{
    uint16_t next_status;

    (void)user_lid;
    (void)dummy;
    if (!local.discovering_secondary && status == 0) {
        local.discovering_secondary = true;
        next_status = ble_gatt_cli_discover_svc(
            conidx, local.user_lid, 0,
            LISA_STACK_GATT_DISCOVER_SECONDARY_ALL, true,
            CLIENT_GATT_MIN_HANDLE, CLIENT_GATT_MAX_HANDLE,
            LISA_STACK_GATT_UUID_16, NULL);
        if (next_status == 0) {
            return;
        }
        status = next_status;
    }

    if (local.discovering_secondary &&
        status == CLIENT_GATT_ATTRIBUTE_NOT_FOUND) {
        status = 0;
    }
    local.discovering_secondary = false;
    reset_service();
    lisa_ble_client_stack_discover_complete(conidx, status);
}

static void on_read_complete(uint8_t conidx, uint8_t user_lid,
                             uint16_t dummy, uint16_t status)
{
    (void)user_lid;
    lisa_ble_client_stack_read_complete(conidx, dummy, status);
}

static void on_write_complete(uint8_t conidx, uint8_t user_lid,
                              uint16_t dummy, uint16_t status)
{
    (void)user_lid;
    lisa_ble_client_stack_write_complete(conidx, dummy, status);
}

static void on_service(uint8_t conidx, uint8_t user_lid, uint16_t dummy,
                       uint16_t handle, uint8_t discovery_info,
                       uint8_t attribute_count,
                       const lisa_stack_gatt_svc_att_t *attributes)
{
    uint8_t index;

    (void)user_lid;
    (void)dummy;
    if (attributes == NULL || attribute_count == 0) {
        return;
    }

    if (discovery_info == LISA_STACK_GATT_SERVICE_COMPLETE ||
        discovery_info == LISA_STACK_GATT_SERVICE_START ||
        !local.service_active) {
        reset_service();
        local.service_active = true;
        local.service.start_handle = handle;
    }

    for (index = 0; index < attribute_count; ++index) {
        append_attribute((uint16_t)(handle + index), &attributes[index]);
    }
    local.service.end_handle = (uint16_t)(handle + attribute_count - 1U);

    if (discovery_info == LISA_STACK_GATT_SERVICE_COMPLETE ||
        discovery_info == LISA_STACK_GATT_SERVICE_END) {
        lisa_ble_client_stack_service(conidx, &local.service);
        local.service_active = false;
    }
}

static void on_attribute_value(uint8_t conidx, uint8_t user_lid,
                               uint16_t dummy, uint16_t handle,
                               uint16_t offset, void *buffer)
{
    (void)user_lid;
    (void)dummy;
    if (buffer == NULL) {
        return;
    }
    lisa_ble_client_stack_read_value(conidx, handle, offset,
                                     ble_co_buf_data(buffer),
                                     ble_co_buf_data_len(buffer));
}

static void on_attribute_event(uint8_t conidx, uint8_t user_lid,
                               uint16_t token, uint8_t event_type,
                               bool complete, uint16_t handle, void *buffer)
{
    (void)user_lid;
    (void)complete;
    if (buffer == NULL) {
        ble_gatt_cli_att_event_cfm(conidx, local.user_lid, token);
        return;
    }
    lisa_ble_client_stack_notification(
        conidx, handle, ble_co_buf_data(buffer), ble_co_buf_data_len(buffer),
        event_type == LISA_STACK_GATT_EVENT_INDICATE, token);
}

static void on_service_changed(uint8_t conidx, uint8_t user_lid,
                               bool out_of_sync, uint16_t start_handle,
                               uint16_t end_handle)
{
    (void)conidx;
    (void)user_lid;
    (void)out_of_sync;
    (void)start_handle;
    (void)end_handle;
}

static const lisa_stack_gatt_cli_cb_t gatt_callbacks = {
    .discover_complete = on_discover_complete,
    .read_complete = on_read_complete,
    .write_complete = on_write_complete,
    .attribute_value_get = NULL,
    .service = on_service,
    .service_info = NULL,
    .included_service = NULL,
    .characteristic = NULL,
    .descriptor = NULL,
    .attribute_value = on_attribute_value,
    .attribute_event = on_attribute_event,
    .service_changed = on_service_changed,
};

static int local_init(void)
{
    uint16_t status;

    reset_service();
    local.discovering_secondary = false;
    local.user_lid = CLIENT_GATT_INVALID_USER;
    status = gatt_user_cli_register(LISA_BLE_CLIENT_MAX_READ_LEN, 0,
                                    &gatt_callbacks, &local.user_lid);
    if (status != 0) {
        local.user_lid = CLIENT_GATT_INVALID_USER;
    }
    return stack_status(status);
}

static void local_deinit(void)
{
    if (local.user_lid != CLIENT_GATT_INVALID_USER) {
        ble_gatt_user_unregister(local.user_lid);
    }
    local.user_lid = CLIENT_GATT_INVALID_USER;
    local.discovering_secondary = false;
    reset_service();
}

static int local_discover_all(uint8_t conidx)
{
    uint16_t status;

    local.discovering_secondary = false;
    reset_service();
    status = ble_gatt_cli_discover_svc(
        conidx, local.user_lid, 0, LISA_STACK_GATT_DISCOVER_PRIMARY_ALL,
        true, CLIENT_GATT_MIN_HANDLE, CLIENT_GATT_MAX_HANDLE,
        LISA_STACK_GATT_UUID_16, NULL);
    return stack_status(status);
}

static int local_read(uint8_t conidx, uint16_t handle)
{
    return stack_status(ble_gatt_cli_read(conidx, local.user_lid, handle,
                                          handle, 0, 0));
}

static int local_write(uint8_t conidx, uint16_t handle, uint16_t length)
{
    uint8_t value[LISA_BLE_CLIENT_MAX_WRITE_LEN];

    if (lisa_ble_client_stack_write_value(value, sizeof(value)) != length) {
        return -EINVAL;
    }
    return stack_status(prf_gatt_write(conidx, local.user_lid, handle,
                                       LISA_STACK_GATT_WRITE, handle, length,
                                       value));
}

static int local_event_register(uint8_t conidx, uint16_t start_handle,
                                uint16_t end_handle)
{
    return stack_status(ble_gatt_cli_event_register(
        conidx, local.user_lid, start_handle, end_handle));
}

static int local_event_unregister(uint8_t conidx, uint16_t start_handle,
                                  uint16_t end_handle)
{
    return stack_status(ble_gatt_cli_event_unregister(
        conidx, local.user_lid, start_handle, end_handle));
}

static int local_event_confirm(uint8_t conidx, uint16_t token)
{
    return stack_status(
        ble_gatt_cli_att_event_cfm(conidx, local.user_lid, token));
}

static const lisa_ble_client_stack_ops_t local_ops = {
    .init = local_init,
    .deinit = local_deinit,
    .discover_all = local_discover_all,
    .read = local_read,
    .write = local_write,
    .event_register = local_event_register,
    .event_unregister = local_event_unregister,
    .event_confirm = local_event_confirm,
};

const lisa_ble_client_stack_ops_t *lisa_ble_client_stack_ops_get(void)
{
    return &local_ops;
}
