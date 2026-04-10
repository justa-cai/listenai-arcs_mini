/*
 * Copyright (c) 2024, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

/* ================ USB common Configuration ================ */

#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

/* Enable USB Host debug */
#define CONFIG_USBHOST_DEBUG

/* Enable print with color */
#define CONFIG_USB_PRINTF_COLOR_ENABLE

#define CONFIG_USB_DCACHE_ENABLE

/* data align size when use dma or use dcache */
#ifdef CONFIG_USB_DCACHE_ENABLE
#define CONFIG_USB_ALIGN_SIZE 32 // 32 or 64
#else
#define CONFIG_USB_ALIGN_SIZE 4
#endif

/* attribute data into no cache ram */
#define USB_NOCACHE_RAM_SECTION

/* ================ USB HOST Stack Configuration ================== */

#define CONFIG_USBHOST_MAX_RHPORTS          1
#define CONFIG_USBHOST_MAX_EXTHUBS          8
#define CONFIG_USBHOST_MAX_EHPORTS          10
#define CONFIG_USBHOST_MAX_INTERFACES       16
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 2
#define CONFIG_USBHOST_MAX_ENDPOINTS        6

#define CONFIG_USBHOST_MAX_SERIAL_CLASS  1
#define CONFIG_USBHOST_MAX_HID_CLASS     1
#define CONFIG_USBHOST_MAX_MSC_CLASS     1
#define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
#define CONFIG_USBHOST_MAX_VIDEO_CLASS   2

#define CONFIG_USBHOST_DEV_NAMELEN 16

#ifndef CONFIG_USBHOST_PSC_PRIO
#define CONFIG_USBHOST_PSC_PRIO 0
#endif
#ifndef CONFIG_USBHOST_PSC_STACKSIZE
#define CONFIG_USBHOST_PSC_STACKSIZE 2048
#endif

#define CONFIG_USBHOST_GET_STRING_DESC

/* Ep0 max transfer buffer */
#ifndef CONFIG_USBHOST_REQUEST_BUFFER_LEN
#define CONFIG_USBHOST_REQUEST_BUFFER_LEN 512
#endif

#ifndef CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
#define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT 500
#endif

/* ================ USB Host Port Configuration ==================*/
#ifndef CONFIG_USBHOST_MAX_BUS
#define CONFIG_USBHOST_MAX_BUS 1
#endif

/* ---------------- MUSB Configuration ---------------- */
#define CONFIG_USB_MUSB_PIPE_NUM 8

/* ---------------- Video Configuration ---------------- */
/* Bulk chunk size: one URB receive per chunk */
#define CONFIG_USBH_VIDEO_BULK_CHUNK_SIZE 512

#endif
