/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_UAC_USB_DEVICE_H
#define CHERRYUSB_UAC_USB_DEVICE_H

#include <stdint.h>

typedef void (*usb_device_event_cb_t)(uint8_t busid, uint8_t event);

int usb_device_start(usb_device_event_cb_t event_cb);

#endif /* CHERRYUSB_UAC_USB_DEVICE_H */
