/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UAC_TUSB_CONFIG_H
#define UAC_TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED
#endif

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | BOARD_TUD_MAX_SPEED)
#if defined(CONFIG_SOC_VENUSA)
#define CFG_TUSB_MCU          OPT_MCU_LS566X
#else
#define CFG_TUSB_MCU          OPT_MCU_ARCS
#endif
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG        0

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__((aligned(32)))
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

#define CFG_TUD_AUDIO          1
#define CFG_TUD_CDC            0
#define CFG_TUD_MSC            0
#define CFG_TUD_HID            0
#define CFG_TUD_MIDI           0
#define CFG_TUD_VENDOR         0

#include "usb_descriptors.h"

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN              UAC_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT              2
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ           128
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE           UAC_MIC_SAMPLE_RATE
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX UAC_MIC_SAMPLE_BYTES
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX         UAC_MIC_CHANNELS
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX UAC_SPK_SAMPLE_BYTES
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX         UAC_SPK_CHANNELS
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX          UAC_MIC_PACKET_BYTES
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ       (4 * UAC_MIC_PACKET_BYTES)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX         UAC_SPK_PACKET_BYTES
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ      (4 * UAC_SPK_PACKET_BYTES)

#define CFG_TUD_AUDIO_ENABLE_EP_IN                 1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                1
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP           1
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL           1
#define CFG_TUD_AUDIO_ENABLE_ENCODING              0
#define CFG_TUD_AUDIO_ENABLE_DECODING              0

#ifdef __cplusplus
}
#endif

#endif /* UAC_TUSB_CONFIG_H */
