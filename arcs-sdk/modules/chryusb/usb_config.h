/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

/* ================ USB common Configuration ================ */
#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)
#define CONFIG_USB_DBG_LEVEL USB_DBG_WARNING

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
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 64

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
#define CONFIG_USBDEV_MSC_MAX_BUFSIZE 512
#define CONFIG_USBDEV_MSC_MANUFACTURER_STRING "ListenAI"
#define CONFIG_USBDEV_MSC_PRODUCT_STRING "ARCS"
#define CONFIG_USBDEV_MSC_VERSION_STRING "0.01"

/* move msc read & write from isr to thread */
// #define CONFIG_USBDEV_MSC_THREAD
#ifdef CONFIG_USBDEV_MSC_THREAD
#define CONFIG_USBDEV_MSC_PRIO 5
#define CONFIG_USBDEV_MSC_STACKSIZE 1024
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
