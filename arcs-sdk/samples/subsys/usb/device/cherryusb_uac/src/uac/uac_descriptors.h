/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_UAC_DESCRIPTORS_H
#define CHERRYUSB_UAC_DESCRIPTORS_H

#include <stdbool.h>

#include "usb_config.h"
#include "usbd_core.h"
#include "usbd_audio.h"

#define UAC_MIC_IN_EP     0x81
#define UAC_SPK_OUT_EP    0x02
#define UAC_MIC_FU_ID     0x02
#define UAC_SPK_FU_ID     0x05

#define UAC_MIC_CHANNELS       CONFIG_UAC_MIC_CHANNELS
#define UAC_MIC_SAMPLE_RATE    CONFIG_UAC_MIC_SAMPLE_RATE
#define UAC_MIC_SAMPLE_BITS    CONFIG_UAC_MIC_SAMPLE_BITS
#define UAC_MIC_FRAME_BYTES    (UAC_MIC_SAMPLE_BITS / 8U)
#define UAC_HS_INTERVAL        CONFIG_UAC_HS_INTERVAL
#define UAC_HS_INTERVAL_FRAMES (1U << (UAC_HS_INTERVAL - 1U))
#define UAC_MIC_PACKET_BYTES   ((UAC_MIC_SAMPLE_RATE * UAC_MIC_FRAME_BYTES * \
                                       UAC_MIC_CHANNELS * UAC_HS_INTERVAL_FRAMES) / 8000U)
#define UAC_MIC_RING_BYTES     ((UAC_MIC_SAMPLE_RATE * UAC_MIC_FRAME_BYTES * \
                                       UAC_MIC_CHANNELS * CONFIG_UAC_MIC_RING_MS) / 1000U)

#define UAC_SPK_CHANNELS       CONFIG_UAC_SPK_CHANNELS
#define UAC_SPK_SAMPLE_RATE    CONFIG_UAC_SPK_SAMPLE_RATE
#define UAC_SPK_SAMPLE_BITS    CONFIG_UAC_SPK_SAMPLE_BITS
#define UAC_SPK_FRAME_BYTES    (UAC_SPK_SAMPLE_BITS / 8U)
#define UAC_SPK_PACKET_BYTES   ((UAC_SPK_SAMPLE_RATE * UAC_SPK_FRAME_BYTES * \
                                       UAC_SPK_CHANNELS * UAC_HS_INTERVAL_FRAMES) / 8000U)
#define UAC_SPK_RING_BYTES     ((UAC_SPK_SAMPLE_RATE * UAC_SPK_FRAME_BYTES * \
                                       UAC_SPK_CHANNELS * CONFIG_UAC_SPK_RING_MS) / 1000U)
#define UAC_SPK_PLAY_SAMPLES   ((UAC_SPK_SAMPLE_RATE * CONFIG_UAC_PLAY_BUFFER_MS * \
                                       UAC_SPK_CHANNELS) / 1000U)
#define UAC_SPK_PLAY_BYTES     (UAC_SPK_PLAY_SAMPLES * UAC_SPK_FRAME_BYTES)

#define UAC_TASK_STACK         2048
#define UAC_MIC_TASK_PRIORITY  9
#define UAC_SPK_TASK_PRIORITY  7
#define UAC_VOLUME_DEFAULT_DB  0


#if !defined(CONFIG_USB_HS)
#error "cherryusb_uac requires high-speed USB"
#endif

#if UAC_MIC_PACKET_BYTES > 1024
#error "UAC MIC packet exceeds high-speed endpoint MPS; reduce channels or interval"
#endif

#if UAC_SPK_PACKET_BYTES > 1024
#error "UAC speaker packet exceeds high-speed endpoint MPS"
#endif

#if UAC_MIC_CHANNELS < 1 || UAC_MIC_CHANNELS > 16
#error "CONFIG_UAC_MIC_CHANNELS must be 1..16"
#endif

#if UAC_SPK_CHANNELS < 1 || UAC_SPK_CHANNELS > 2
#error "CONFIG_UAC_SPK_CHANNELS must be 1 or 2"
#endif

#if (UAC_MIC_SAMPLE_BITS != 16 && UAC_MIC_SAMPLE_BITS != 24 && UAC_MIC_SAMPLE_BITS != 32) || \
    (UAC_SPK_SAMPLE_BITS != 16 && UAC_SPK_SAMPLE_BITS != 24 && UAC_SPK_SAMPLE_BITS != 32)
#error "CONFIG_UAC_*_SAMPLE_BITS must be 16, 24 or 32"
#endif

#define UAC_CH_ENABLE(ch) ((uint16_t)((1UL << (ch)) - 1UL))

