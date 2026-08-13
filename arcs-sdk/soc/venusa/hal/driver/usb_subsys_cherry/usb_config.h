/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H
#include "log_print.h"


//int xPortIsInsideInterrupt(void)
//{
   // return vApplicationInIrq();
//}

/* ================ USB common Configuration ================ */
#define CONFIG_USB_PRINTF(...) CLOG(__VA_ARGS__)
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO//USB_DBG_LOG

/* Enable print with color */
#define CONFIG_USB_PRINTF_COLOR_ENABLE

/* data align size when use dma or use dcache */
#define CONFIG_USB_ALIGN_SIZE 4

// #define CONFIG_USB_DCACHE_ENABLE

/* attribute data into no cache ram */
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".fast.bss")))

/* use usb_memcpy default for high performance but cost more flash memory.
 * And, arm libc has a bug that memcpy() may cause data misalignment when the size is not a multiple of 4.
*/
// #define CONFIG_USB_MEMCPY_DISABLE

/* ================= USB Device Stack Configuration ================ */

/* Ep0 in and out transfer buffer */
#ifndef CONFIG_USBDEV_REQUEST_BUFFER_LEN
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 64
#endif

#define CONFIG_USBDEV_EPX_BUFFER_MINLEN  64

/* Setup packet log for debug */
// #define CONFIG_USBDEV_SETUP_LOG_PRINT

/* Send ep0 in data from user buffer instead of copying into ep0 reqdata
 * Please note that user buffer must be aligned with CONFIG_USB_ALIGN_SIZE
*/
#define CONFIG_USBDEV_EP0_INDATA_NO_COPY

/* enable advance desc register api */
#define CONFIG_USBDEV_ADVANCE_DESC

#define CONFIG_USBDEV_MSC_MAX_LUN 1
#define CONFIG_USBDEV_MSC_MAX_BUFSIZE 32768  /* 32KB: Maximum DPB burst throughput configuration */
#define CONFIG_USBDEV_MSC_MANUFACTURER_STRING "ListenAI"
#define CONFIG_USBDEV_MSC_PRODUCT_STRING "VENUSA"
#define CONFIG_USBDEV_MSC_VERSION_STRING "0.01"

/* move msc read & write from isr to thread */
#define CONFIG_USBDEV_MSC_THREAD
#ifdef CONFIG_USBDEV_MSC_THREAD
#define CONFIG_USBDEV_MSC_PRIO 5
#define CONFIG_USBDEV_MSC_STACKSIZE 2048  /* Increased for 16KB buffer local ops */
#else
/* move msc read & write from isr to while(1), you should call usbd_msc_polling in while(1) */
#define CONFIG_USBDEV_MSC_POLLING
#endif//CONFIG_USBDEV_MSC_THREAD

/* ================ USB Device Port Configuration ================*/
#define CONFIG_USBDEV_MAX_BUS 1 // for now, bus num must be 1 except hpm ip
#define CONFIG_USBDEV_EP_NUM 8

// #define CONFIG_USBDEV_SOF_ENABLE

/* When your chip hardware supports high-speed and wants to initialize it in high-speed mode, 
the relevant IP will configure the internal or external high-speed PHY according to CONFIG_USB_HS. */
#define CONFIG_USB_HS

/* ================ USB HOST Stack Configuration ================== */

#define CONFIG_USBHOST_MAX_RHPORTS          1
#define CONFIG_USBHOST_MAX_EXTHUBS          1
#define CONFIG_USBHOST_MAX_EHPORTS          4
#ifndef CONFIG_USBHOST_MAX_INTERFACES
#define CONFIG_USBHOST_MAX_INTERFACES       8
#endif

#ifndef CONFIG_USBHOST_MAX_INTF_ALTSETTINGS
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 8
#endif

#ifndef CONFIG_USBHOST_MAX_ENDPOINTS
#define CONFIG_USBHOST_MAX_ENDPOINTS        4
#endif

#define CONFIG_USBHOST_MAX_CDC_ACM_CLASS 4
#define CONFIG_USBHOST_MAX_HID_CLASS     4
#define CONFIG_USBHOST_MAX_MSC_CLASS     2
#define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
#define CONFIG_USBHOST_MAX_VIDEO_CLASS   1

#define CONFIG_USBHOST_DEV_NAMELEN 16

#ifndef CONFIG_USBHOST_PSC_PRINTF
#define CONFIG_USBHOST_PSC_PRINTF(...)
#endif

#ifndef CONFIG_USBHOST_PSC_PRIO
#define CONFIG_USBHOST_PSC_PRIO 0
#endif
#ifndef CONFIG_USBHOST_PSC_STACKSIZE
#define CONFIG_USBHOST_PSC_STACKSIZE 2048
#endif

/* Ep0 max transfer buffer */
#ifndef CONFIG_USBHOST_REQUEST_BUFFER_LEN
#define CONFIG_USBHOST_REQUEST_BUFFER_LEN 512
#endif

/* Allow parsing of huge UVC descriptor trees */
#ifndef CONFIG_USBHOST_MAX_EXTDESC_TAB_SZ
#define CONFIG_USBHOST_MAX_EXTDESC_TAB_SZ 256
#endif

/* UVC class settings */
#ifndef CONFIG_USBHOST_VIDEO_MAX_FRAMES
#define CONFIG_USBHOST_VIDEO_MAX_FRAMES   3
#endif

#ifndef CONFIG_USBHOST_VIDEO_MAX_FORMATS
#define CONFIG_USBHOST_VIDEO_MAX_FORMATS  3
#endif

#ifndef CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
#define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT 500
#endif

#ifndef CONFIG_USBHOST_MSC_TIMEOUT
#define CONFIG_USBHOST_MSC_TIMEOUT 5000
#endif

/* This parameter affects usb performance, and depends on (TCP_WND)tcp eceive windows size,
 * you can change to 2K ~ 16K and must be larger than TCP RX windows size in order to avoid being overflow.
 */
#ifndef CONFIG_USBHOST_CDC_NCM_ETH_MAX_RX_SIZE
#define CONFIG_USBHOST_CDC_NCM_ETH_MAX_RX_SIZE (2048)
#endif
/* Because lwip do not support multi pbuf at a time, so increasing this variable has no performance improvement */
#ifndef CONFIG_USBHOST_CDC_NCM_ETH_MAX_TX_SIZE
#define CONFIG_USBHOST_CDC_NCM_ETH_MAX_TX_SIZE (2048)
#endif

/* ================ USB Host Port Configuration ==================*/
#ifndef CONFIG_USBHOST_MAX_BUS
#define CONFIG_USBHOST_MAX_BUS 1
#endif

#ifndef CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USBHOST_PIPE_NUM 10
#endif

/* ================ USB Dcache Configuration ==================*/
#ifdef CONFIG_USB_DCACHE_ENABLE
/* style 1*/
void usb_dcache_clean(uintptr_t addr, uint32_t size);
void usb_dcache_invalidate(uintptr_t addr, uint32_t size);
void usb_dcache_flush(uintptr_t addr, uint32_t size);

/* style 2*/
// #define usb_dcache_clean(addr, size)
// #define usb_dcache_invalidate(addr, size)
// #define usb_dcache_flush(addr, size)
#endif

#endif
