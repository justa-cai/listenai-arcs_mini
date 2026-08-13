/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usbd_core.h"
#include "adb_device.h"
#include "app_usb_cherry.h"
#include "usbd_audio.h"
#include "usbd_msc.h"

#define APP_USB_VID     0x0483U
#define APP_USB_ADB_PID 0x0ADBU
#define APP_USB_MSC_PID 0x4001U
#define APP_USB_ADB_BCD_DEVICE 0x0104U

#define APP_USB_MAX_POWER_MA 100U
#define APP_USB_ADB_DESC_LEN 23U

#if defined(CONFIG_USB_HS)
#define APP_USB_BULK_MPS 512U
#else
#define APP_USB_BULK_MPS 64U
#endif

enum {
    APP_USB_ITF_ADB = 0,
#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
    APP_USB_ITF_AUDIO_CONTROL,
    APP_USB_ITF_AUDIO_STREAM,
#endif
    APP_USB_ITF_TOTAL,
};

#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
#define APP_USB_AUDIO_INPUT_TERM_ID  0x01U
#define APP_USB_AUDIO_FEATURE_UNIT_ID 0x02U
#define APP_USB_AUDIO_OUTPUT_TERM_ID 0x03U

#define APP_USB_AUDIO_CHANNEL_CONFIG 0x0033U
#define APP_USB_AUDIO_INPUT_CONTROLS \
    0x03U, 0x03U, 0x03U, 0x03U, 0x03U

#define APP_USB_AUDIO_AC_SIZE \
    (AUDIO_SIZEOF_AC_HEADER_DESC(1) + \
     AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC + \
     AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(APP_USB_AUDIO_CHANNELS, 1) + \
     AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC)

#define APP_USB_AUDIO_DESC_LEN \
    (AUDIO_AC_DESCRIPTOR_LEN(1) + \
     AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC + \
     AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(APP_USB_AUDIO_CHANNELS, 1) + \
     AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC + \
     AUDIO_AS_DESCRIPTOR_LEN(1))

/* The ARCS CherryUSB sample clears sampling-frequency control for fixed-rate HS audio. */
#define APP_USB_AUDIO_AS_FIXED_FREQ_DESCRIPTOR_INIT() \
    0x09, USB_DESCRIPTOR_TYPE_INTERFACE, APP_USB_ITF_AUDIO_STREAM, 0x00, 0x00, \
        USB_DEVICE_CLASS_AUDIO, AUDIO_SUBCLASS_AUDIOSTREAMING, AUDIO_PROTOCOL_UNDEFINED, 0x05, \
    0x09, USB_DESCRIPTOR_TYPE_INTERFACE, APP_USB_ITF_AUDIO_STREAM, 0x01, 0x01, \
        USB_DEVICE_CLASS_AUDIO, AUDIO_SUBCLASS_AUDIOSTREAMING, AUDIO_PROTOCOL_UNDEFINED, 0x00, \
    0x07, AUDIO_INTERFACE_DESCRIPTOR_TYPE, AUDIO_STREAMING_GENERAL, \
        APP_USB_AUDIO_OUTPUT_TERM_ID, 0x01, WBVAL(AUDIO_FORMAT_PCM), \
    0x0B, AUDIO_INTERFACE_DESCRIPTOR_TYPE, AUDIO_STREAMING_FORMAT_TYPE, \
        AUDIO_FORMAT_TYPE_I, APP_USB_AUDIO_CHANNELS, APP_USB_AUDIO_SAMPLE_BITS / 8U, \
        APP_USB_AUDIO_SAMPLE_BITS, 0x01, AUDIO_SAMPLE_FREQ_3B(APP_USB_AUDIO_SAMPLE_RATE), \
    0x09, USB_DESCRIPTOR_TYPE_ENDPOINT, APP_USB_AUDIO_IN_EP, 0x05, \
        WBVAL(APP_USB_AUDIO_MAX_PACKET_SIZE), 0x04, 0x00, 0x00, \
    0x07, AUDIO_ENDPOINT_DESCRIPTOR_TYPE, AUDIO_ENDPOINT_GENERAL, \
        0x00, 0x00, 0x00, 0x00
#else
#define APP_USB_AUDIO_DESC_LEN 0U
#endif

#define APP_USB_ADB_CONFIG_SIZE \
    (9U + APP_USB_ADB_DESC_LEN + APP_USB_AUDIO_DESC_LEN)
#define APP_USB_MSC_CONFIG_SIZE (9U + MSC_DESCRIPTOR_LEN)

static bool usb_msc_mode;

static const uint8_t adb_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00,
                               APP_USB_VID, APP_USB_ADB_PID,
                               APP_USB_ADB_BCD_DEVICE, 0x01)
};

static const uint8_t msc_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, USB_DEVICE_CLASS_MASS_STORAGE, 0xFF, 0x00,
                               APP_USB_VID, APP_USB_MSC_PID, 0x0100, 0x01)
};

