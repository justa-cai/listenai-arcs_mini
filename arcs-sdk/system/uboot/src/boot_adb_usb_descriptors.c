#include "tusb.h"

#include <string.h>

extern const char *device_id_str_get(void);

static tusb_desc_device_t const boot_adb_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x12D1,
    .idProduct = 0x107E,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

#define BOOT_ADB_INTERFACE_INDEX 0
#define BOOT_ADB_BULK_OUT_EP     0x02
#define BOOT_ADB_BULK_IN_EP      0x83

#define BOOT_ADB_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize)                                                 \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x42, 0x01, _stridx,                           \
        7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,                                     \
        7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

#define BOOT_ADB_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

static uint8_t const boot_adb_desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, BOOT_ADB_CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    BOOT_ADB_DESCRIPTOR(BOOT_ADB_INTERFACE_INDEX, 4, BOOT_ADB_BULK_OUT_EP,
                        BOOT_ADB_BULK_IN_EP, 64),
};

#if TUD_OPT_HIGH_SPEED
static uint8_t const boot_adb_desc_hs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, BOOT_ADB_CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    BOOT_ADB_DESCRIPTOR(BOOT_ADB_INTERFACE_INDEX, 4, BOOT_ADB_BULK_OUT_EP,
                        BOOT_ADB_BULK_IN_EP, 512),
};
#endif

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;

#if TUD_OPT_HIGH_SPEED
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? boot_adb_desc_hs_configuration :
                                                  boot_adb_desc_fs_configuration;
#else
    return boot_adb_desc_fs_configuration;
#endif
}

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&boot_adb_desc_device;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t desc_str[32];
    const char *str = NULL;
    uint8_t chr_count;

    (void)langid;

    if (index == 0) {
        static const uint8_t lang_desc[] = {0x09, 0x04};

        memcpy(&desc_str[1], lang_desc, sizeof(lang_desc));
        chr_count = 1;
    } else {
        switch (index) {
        case 1:
            str = "ListenAi";
            break;
        case 2:
            str = "ARCS Boot Recovery";
            break;
        case 3:
            str = device_id_str_get();
            break;
        case 4:
            str = "ADB Interface";
            break;
        default:
            return NULL;
        }

        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        for (uint8_t i = 0; i < chr_count; ++i) {
            desc_str[1 + i] = str[i];
        }
    }

    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return desc_str;
}
