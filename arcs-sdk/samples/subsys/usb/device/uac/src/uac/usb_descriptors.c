/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"
#include "usb_descriptors.h"

#define USB_VID 0xCAFE
#define USB_PID 0x4020

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

enum {
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_MIC_STREAMING,
    ITF_NUM_SPK_STREAMING,
    ITF_NUM_TOTAL,
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + UAC_DESC_LEN)

#define UAC_DESC_FEATURE_UNIT(_unitid, _srcid, _channels, ...) \
    UAC_FEATURE_UNIT_LEN(_channels), TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, \
    _unitid, _srcid, __VA_ARGS__, 0x00

#define UAC_AUDIO_CONTROL_DESCRIPTOR() \
    TUD_AUDIO_DESC_IAD(ITF_NUM_AUDIO_CONTROL, 0x03, 0x04), \
    TUD_AUDIO_DESC_STD_AC(ITF_NUM_AUDIO_CONTROL, 0x00, 0x04), \
    TUD_AUDIO_DESC_CS_AC(0x0200, AUDIO_FUNC_IO_BOX, UAC_AC_ENTITIES_LEN, \
        AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS), \
    TUD_AUDIO_DESC_CLK_SRC(UAC_MIC_CLK_ID, AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, \
        (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), UAC_MIC_IT_ID, 0x00), \
    TUD_AUDIO_DESC_INPUT_TERM(UAC_MIC_IT_ID, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 0x00, \
        UAC_MIC_CLK_ID, UAC_MIC_CHANNELS, UAC_CH_ENABLE(UAC_MIC_CHANNELS), 0x00, \
        AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS, 0x00), \
    UAC_DESC_FEATURE_UNIT(UAC_MIC_FU_ID, UAC_MIC_IT_ID, UAC_MIC_CHANNELS, UAC_MIC_FEATURE_CONTROLS), \
    TUD_AUDIO_DESC_OUTPUT_TERM(UAC_MIC_OT_ID, AUDIO_TERM_TYPE_USB_STREAMING, UAC_MIC_IT_ID, \
        UAC_MIC_FU_ID, UAC_MIC_CLK_ID, 0x0000, 0x00), \
    TUD_AUDIO_DESC_CLK_SRC(UAC_SPK_CLK_ID, AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, \
        (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), UAC_SPK_IT_ID, 0x00), \
    TUD_AUDIO_DESC_INPUT_TERM(UAC_SPK_IT_ID, AUDIO_TERM_TYPE_USB_STREAMING, 0x00, \
        UAC_SPK_CLK_ID, UAC_SPK_CHANNELS, UAC_CH_ENABLE(UAC_SPK_CHANNELS), 0x00, 0x0000, 0x00), \
    UAC_DESC_FEATURE_UNIT(UAC_SPK_FU_ID, UAC_SPK_IT_ID, UAC_SPK_CHANNELS, UAC_SPK_FEATURE_CONTROLS), \
    TUD_AUDIO_DESC_OUTPUT_TERM(UAC_SPK_OT_ID, AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, UAC_SPK_IT_ID, \
        UAC_SPK_FU_ID, UAC_SPK_CLK_ID, 0x0000, 0x00)

#define UAC_MIC_STREAMING_DESCRIPTOR() \
    TUD_AUDIO_DESC_STD_AS_INT(ITF_NUM_MIC_STREAMING, 0x00, 0x00, 0x05), \
    TUD_AUDIO_DESC_STD_AS_INT(ITF_NUM_MIC_STREAMING, 0x01, 0x01, 0x00), \
    TUD_AUDIO_DESC_CS_AS_INT(UAC_MIC_OT_ID, AUDIO_CTRL_NONE, AUDIO_FORMAT_TYPE_I, \
        AUDIO_DATA_FORMAT_TYPE_I_PCM, UAC_MIC_CHANNELS, UAC_CH_ENABLE(UAC_MIC_CHANNELS), 0x00), \
    TUD_AUDIO_DESC_TYPE_I_FORMAT(UAC_MIC_SAMPLE_BYTES, UAC_MIC_SAMPLE_BITS), \
    TUD_AUDIO_DESC_STD_AS_ISO_EP(UAC_MIC_EP_IN, \
        (uint8_t)(TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), \
        UAC_MIC_PACKET_BYTES, 0x01), \
    TUD_AUDIO_DESC_CS_AS_ISO_EP(AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, \
        AUDIO_CTRL_NONE, AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000)

#define UAC_SPK_STREAMING_DESCRIPTOR() \
    TUD_AUDIO_DESC_STD_AS_INT(ITF_NUM_SPK_STREAMING, 0x00, 0x00, 0x06), \
    TUD_AUDIO_DESC_STD_AS_INT(ITF_NUM_SPK_STREAMING, 0x01, 0x02, 0x00), \
    TUD_AUDIO_DESC_CS_AS_INT(UAC_SPK_IT_ID, AUDIO_CTRL_NONE, AUDIO_FORMAT_TYPE_I, \
        AUDIO_DATA_FORMAT_TYPE_I_PCM, UAC_SPK_CHANNELS, UAC_CH_ENABLE(UAC_SPK_CHANNELS), 0x00), \
    TUD_AUDIO_DESC_TYPE_I_FORMAT(UAC_SPK_SAMPLE_BYTES, UAC_SPK_SAMPLE_BITS), \
    TUD_AUDIO_DESC_STD_AS_ISO_EP(UAC_SPK_EP_OUT, \
        (uint8_t)(TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), \
        UAC_SPK_PACKET_BYTES, 0x01), \
    TUD_AUDIO_DESC_CS_AS_ISO_EP(AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, \
        AUDIO_CTRL_NONE, AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000), \
    TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(UAC_SPK_EP_FB, 4, 0x01)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    UAC_AUDIO_CONTROL_DESCRIPTOR(),
    UAC_MIC_STREAMING_DESCRIPTOR(),
    UAC_SPK_STREAMING_DESCRIPTOR(),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_AUDIO,
    STRID_MIC,
    STRID_SPK,
};

static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },
    "ListenAI",
    "ListenAI TinyUSB UAC2",
    "2026060502",
    "UAC Audio",
    "UAC Microphone",
    "UAC Speaker",
};

static uint16_t desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;

    if (index == STRID_LANGID) {
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 32) {
            chr_count = 32;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint8_t)str[i];
        }
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2U * chr_count + 2U));
    return desc_str;
}
