/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BT_STORAGE_PORT_H__
#define __BT_STORAGE_PORT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t bt_storage_port_get(uint8_t param_id, uint8_t *lengthPtr, uint8_t *buf);
uint8_t bt_storage_port_set(uint8_t param_id, uint8_t length, uint8_t *buf);
uint8_t bt_storage_port_del(uint8_t param_id);

#ifdef __cplusplus
}
#endif

#endif /* __BT_STORAGE_PORT_H__ */
