/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef WIFI_UTILS_H
#define WIFI_UTILS_H

#include <stdbool.h>

/**
 * Initialize WiFi low-level driver (MAC manager, wifi driver).
 * Does NOT connect to any AP. Call once at startup.
 * @return 0 on success
 */
int wifi_utils_init(void);

/**
 * Connect to a WiFi AP and wait for DHCP.
 * @param ssid     AP SSID
 * @param pwd      AP password (can be empty string for open networks)
 * @param timeout_s  Max seconds to wait for connection + DHCP
 * @return 0 on success, -1 on timeout/failure
 */
int wifi_utils_connect(const char *ssid, const char *pwd, int timeout_s);

/**
 * @return true if WiFi is connected and IP obtained
 */
bool wifi_utils_is_connected(void);

#endif /* WIFI_UTILS_H */
