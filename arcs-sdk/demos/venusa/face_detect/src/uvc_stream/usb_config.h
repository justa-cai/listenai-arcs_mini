/*
 * CherryUSB configuration for the VenusA face_detect UVC device.
 *
 * Keep this file in the demo so UVC tuning does not change the shared
 * CherryUSB module defaults used by other applications.
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)
#define CONFIG_USB_DBG_LEVEL   USB_DBG_INFO

#define CONFIG_USB_PRINTF_COLOR_ENABLE
#define CONFIG_USB_ALIGN_SIZE 4

#define USB_NOCACHE_RAM_SECTION __attribute__((section(".fast.bss")))

/* UVC configuration descriptors and PROBE/COMMIT controls exceed 64 bytes. */
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 512
#define CONFIG_USBDEV_EPX_BUFFER_MINLEN  64

#define CONFIG_USBDEV_EP0_INDATA_NO_COPY
#define CONFIG_USBDEV_ADVANCE_DESC

#define CONFIG_USBDEV_MAX_BUS  1
#define CONFIG_USBDEV_EP_NUM   8
#define CONFIG_USB_MUSB_EP_NUM 8

#define CONFIG_USB_HS

#endif
