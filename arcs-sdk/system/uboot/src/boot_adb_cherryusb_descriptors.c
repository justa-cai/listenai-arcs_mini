#include <stdbool.h>
#include <stdio.h>

#include "usbd_adb.h"
#include "usbd_core.h"

extern const char *device_id_str_get(void);

#define BOOT_ADB_CHERRYUSB_IN_EP  0x83
#define BOOT_ADB_CHERRYUSB_OUT_EP 0x02

#ifdef CONFIG_USB_HS
#define BOOT_ADB_CHERRYUSB_EP_MPS 512
#else
#define BOOT_ADB_CHERRYUSB_EP_MPS 64
#endif

#define BOOT_ADB_CHERRYUSB_VID 0x12D1
#define BOOT_ADB_CHERRYUSB_PID 0x107E
#define BOOT_ADB_CHERRYUSB_DESC_SIZE 23
#define BOOT_ADB_CHERRYUSB_CONFIG_SIZE (9 + BOOT_ADB_CHERRYUSB_DESC_SIZE)

enum {
    BOOT_ADB_CHERRYUSB_ITF_ADB = 0,
    BOOT_ADB_CHERRYUSB_ITF_TOTAL,
};

static const uint8_t boot_adb_cherryusb_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0,
                               0x00, 0x00, 0x00,
                               BOOT_ADB_CHERRYUSB_VID, BOOT_ADB_CHERRYUSB_PID,
                               0x0100,
                               0x01)
};

static const uint8_t boot_adb_cherryusb_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(BOOT_ADB_CHERRYUSB_CONFIG_SIZE,
                               BOOT_ADB_CHERRYUSB_ITF_TOTAL,
                               0x01,
                               USB_CONFIG_BUS_POWERED,
                               100),
    ADB_DESCRIPTOR_INIT(BOOT_ADB_CHERRYUSB_ITF_ADB,
                        BOOT_ADB_CHERRYUSB_IN_EP,
                        BOOT_ADB_CHERRYUSB_OUT_EP,
                        BOOT_ADB_CHERRYUSB_EP_MPS),
};

static const uint8_t boot_adb_cherryusb_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
};

static const uint8_t *boot_adb_cherryusb_device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return boot_adb_cherryusb_device_descriptor;
}

static const uint8_t *boot_adb_cherryusb_config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return boot_adb_cherryusb_config_descriptor;
}

static const uint8_t *boot_adb_cherryusb_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return boot_adb_cherryusb_quality_descriptor;
}

static const char *boot_adb_cherryusb_string_descriptor_callback(uint8_t speed,
                                                                 uint8_t index)
{
    static const char lang_id[] = { 0x09, 0x04 };

    (void)speed;

    switch (index) {
    case 0:
        return lang_id;
    case 1:
        return "ListenAi";
    case 2:
        return "ARCS Boot Recovery";
    case 3: {
        static char sn[32] = {0};
        snprintf(sn, sizeof(sn), "BOOT-%s", device_id_str_get());
        return sn;
    }
    case 4:
        return "ADB Interface";
    default:
        return NULL;
    }
}

const struct usb_descriptor boot_adb_cherryusb_descriptor = {
    .device_descriptor_callback = boot_adb_cherryusb_device_descriptor_callback,
    .config_descriptor_callback = boot_adb_cherryusb_config_descriptor_callback,
    .device_quality_descriptor_callback = boot_adb_cherryusb_quality_descriptor_callback,
    .string_descriptor_callback = boot_adb_cherryusb_string_descriptor_callback,
};

const uint8_t boot_adb_cherryusb_in_ep = BOOT_ADB_CHERRYUSB_IN_EP;
const uint8_t boot_adb_cherryusb_out_ep = BOOT_ADB_CHERRYUSB_OUT_EP;
