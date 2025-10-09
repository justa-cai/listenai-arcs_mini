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

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

//#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED) //BSD: device, full-speed
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED) //BSD: device, high-speed (USB2.0)

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS               OPT_OS_NONE
#endif

// CFG_TUSB_DEBUG is defined by compiler in DEBUG build
#define CFG_TUSB_DEBUG           1 //2 //0 //
//#error "use my own tusb_config.h" //TEST ONLY!!

//BSD: RAM_XXX indicates that code/data are located in RAM,
// code may run faster than on flash, data can be modified
//FIXME: SHOULD accord with linker script (.ld)!!
//#define RAM_CODE __attribute__((section (".data")))
#define RAM_DATA    __attribute__((section (".data")))
#define RAM_BSS     __attribute__((section (".bss")))
#define FAST_CODE   __attribute__((section (".text.ap_ilm")))
#define FAST_DATA   __attribute__((section (".data.ap_dlm")))
#define FAST_BSS    __attribute__((section (".bss.ap_dlm")))

// The memory space that can be accessed by USB DMAC
#ifndef USB_DMA_SPACE
#define USB_DMA_SPACE  __attribute__ ((section (".data")))
#endif

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION        USB_DMA_SPACE //BSD:
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

// BSD define.
// EP0 status stage is always handled by USB controller and its driver.
// It SHOULD NOT be touched by tinyUSB for some USB controllers, e.g. MUSB
#define CFG_TUD_IGNORE_EP0_STATUS_STAGE     1

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64 //TODO: change it? BSD 2021.12.6.
#endif

//------------- CONTROLLER -------------//
// configure SOF_CNT feature, that is, user can specify
// an interrupt is triggered on receiving how many SOF packets...
#define CONFIG_SOF_CNT              0 // 1
#define SOF_CNT_OVFLOW_THRESHOLD    32 // 8 // *125um

// configure Debug EP if implemented (two extra IN EP & OUT EP are dedicated to debug only!!)
#define CONFIG_DBG_EP   0 // 1

// enable Double Packet Buffering for EP?
// NOTE: SHOULD define CONFIG_DPB_IN as 0 for TX DPB feature CANNOT work as yet!!
#define CONFIG_DPB_IN   0 //1 // for IN EP
#define CONFIG_DPB_OUT  1 //0 // for OUT EP

//------------- CLASS -------------//
#ifndef CFG_TUD_AUDIO
#define CFG_TUD_AUDIO             0 //1 // BSD: enable UAC
#endif

#ifndef CFG_TUD_HID
#define CFG_TUD_HID               0 //1 // BSD: count of HID devices
#endif

#ifndef CFG_TUD_CDC
#define CFG_TUD_CDC               0 //1 // BSD: count of CDC devices
#endif

#ifndef CFG_TUD_BTH
#define CFG_TUD_BTH               0 // BSD: disable BT
#endif

#ifndef CFG_TUD_MSC
#define CFG_TUD_MSC               1 //0 // BSD: enable MSC
#endif

#ifndef CFG_TUD_MIDI
#define CFG_TUD_MIDI              0
#endif

#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR            0 // BSD: count of vendor-defined class
#endif


//----------------------- Audio CFG -----------------------
#if CFG_TUD_AUDIO

// size of UAC internal buffer for control requests
#define CFG_TUD_AUDIO_CTRL_BUF_SIZE     CFG_TUD_ENDPOINT0_SIZE

// Number of Standard AS Interface Descriptors (4.9.1), 1 for MIC, 1 for SPK
// CFG_TUD_AUDIO_N_AS_INT = 1, indicates either MIC or SPK (only 1 Audio Stream) exists
// CFG_TUD_AUDIO_N_AS_INT = 2, indicates both MIC and SPK (2 Audio Streams) exist
#define CFG_TUD_AUDIO_N_AS_INT      2 // MIC + SPK //FIXME: change it

