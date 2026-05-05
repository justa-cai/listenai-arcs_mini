#include "usbd_core.h"
#include "usbd_adb.h"

#include <string.h>

#define ADB_IN_EP  0x83
#define ADB_OUT_EP 0x02

#ifdef CONFIG_USB_HS
#define ADB_EP_MPS 512
#else
#define ADB_EP_MPS 64
#endif

#define USB_VID 0x12D1
#define USB_PID 0x107e
#define ADB_DESC_SIZE 23
#define USB_CONFIG_SIZE_ADB (9 + ADB_DESC_SIZE)

enum {
    ITF_ADB = 0,
    ITF_ADB_TOTAL,
};

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0,
                               0x00, 0x00, 0x00,
                               USB_VID, USB_PID,
                               0x0100,
                               0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE_ADB,
                               ITF_ADB_TOTAL,
                               0x01,
                               USB_CONFIG_BUS_POWERED,
                               100),
    ADB_DESCRIPTOR_INIT(ITF_ADB, ADB_IN_EP, ADB_OUT_EP, ADB_EP_MPS),
};

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    "ListenAi",
    "ScanPen",
    "0123456789abcdef",
    "ADB Interface",
};

#define STRING_DESC_COUNT (sizeof(string_descriptors) / sizeof(string_descriptors[0]))

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= STRING_DESC_COUNT) {
        return NULL;
    }

    return string_descriptors[index];
}

const struct usb_descriptor cherryusb_adb_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

const uint8_t g_adb_in_ep = ADB_IN_EP;
const uint8_t g_adb_out_ep = ADB_OUT_EP;