static const uint8_t adb_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(APP_USB_ADB_CONFIG_SIZE, APP_USB_ITF_TOTAL, 0x01,
                               USB_CONFIG_BUS_POWERED | USB_CONFIG_REMOTE_WAKEUP,
                               APP_USB_MAX_POWER_MA),
    USB_INTERFACE_DESCRIPTOR_INIT(APP_USB_ITF_ADB, 0x00, 0x02, 0xFF, 0x42, 0x01, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(APP_USB_ADB_OUT_EP, 0x02, APP_USB_BULK_MPS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(APP_USB_ADB_IN_EP, 0x02, APP_USB_BULK_MPS, 0x00),
#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
    AUDIO_AC_DESCRIPTOR_INIT(APP_USB_ITF_AUDIO_CONTROL, 0x02,
                             APP_USB_AUDIO_AC_SIZE, 0x05,
                             APP_USB_ITF_AUDIO_STREAM),
    AUDIO_AC_INPUT_TERMINAL_DESCRIPTOR_INIT(APP_USB_AUDIO_INPUT_TERM_ID,
                                             AUDIO_INTERM_MIC,
                                             APP_USB_AUDIO_CHANNELS,
                                             APP_USB_AUDIO_CHANNEL_CONFIG),
    AUDIO_AC_FEATURE_UNIT_DESCRIPTOR_INIT(APP_USB_AUDIO_FEATURE_UNIT_ID,
                                           APP_USB_AUDIO_INPUT_TERM_ID, 0x01,
                                           APP_USB_AUDIO_INPUT_CONTROLS),
    AUDIO_AC_OUTPUT_TERMINAL_DESCRIPTOR_INIT(APP_USB_AUDIO_OUTPUT_TERM_ID,
                                              AUDIO_TERMINAL_STREAMING,
                                              APP_USB_AUDIO_FEATURE_UNIT_ID),
    APP_USB_AUDIO_AS_FIXED_FREQ_DESCRIPTOR_INIT(),
#endif
};

static const uint8_t msc_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(APP_USB_MSC_CONFIG_SIZE, 0x01, 0x01,
                               USB_CONFIG_BUS_POWERED | USB_CONFIG_REMOTE_WAKEUP,
                               APP_USB_MAX_POWER_MA),
    MSC_DESCRIPTOR_INIT(0x00, APP_USB_MSC_OUT_EP, APP_USB_MSC_IN_EP,
                        APP_USB_BULK_MPS, 0x00)
};

_Static_assert(sizeof(adb_config_descriptor) == APP_USB_ADB_CONFIG_SIZE,
               "ADB composite descriptor size mismatch");
_Static_assert(sizeof(msc_config_descriptor) == APP_USB_MSC_CONFIG_SIZE,
               "MSC descriptor size mismatch");

static const uint8_t adb_qualifier_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
};

static const uint8_t msc_qualifier_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, USB_DEVICE_CLASS_MASS_STORAGE,
                                         0xFF, 0x00, 0x01),
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    "ListenAi",
    "VoiceAssistant CherryUSB UAC1",
    "FFBBCCDDEE001122",
    "ADB Interface",
    "CherryUSB UAC1 Audio",
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return usb_msc_mode ? msc_device_descriptor : adb_device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return usb_msc_mode ? msc_config_descriptor : adb_config_descriptor;
}

static const uint8_t *qualifier_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return usb_msc_mode ? msc_qualifier_descriptor : adb_qualifier_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(string_descriptors) / sizeof(string_descriptors[0]))) {
        return NULL;
    }
    return string_descriptors[index];
}

static const struct usb_descriptor app_usb_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = qualifier_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

static struct usbd_interface adb_interface;
static struct usbd_interface msc_interface;

#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
static struct usbd_interface audio_control_interface;
static struct usbd_interface audio_stream_interface;
static struct audio_entity_info audio_entity_table[] = {
    {
        .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT,
        .bEntityId = APP_USB_AUDIO_FEATURE_UNIT_ID,
        .ep = APP_USB_AUDIO_IN_EP,
    },
};
#endif

void app_usb_cherry_descriptors_register(bool msc_mode)
{
    usb_msc_mode = msc_mode;
    usbd_desc_register(APP_USB_BUS_ID, &app_usb_descriptor);

    if (msc_mode) {
        usbd_add_interface(APP_USB_BUS_ID,
                           usbd_msc_init_intf(APP_USB_BUS_ID, &msc_interface,
                                              APP_USB_MSC_OUT_EP, APP_USB_MSC_IN_EP));
        return;
    }

    usbd_add_interface(APP_USB_BUS_ID,
                       adb_dev_init_intf(APP_USB_BUS_ID, &adb_interface,
                                         APP_USB_ADB_IN_EP, APP_USB_ADB_OUT_EP));

#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
    usbd_add_interface(APP_USB_BUS_ID,
                       usbd_audio_init_intf(APP_USB_BUS_ID, &audio_control_interface,
                                            0x0100, audio_entity_table,
                                            sizeof(audio_entity_table) / sizeof(audio_entity_table[0])));
    usbd_add_interface(APP_USB_BUS_ID,
                       usbd_audio_init_intf(APP_USB_BUS_ID, &audio_stream_interface,
                                            0x0100, audio_entity_table,
                                            sizeof(audio_entity_table) / sizeof(audio_entity_table[0])));
    app_usb_cherry_audio_register_endpoint(APP_USB_BUS_ID);
#endif
}
