/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#include "sysutils.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)
#define CFG_TUSB_MCU          OPT_MCU_LS566X
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG        0

#define CFG_TUSB_MEM_SECTION __psram_bss__

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
// CFG_TUSB_MCU is defined by compiler in command line
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

// CFG_TUSB_OS is defined by compiler in command line
#ifndef CFG_TUSB_OS
#error CFG_TUSB_OS must be defined
#endif

// CFG_TUSB_DEBUG is defined by compiler in command line
#ifndef CFG_TUSB_DEBUG
#error CFG_TUSB_DEBUG must be defined
#endif

// Enable Device stack
#define CFG_TUD_ENABLED 1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED OPT_MODE_HIGH_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

//------------- CLASS -------------//
#if CONFIG_APP_USB_CDC_ENABLE
#define CFG_TUD_CDC    1
#else
#define CFG_TUD_CDC    0
#endif
#define CFG_TUD_MSC    1
#define CFG_TUD_HID    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0
#if CONFIG_APP_USB_AUDIO_ENABLE
#define CFG_TUD_AUDIO  1
#else
#define CFG_TUD_AUDIO  0
#endif

#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE              16000

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                 TUD_AUDIO_MIC_FOUR_CH_DESC_LEN

#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                 1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ              64

#define CFG_TUD_AUDIO_ENABLE_EP_IN                    1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX    2
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX            4
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_INTERVAL           4
#define CFG_TUD_AUDIO_EP_SZ_IN                        (((((CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE *                                         \
                                                          (1U << (CFG_TUD_AUDIO_FUNC_1_EP_IN_INTERVAL - 1U))) + 7999U) / 8000U) + 1U) \
                                                        * CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX                                  \
                                                        * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)

#define CFG_TUD_AUDIO_ENABLE_ENCODING                 0
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL              1

#if CFG_TUD_AUDIO_ENABLE_ENCODING

#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX             CFG_TUD_AUDIO_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ          CFG_TUD_AUDIO_EP_SZ_IN

#define CFG_TUD_AUDIO_ENABLE_TYPE_I_ENCODING          1
#define CFG_TUD_AUDIO_FUNC_1_CHANNEL_PER_FIFO_TX      2         // One I2S stream contains two channels, each stream is saved within one support FIFO - this value is currently fixed, the driver does not support a changing value
#define CFG_TUD_AUDIO_FUNC_1_N_TX_SUPP_SW_FIFO        (CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX / CFG_TUD_AUDIO_FUNC_1_CHANNEL_PER_FIFO_TX)
#define CFG_TUD_AUDIO_FUNC_1_TX_SUPP_SW_FIFO_SZ       (TUD_OPT_HIGH_SPEED ? 32 : 4) * (CFG_TUD_AUDIO_EP_SZ_IN / CFG_TUD_AUDIO_FUNC_1_N_TX_SUPP_SW_FIFO) // Example write FIFO every 1ms, so it should be 8 times larger for HS device

#else

#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX             CFG_TUD_AUDIO_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ          (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_EP_SZ_IN * 16 * 2 // Example write FIFO every 16ms, so it should be 8 times larger for HS device

#endif

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

// CDC Endpoint transfer buffer size, more is faster
#define CFG_TUD_CDC_EP_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

// MSC Buffer size of Device Mass storage
#define CFG_TUD_MSC_EP_BUFSIZE 512
#define CFG_TUD_TASK_QUEUE_SZ  512

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
