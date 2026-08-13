/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uac_device.h"

#include <stdbool.h>

#include "uac_descriptors.h"
#include "usb_device_config.h"
#include "usbd_core.h"

#define TAG "uac"
#include "lisa_log.h"

void uac_descriptors_register(uint8_t busid);
int uac_stream_init(const uac_device_config_t *config);
void uac_stream_register_endpoints(uint8_t busid);
void uac_stream_on_disconnect(void);

static volatile bool uac_registered;

static bool uac_device_config_valid(const uac_device_config_t *config)
{
    return config != NULL &&
           config->capture != NULL &&
           config->capture->dir == UAC_STREAM_CAPTURE &&
           config->capture->ops != NULL &&
           config->capture->ops->read != NULL &&
           config->playback != NULL &&
           config->playback->dir == UAC_STREAM_PLAYBACK &&
           config->playback->ops != NULL &&
           config->playback->ops->write != NULL;
}

int uac_device_register(const uac_device_config_t *config)
{
    if (uac_registered) {
        return 0;
    }

    if (!uac_device_config_valid(config)) {
        LISA_LOGE(TAG, "invalid UAC stream config");
        return -1;
    }

    int ret = uac_stream_init(config);
    if (ret != 0) {
        return ret;
    }

    uac_descriptors_register(USB_DEVICE_BUS_ID);
    uac_stream_register_endpoints(USB_DEVICE_BUS_ID);

    uac_registered = true;
    LISA_LOGI(TAG, "CherryUSB UAC registered: mic %uHz/%ubit/%uch packet=%u interval=%u, speaker %uHz/%ubit/%uch packet=%u",
              (unsigned int)UAC_MIC_SAMPLE_RATE,
              (unsigned int)UAC_MIC_SAMPLE_BITS,
              (unsigned int)UAC_MIC_CHANNELS,
              (unsigned int)UAC_MIC_PACKET_BYTES,
              (unsigned int)UAC_HS_INTERVAL,
              (unsigned int)UAC_SPK_SAMPLE_RATE,
              (unsigned int)UAC_SPK_SAMPLE_BITS,
              (unsigned int)UAC_SPK_CHANNELS,
              (unsigned int)UAC_SPK_PACKET_BYTES);
    return 0;
}

void uac_device_handle_usb_event(uint8_t event)
{
    if (event == USBD_EVENT_DISCONNECTED) {
        uac_stream_on_disconnect();
    }
}
