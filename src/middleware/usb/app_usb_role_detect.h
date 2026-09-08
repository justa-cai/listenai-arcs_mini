/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_USB_ROLE_UNKNOWN = -1,
    APP_USB_ROLE_DEVICE = 0,
    APP_USB_ROLE_HOST = 1,
} app_usb_role_t;

int app_usb_role_detect_init(void);
/* Returns <0 on I2C error, 0 when disconnected, 1 when a valid role is present. */
int app_usb_role_detect_read(app_usb_role_t *role);
const char *app_usb_role_name(app_usb_role_t role);

#ifdef __cplusplus
}
#endif
