/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uac_descriptors.h"
#include "usb_device_config.h"

#define UAC_CONFIG_SIZE (unsigned long)(9 +                                                       \
                                        AUDIO_AC_DESCRIPTOR_LEN(2) +                              \
                                        AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                     \
                                        AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(UAC_MIC_CHANNELS, 1) + \
                                        AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC +                    \
                                        AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                     \
                                        AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(UAC_SPK_CHANNELS, 1) + \
                                        AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC +                    \
                                        AUDIO_AS_DESCRIPTOR_LEN(1) +                              \
                                        AUDIO_AS_DESCRIPTOR_LEN(1))

#define UAC_AC_SIZE (AUDIO_SIZEOF_AC_HEADER_DESC(2) +                                             \
                     AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                                       \
                     AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(UAC_MIC_CHANNELS, 1) +                    \
                     AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC +                                      \
                     AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                                       \
                     AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(UAC_SPK_CHANNELS, 1) +                    \
                     AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC)

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xef, 0x02, 0x01, USB_DEVICE_VID, USB_DEVICE_PID, 0x0001, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(UAC_CONFIG_SIZE, UAC_ITF_TOTAL, 0x01, USB_CONFIG_BUS_POWERED, USB_DEVICE_MAX_POWER),
    AUDIO_AC_DESCRIPTOR_INIT(UAC_ITF_CONTROL, UAC_ITF_TOTAL, UAC_AC_SIZE, 0x00, UAC_ITF_MIC, UAC_ITF_SPK),

    AUDIO_AC_INPUT_TERMINAL_DESCRIPTOR_INIT(0x01, AUDIO_INTERM_MIC, UAC_MIC_CHANNELS, UAC_CH_ENABLE(UAC_MIC_CHANNELS)),
    AUDIO_AC_FEATURE_UNIT_DESCRIPTOR_INIT(UAC_MIC_FU_ID, 0x01, 0x01, UAC_MIC_CTRL),
    AUDIO_AC_OUTPUT_TERMINAL_DESCRIPTOR_INIT(0x03, AUDIO_TERMINAL_STREAMING, UAC_MIC_FU_ID),

    AUDIO_AC_INPUT_TERMINAL_DESCRIPTOR_INIT(0x04, AUDIO_TERMINAL_STREAMING, UAC_SPK_CHANNELS, UAC_CH_ENABLE(UAC_SPK_CHANNELS)),
    AUDIO_AC_FEATURE_UNIT_DESCRIPTOR_INIT(UAC_SPK_FU_ID, 0x04, 0x01, UAC_SPK_CTRL),
    AUDIO_AC_OUTPUT_TERMINAL_DESCRIPTOR_INIT(0x06, AUDIO_OUTTERM_SPEAKER, UAC_SPK_FU_ID),

    AUDIO_AS_FIXED_FREQ_DESCRIPTOR_INIT(UAC_ITF_MIC, 0x04, 0x03, UAC_MIC_CHANNELS,
                                        UAC_MIC_FRAME_BYTES, UAC_MIC_SAMPLE_BITS,
                                        UAC_MIC_IN_EP, 0x05, UAC_MIC_PACKET_BYTES,
                                        UAC_HS_INTERVAL, AUDIO_SAMPLE_FREQ_3B(UAC_MIC_SAMPLE_RATE)),
    AUDIO_AS_FIXED_FREQ_DESCRIPTOR_INIT(UAC_ITF_SPK, 0x05, 0x04, UAC_SPK_CHANNELS,
                                        UAC_SPK_FRAME_BYTES, UAC_SPK_SAMPLE_BITS,
                                        UAC_SPK_OUT_EP, 0x09, UAC_SPK_PACKET_BYTES,
                                        UAC_HS_INTERVAL, AUDIO_SAMPLE_FREQ_3B(UAC_SPK_SAMPLE_RATE)),
};

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    USB_DEVICE_MFR_STRING,
    USB_DEVICE_PRODUCT_STRING,
    USB_DEVICE_SERIAL_STRING,
    "UAC Microphone",
    "UAC Speaker",
};

static struct usbd_interface audio_control_intf;
static struct usbd_interface audio_mic_intf;
static struct usbd_interface audio_spk_intf;
static struct audio_entity_info audio_entity_table[] = {
    { .bEntityId = UAC_MIC_FU_ID, .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT, .ep = UAC_MIC_IN_EP },
    { .bEntityId = UAC_SPK_FU_ID, .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT, .ep = UAC_SPK_OUT_EP },
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;
    if (index >= (sizeof(string_descriptors) / sizeof(string_descriptors[0]))) {
        return NULL;
    }
    return string_descriptors[index];
}

static const struct usb_descriptor uac_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

void uac_descriptors_register(uint8_t busid)
{
    usbd_desc_register(busid, &uac_descriptor);
    usbd_add_interface(busid,
                       usbd_audio_init_intf(busid, &audio_control_intf,
                                            0x0100, audio_entity_table, 2));
    usbd_add_interface(busid,
                       usbd_audio_init_intf(busid, &audio_mic_intf,
                                            0x0100, audio_entity_table, 2));
    usbd_add_interface(busid,
                       usbd_audio_init_intf(busid, &audio_spk_intf,
                                            0x0100, audio_entity_table, 2));
}
