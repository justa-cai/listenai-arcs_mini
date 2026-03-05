#include "tusb.h"
#include "log_print.h"
#include <string.h>

enum
{
  ITF_NUM_ADB = 0,
#if CONFIG_APP_USB_CDC_ENABLE
  ITF_NUM_CDC,
  ITF_NUM_CDC_DATA,
#endif
#if CONFIG_APP_USB_AUDIO_ENABLE
  ITF_NUM_AUDIO_CONTROL,     // USB Audio
  ITF_NUM_AUDIO_STREAMING,   // USB Audio
#endif
  ITF_NUM_TOTAL
};

static tusb_desc_device_t const desc_device_adb = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x0483,
    .idProduct = 0x0adb, // Android Debug Bridge (ADB) device
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static tusb_desc_device_t const desc_device_msc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MSC,
    .bDeviceSubClass = TUSB_CLASS_VENDOR_SPECIFIC,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x0483,
    .idProduct = 0x4001,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

const char *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "ListenAi",                 // 1: Manufacturer
    "VoiceAssistant",           // 2: Product
    NULL,                       // 3: Serial (will be obtained dynamically)
    "ADB Interface",            // 4: Interface
    "Audio Interface",          // 5: Interface
    "CDC Audio Stream",         // 6: CDC Interface
};

#define EPNUM_MSC_OUT 0x01
#define EPNUM_MSC_IN  0x81

#define ADB_BULK_OUT_EP 0x02
#define ADB_BULK_IN_EP  0x82

#define EPNUM_CDC_NOTIF 0x83
#define EPNUM_CDC_OUT   0x04
#define EPNUM_CDC_IN    0x84

#define EPNUM_AUDIO     0x85
#if (CFG_TUD_MAX_SPEED == OPT_MODE_HIGH_SPEED)
#define EP_BULK_MAX_SIZE 512
#else
#define EP_BULK_MAX_SIZE 64
#endif

#define ADB_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize)                                                       \
    /* Interface */                                                                                                    \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x42, 0x01, _stridx, /* Endpoint Out */         \
        7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,           /* Endpoint In */          \
        7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

// #define CONFIG_TOTAL_LEN_ADB (TUD_CONFIG_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_MIC_FOUR_CH_DESC_LEN)
// ADB descriptor length: 9 (interface) + 7 (ep out) + 7 (ep in) = 23
#define ADB_DESC_LEN 23

#if CONFIG_APP_USB_CDC_ENABLE
#define CDC_DESC_LEN TUD_CDC_DESC_LEN
#else
#define CDC_DESC_LEN 0
#endif

#if CONFIG_APP_USB_AUDIO_ENABLE
#define CONFIG_TOTAL_LEN_ADB (TUD_CONFIG_DESC_LEN + ADB_DESC_LEN + CDC_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_MIC_FOUR_CH_DESC_LEN)
static uint8_t const desc_cfg_adb[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN_ADB, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    ADB_DESCRIPTOR(ITF_NUM_ADB, 4, ADB_BULK_OUT_EP, ADB_BULK_IN_EP, EP_BULK_MAX_SIZE),
#if CONFIG_APP_USB_CDC_ENABLE
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, EP_BULK_MAX_SIZE),
#endif
    TUD_AUDIO_MIC_FOUR_CH_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 5,
                                     CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
                                     CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX * 8,
                                     EPNUM_AUDIO, CFG_TUD_AUDIO_EP_SZ_IN),
};
#else
// USB Audio disabled - removed TUD_AUDIO_MIC_FOUR_CH_DESC_LEN from total length
#define CONFIG_TOTAL_LEN_ADB (TUD_CONFIG_DESC_LEN + ADB_DESC_LEN + CDC_DESC_LEN)
static uint8_t const desc_cfg_adb[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN_ADB, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    ADB_DESCRIPTOR(ITF_NUM_ADB, 4, ADB_BULK_OUT_EP, ADB_BULK_IN_EP, EP_BULK_MAX_SIZE),
#if CONFIG_APP_USB_CDC_ENABLE
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, EP_BULK_MAX_SIZE),
#endif
    // USB Audio descriptor removed to prevent continuous isochronous packets
    // TUD_AUDIO_MIC_FOUR_CH_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 5,
    //                                  CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
    //                                  CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX * 8,
    //                                  EPNUM_AUDIO, CFG_TUD_AUDIO_EP_SZ_IN),
};
#endif


#define CONFIG_TOTAL_LEN_MSC (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)
static uint8_t const desc_cfg_msc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN_MSC, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(0, 0, EPNUM_MSC_OUT, EPNUM_MSC_IN, EP_BULK_MAX_SIZE),
};

extern bool app_usb_msc_enabled();

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    if (app_usb_msc_enabled()) {
        return desc_cfg_msc;
    }

    return desc_cfg_adb;
}

uint8_t const *tud_descriptor_device_cb(void)
{
    if (app_usb_msc_enabled()) {
        return (uint8_t const *)&desc_device_msc;
    }

    return (uint8_t const *)&desc_device_adb;
}

// Function to get the chip's unique ID
static size_t get_chip_id(uint8_t *id_buffer, size_t buffer_size)
{
    // TODO: Read chip ID from efuse.
    // For now, we'll use a placeholder static ID because there is no chip id in efuse.
    const uint8_t chip_id[] = {0xFF, 0xBB, 0xCC, 0xDD, 0xEE, 0x00, 0x11, 0x22};
    size_t id_len = sizeof(chip_id);

    // Make sure we don't overflow the buffer
    if (id_len > buffer_size) {
        id_len = buffer_size;
    }

    // Copy the ID to the provided buffer
    memcpy(id_buffer, chip_id, id_len);

    return id_len;
}

// Function to get chip ID and convert it to a USB string descriptor
static uint16_t *get_serial_string_desc(uint16_t desc_str[], size_t max_len)
{
    uint8_t unique_id[16] = {0};
    size_t id_len = get_chip_id(unique_id, sizeof(unique_id));

    // Convert the binary ID to a hex string in UTF-16 format for USB
    if (id_len > max_len / 4) {
        id_len = max_len / 4; // Ensure we don't overflow
    }

    // Hex digits for conversion
    const char hex_digits[] = "0123456789ABCDEF";

    // Fill in the descriptor data

    for (size_t i = 0; i < id_len; i++) {
        desc_str[1 + i * 2] = hex_digits[unique_id[i] >> 4];      // High nibble
        desc_str[1 + i * 2 + 1] = hex_digits[unique_id[i] & 0xF]; // Low nibble
    }

    // First 16-bit word contains string descriptor info
    // 0x0300 means string descriptor with length of 3 bytes
    // Length = num of bytes = (id_len * 4) + 2 (for descriptor header)
    desc_str[0] = (TUSB_DESC_STRING << 8) | (id_len * 4 + 2);

    return desc_str;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t desc_str[32];
    uint8_t chr_count;

    // Handle special case for language descriptor (index 0)
    if (index == 0) {
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    }
    // Handle special case for serial number (index 3)
    else if (index == 3) {
        // Serial Number is index 3 - get it dynamically from chip ID
        return get_serial_string_desc(desc_str, 32);
    }
    // Handle all other descriptors with proper bounds checking
    else if (index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
        // Get the string from the array
        const char *str = string_desc_arr[index];

        // If the string is NULL, return NULL
        if (str == NULL) {
            return NULL;
        }

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