#define DEF_AUDIO_TX_SAMP_FREQ                  16000 //for MIC, BSD added, Change it!
#define DEF_AUDIO_RX_SAMP_FREQ                  48000 //16000 //for SPK, BSD added, Change it!

// Audio IN (MIC, USB TX) and OUT (SPK, USB RX) channels
#define CFG_TUD_AUDIO_N_CHANNELS_TX             2 // 1 // 2 channel(s) //FIXME: change it!
#define CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX     2 // 16 bits //FIXME: change it!

#define CFG_TUD_AUDIO_N_CHANNELS_RX             2 // 1 // 4 // 1 channel(s) //FIXME: change it!
#define CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX     2 // 16bits //FIXME: change it!

// 16 Samples (16 kHz) x M Bytes/Sample x N Channels  //FIXME: change it!
// DEF_AUDIO_TX_SAMP_FREQ / 1000 = 16
#define CFG_TUD_AUDIO_EPSIZE_IN     (DEF_AUDIO_TX_SAMP_FREQ / 1000 * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_N_CHANNELS_TX)
//BSD FIXME: definition of CFG_TUD_AUDIO_TX_FIFO_SIZE as 0 can decrease internal RAM usage...
//#define CFG_TUD_AUDIO_TX_FIFO_SIZE  CFG_TUD_AUDIO_EPSIZE_IN
//#define CFG_TUD_AUDIO_TX_FIFO_COUNT 1 // FIFO = Ring Buffer? It seems meaningful that only 1 Ring Buffer for TX?
#define CFG_TUD_AUDIO_TX_FIFO_SIZE  0
#define CFG_TUD_AUDIO_TX_FIFO_COUNT 0

// 48 Samples (48 kHz) x M Bytes/Sample x N Channels  //FIXME: change it!
// DEF_AUDIO_RX_SAMP_FREQ / 1000 = 48
#define CFG_TUD_AUDIO_EPSIZE_OUT    (DEF_AUDIO_RX_SAMP_FREQ / 1000 * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_N_CHANNELS_RX)
 //BSD FIXME: definition of CFG_TUD_AUDIO_RX_FIFO_SIZE as 0 can decrease internal RAM usage...
//#define CFG_TUD_AUDIO_RX_FIFO_SIZE  CFG_TUD_AUDIO_EPSIZE_OUT
//#define CFG_TUD_AUDIO_RX_FIFO_COUNT 1 // FIFO = Ring Buffer? It seems meaningful that only 1 Ring Buffer for RX?
#define CFG_TUD_AUDIO_RX_FIFO_SIZE  0
#define CFG_TUD_AUDIO_RX_FIFO_COUNT 0

// Audio format type
#define CFG_TUD_AUDIO_FORMAT_TYPE_TX            AUDIO_FORMAT_TYPE_I
#define CFG_TUD_AUDIO_FORMAT_TYPE_RX            AUDIO_FORMAT_TYPE_I

// Audio format type I specifications
#define CFG_TUD_AUDIO_FORMAT_TYPE_I_TX              AUDIO_DATA_FORMAT_TYPE_I_PCM
#define CFG_TUD_AUDIO_FORMAT_TYPE_I_RX              AUDIO_DATA_FORMAT_TYPE_I_PCM

#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP        0 // 1 //FIXME:

//BSD: Whether or not add IAD in prior to the first audio interface descriptor for UAC1.0?
//NOTE: Hosts that don’t support the IAD ignore it.
// Windows began supporting the descriptor with Windows XP SP2.
#define CFG_TUD_AUDIO_USE_IAD                   1 // 0

#endif // CFG_TUD_AUDIO

//----------------------- HID CFG -----------------------
#if CFG_TUD_HID

// HID buffer size Should be sufficient to hold ID (if any) + Data
//BSD: please change it according to max.report length (including Report ID). NOT used by UHID!!
#define CFG_TUD_HID_EP_BUFSIZE     4 // 16

#endif

//----------------------- CDC CFG -----------------------
#if CFG_TUD_CDC

