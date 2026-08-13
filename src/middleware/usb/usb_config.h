/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_USB_CHERRY_CONFIG_H
#define APP_USB_CHERRY_CONFIG_H

#include <stdio.h>

#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_WARNING
#endif

#define CONFIG_USB_PRINTF_COLOR_ENABLE

#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

#define USB_NOCACHE_RAM_SECTION __attribute__((section(".fast.bss")))

#ifndef CONFIG_USBDEV_REQUEST_BUFFER_LEN
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 512
#endif

#ifndef CONFIG_USBDEV_EPX_BUFFER_MINLEN
#define CONFIG_USBDEV_EPX_BUFFER_MINLEN 64
#endif

#define CONFIG_USBDEV_EP0_INDATA_NO_COPY
#define CONFIG_USBDEV_ADVANCE_DESC

#define CONFIG_USBDEV_MSC_MAX_LUN 1
#define CONFIG_USBDEV_MSC_MANUFACTURER_STRING "ListenAI"
#define CONFIG_USBDEV_MSC_PRODUCT_STRING      "VoiceAssistant"
#define CONFIG_USBDEV_MSC_VERSION_STRING      "1.0"

/* Keep block I/O out of the USB interrupt and service it from app_usb. */
#define CONFIG_USBDEV_MSC_POLLING

#define CONFIG_USBDEV_MAX_BUS  1
#define CONFIG_USBDEV_EP_NUM   8
#define CONFIG_USB_MUSB_EP_NUM 8

#if defined(CONFIG_CHERRYUSB_DEVICE_SPEED_HS)
#define CONFIG_USB_HS
#endif

#endif /* APP_USB_CHERRY_CONFIG_H */