#if UAC_MIC_CHANNELS == 1
#define UAC_MIC_CTRL      0x03, 0x03
#elif UAC_MIC_CHANNELS == 2
#define UAC_MIC_CTRL      0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 3
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 4
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 5
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 6
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 7
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 8
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 9
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 10
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 11
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 12
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 13
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 14
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#elif UAC_MIC_CHANNELS == 15
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#else
#define UAC_MIC_CTRL      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#endif

#if UAC_SPK_CHANNELS == 1
#define UAC_SPK_CTRL      0x03, 0x03
#else
#define UAC_SPK_CTRL      0x03, 0x03, 0x03
#endif

/*
 * CherryUSB's generic UAC1 macro advertises Sampling Frequency Control in the
 * class-specific endpoint descriptor. This sample exposes one fixed frequency
 * per stream, so keep the control bit clear to avoid host-side quirks while
 * parsing high-speed multi-channel endpoints.
 */
#define AUDIO_AS_FIXED_FREQ_DESCRIPTOR_INIT(bInterfaceNumber, stridx, bTerminalLink, bNrChannels, bSubFrameSize, bBitResolution, bEndpointAddress, bmAttributes, wMaxPacketSize, bInterval, ...) \
    0x09,                            /* bLength */                                                                       \
    USB_DESCRIPTOR_TYPE_INTERFACE,   /* bDescriptorType */                                                               \
    bInterfaceNumber,                /* bInterfaceNumber */                                                              \
    0x00,                            /* bAlternateSetting */                                                             \
    0x00,                            /* bNumEndpoints */                                                                 \
    USB_DEVICE_CLASS_AUDIO,          /* bInterfaceClass */                                                               \
    AUDIO_SUBCLASS_AUDIOSTREAMING,   /* bInterfaceSubClass */                                                            \
    AUDIO_PROTOCOL_UNDEFINED,        /* bInterfaceProtocol */                                                            \
    stridx,                          /* iInterface */                                                                    \
    0x09,                            /* bLength */                                                                       \
    USB_DESCRIPTOR_TYPE_INTERFACE,   /* bDescriptorType */                                                               \
    bInterfaceNumber,                /* bInterfaceNumber */                                                              \
    0x01,                            /* bAlternateSetting */                                                             \
    0x01,                            /* bNumEndpoints */                                                                 \
    USB_DEVICE_CLASS_AUDIO,          /* bInterfaceClass */                                                               \
    AUDIO_SUBCLASS_AUDIOSTREAMING,   /* bInterfaceSubClass */                                                            \
    AUDIO_PROTOCOL_UNDEFINED,        /* bInterfaceProtocol */                                                            \
    0x00,                            /* iInterface */                                                                    \
    0x07,                            /* bLength */                                                                       \
    AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */                                                               \
    AUDIO_STREAMING_GENERAL,         /* bDescriptorSubtype */                                                            \
    bTerminalLink,                   /* bTerminalLink */                                                                 \
    0x01,                            /* bDelay */                                                                        \
    WBVAL(AUDIO_FORMAT_PCM),         /* wFormatTag */                                                                    \
    0x08 + PP_NARG(__VA_ARGS__),     /* bLength */                                                                       \
    AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */                                                               \
    AUDIO_STREAMING_FORMAT_TYPE,     /* bDescriptorSubtype */                                                            \
    AUDIO_FORMAT_TYPE_I,             /* bFormatType */                                                                   \
    bNrChannels,                     /* bNrChannels */                                                                   \
    bSubFrameSize,                   /* bSubFrameSize */                                                                 \
    bBitResolution,                  /* bBitResolution */                                                                \
    (PP_NARG(__VA_ARGS__)/3),        /* bSamFreqType */                                                                  \
    __VA_ARGS__,                     /* tSamFreq */                                                                      \
    0x09,                            /* bLength */                                                                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,    /* bDescriptorType */                                                               \
    bEndpointAddress,                /* bEndpointAddress */                                                              \
    bmAttributes,                    /* bmAttributes */                                                                  \
    WBVAL(wMaxPacketSize),           /* wMaxPacketSize */                                                                \
    bInterval,                       /* bInterval */                                                                     \
    0x00,                            /* bRefresh */                                                                      \
    0x00,                            /* bSynchAddress */                                                                 \
    0x07,                            /* bLength */                                                                       \
    AUDIO_ENDPOINT_DESCRIPTOR_TYPE,  /* bDescriptorType */                                                               \
    AUDIO_ENDPOINT_GENERAL,          /* bDescriptorSubtype */                                                            \
    0x00,                            /* bmAttributes: fixed sampling frequency */                                        \
    0x00,                            /* bLockDelayUnits */                                                               \
    0x00,                            /* wLockDelay */                                                                    \
    0x00

enum {
    UAC_ITF_CONTROL = 0,
    UAC_ITF_MIC,
    UAC_ITF_SPK,
    UAC_ITF_TOTAL,
};

#endif /* CHERRYUSB_UAC_DESCRIPTORS_H */