//BSD: use CDC internal SW FIFO for buffering?
// STRONGLY RECOMMEND - DON'T use CDC internal SW FIFO, for it has no benefit,
// and just consumes RAM space and reduces efficiency of data transfer!
//
// Data path if CDC internal FIFO used (EP buffer size = max packet size, the same below):
//  EP HW FIFO <-> (CDC) EP buffer <-> (CDC) EP SW FIFO <-> user buffer
//
// Data path if NO CDC internal FIFO:
//  EP HW FIFO <-> (CDC) EP buffer <-> user buffer
//
#define CFG_TUD_CDC_USE_FIFO  0 // 1

#if CFG_TUD_CDC_USE_FIFO
// CDC FIFO size of TX and RX //BSD: FIXME: BETTER remove damned TX/RX FIFO!!
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif // CFG_TUD_CDC_USE_FIFO

// CDC Endpoint transfer buffer size, more is faster
#define CFG_TUD_CDC_EP_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)

#endif

//----------------------- MSC CFG -----------------------
#if CFG_TUD_MSC

// use EP buffer provided by user if 1, ignore internal MSC Buffer of MSC class
#define MSC_USER_EP_BUF             1 //0 // default 0
// copy data directly from user buffer to EP's HW FIFO, ignore EP buffer (whether user_provided or internal)
#define MSC_USER_BUF_DIRECT_READ    1 //0 // default 0
// copy data directly from EP's HW FIFO to user buffer, ignore EP buffer (whether user_provided or internal)
#define MSC_USER_BUF_DIRECT_WRITE   1 //0 // default 0

// internal MSC Buffer size of Device Mass storage, use 4KB (?) for better cost-effectiveness
#define CFG_TUD_MSC_EP_BUFSIZE      512 //(512 * 8) //(512 * 16) //(512 * 32) //(512 * 64) //

#endif

//----------------------- VENDOR CFG -----------------------
#if CFG_TUD_VENDOR

//BSD: use internal SW FIFO of vendor-defined class for buffering?
// STRONGLY RECOMMEND - DON'T use internal SW FIFO, for it has no benefit,
// and just consumes RAM space and reduces efficiency of data transfer!
//
// Data path if internal FIFO used (EP buffer size = max packet size, the same below):
//  EP HW FIFO <-> EP buffer <-> EP SW FIFO <-> user buffer
//
// Data path if NO internal FIFO:
//  EP HW FIFO <-> EP buffer <-> user buffer
//
#define CFG_TUD_VENDOR_USE_FIFO     0 //1 //

#if CFG_TUD_VENDOR_USE_FIFO

// Vendor FIFO size of TX and RX
#define CFG_TUD_VENDOR_RX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_VENDOR_TX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

#else // !CFG_TUD_VENDOR_USE_FIFO

//BSD: max. count of IN endpoint and OUT endpoint
// belonging to a vendor-defined device/interface
#define MAX_IN_EP_COUNT     2 //3 //4 //
#define MAX_OUT_EP_COUNT    2 //3 //4 //

//BSD: Read/Write endpoint HW FIFO directly or not?
// The DIRECT RW mode can remove memory copy operation between user buffer and EP buffer,
// decrease HW interrupt count, and therefore improve RW performance.
//
//NOTE: the requisite of DIRECT RW mode is - user buffer MUST be accessible by USB built-in DMAC!
//      That means user buffer should be limited as CFG_TUSB_MEM_SECTION and CFG_TUSB_MEM_ALIGN.
//
#define DIRECT_RW_EP    1 // 0  (default 0)

#endif // CFG_TUD_VENDOR_USE_FIFO

#endif // CFG_TUD_VENDOR

//----------------------- BTH CFG -----------------------
#if CFG_TUD_BTH

#define CFG_TUD_BTH_ISO_ALT_COUNT   7 //6 //FIXME: change it

#endif


//BSD: whether to use internal control buffer of max. packet size or not?
#define CFG_INTERNAL_CTRL_BUF       0 //1 //

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
