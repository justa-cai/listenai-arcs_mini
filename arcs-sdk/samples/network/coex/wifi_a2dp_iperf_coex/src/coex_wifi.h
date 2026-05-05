/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COEX_WIFI_H
#define COEX_WIFI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool initialized;
    bool stack_ready;
    bool connected;
    bool ready;
    bool connecting;
    uint32_t ip_addr;
} coex_wifi_status_t;

int coex_wifi_init(void);
int coex_wifi_connect(void);
int coex_wifi_disconnect(void);
bool coex_wifi_is_ready(void);
void coex_wifi_get_status(coex_wifi_status_t *status);

#endif /* COEX_WIFI_H */
