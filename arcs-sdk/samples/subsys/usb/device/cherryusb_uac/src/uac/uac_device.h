/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_UAC_DEVICE_H
#define CHERRYUSB_UAC_DEVICE_H

#include <stdint.h>

#include "uac_stream.h"

typedef struct {
    uac_stream_t *capture;
    uac_stream_t *playback;
} uac_device_config_t;

int uac_device_register(const uac_device_config_t *config);
void uac_device_handle_usb_event(uint8_t event);

#endif /* CHERRYUSB_UAC_DEVICE_H */
