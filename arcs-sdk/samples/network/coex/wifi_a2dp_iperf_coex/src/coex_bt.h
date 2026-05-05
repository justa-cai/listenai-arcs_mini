/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COEX_BT_H
#define COEX_BT_H

#include <stdbool.h>
#include <stdint.h>

#define COEX_BT_ADDR_STR_LEN 18

typedef struct {
    bool initialized;
    bool discovery_registered;
    bool connected;
    bool a2dp_connected;
    uint8_t discovered_count;
    char peer_addr[COEX_BT_ADDR_STR_LEN];
} coex_bt_status_t;

int coex_bt_init(void);
int coex_bt_inquiry(void);
int coex_bt_connect_by_name(const char *name);
int coex_bt_connect_by_index(uint8_t index);
bool coex_bt_is_connected(void);
bool coex_bt_is_a2dp_connected(void);
void coex_bt_get_status(coex_bt_status_t *status);

#endif /* COEX_BT_H */
