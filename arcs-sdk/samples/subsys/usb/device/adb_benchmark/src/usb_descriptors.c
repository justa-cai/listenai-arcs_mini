#include "tusb.h"
#include "log_print.h"

tusb_desc_device_t const desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                        .bDescriptorType = TUSB_DESC_DEVICE,
                                        .bcdUSB = 0x0200,
                                        .bDeviceClass = 0,
                                        .bDeviceSubClass = 0,
                                        .bDeviceProtocol = 0,
                                        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
                                        .idVendor = 0x12D1,
                                        .idProduct = 0x107e,
                                        .bcdDevice = 0x0100,
                                        .iManufacturer = 0x01,
                                        .iProduct = 0x02,
                                        .iSerialNumber = 0x03,
                                        .bNumConfigurations = 0x01};

char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "ListenAi",                 // 1: Manufacturer
    "ScanPen",                  // 2: Product
    "0123456789abcdef",         // 3: Serial
    "ADB Interface",            // 4: Interface
};

#define ADB_INTERFACE_INDEX 0
#define ADB_BULK_OUT_EP     0x02
#define ADB_BULK_IN_EP      0x83

#define ADB_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize)                                                       \
    /* Interface */                                                                                                    \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x42, 0x01, _stridx, /* Endpoint Out */         \
        7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,           /* Endpoint In */          \
        7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)
uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    ADB_DESCRIPTOR(ADB_INTERFACE_INDEX, 4, ADB_BULK_OUT_EP, ADB_BULK_IN_EP, 64),
};

#if TUD_OPT_HIGH_SPEED
uint8_t const desc_hs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    ADB_DESCRIPTOR(ADB_INTERFACE_INDEX, 4, ADB_BULK_OUT_EP, ADB_BULK_IN_EP, 512),
};
#endif

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;

#if TUD_OPT_HIGH_SPEED
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
#else
    return desc_fs_configuration;
#endif
}
uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }

        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        // Convert ASCII to UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return desc_str;
}
