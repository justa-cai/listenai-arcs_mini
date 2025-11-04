/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "cdc_device.h"
#include "main.h"

#define USB20_BASED    (TUD_OPT_HIGH_SPEED ? 1 : 0) // 1: USB2.0; 0: USB1.1

#if (USB20_BASED == 0)
#define BCD_USB_VAL 0x0110
#elif (USB20_BASED == 1)
#define BCD_USB_VAL 0x0200
#endif

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = BCD_USB_VAL,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,      // 0
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON, // 0
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,    // 0
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

#if USB20_BASED

tusb_desc_device_qualifier_t const desc_dev_qua = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = BCD_USB_VAL,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .bNumConfigurations = 0x01,
    .bReserved = 0
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
    return (uint8_t const *) &desc_dev_qua;
}

#endif // USB20_BASED

uint8_t const desc_hid_report[] =
{
    // only 1 report descriptor, NO Report ID!!
    HID_USAGE_PAGE ( HID_USAGE_PAGE_CONSUMER    )              ,
    HID_USAGE      ( HID_USAGE_CONSUMER_CONTROL )              ,
    HID_COLLECTION ( HID_COLLECTION_APPLICATION )              ,
    // Add Report ID here if any...
    HID_LOGICAL_MIN  ( 0 ) ,
    HID_LOGICAL_MAX  ( 1 ) ,
    HID_USAGE    ( HID_USAGE_CONSUMER_VOLUME_DECREMENT ) ,
    HID_USAGE    ( HID_USAGE_CONSUMER_VOLUME_INCREMENT ) ,
    HID_REPORT_SIZE  ( 1 ) ,
    HID_REPORT_COUNT ( 2 ) ,
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,
    HID_REPORT_COUNT ( HID_REPORT_LEN * 8 - 2 ) , // HID_REPORT_COUNT ( 6 )
    HID_INPUT        ( HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE ) ,
    HID_COLLECTION_END
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void) itf;
    return desc_hid_report;
}


//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum
{
    ITF_NUM_CDC_0 = 0,
    ITF_NUM_CDC_0_DATA,
#if (CFG_TUD_CDC >= 2)
    ITF_NUM_CDC_1,
    ITF_NUM_CDC_1_DATA,
#endif
#ifdef EPADDR_HID_IN
    ITF_NUM_HID,
#endif
    ITF_NUM_TOTAL
};


//#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_NOTIF_LEN)
//#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN)
#if (CFG_TUD_CDC >= 2)
#if (CDC0_HAS_NOTIF_EP && CDC1_HAS_NOTIF_EP) // 2 devices, both have NOTIF_EP
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_NOTIF_LEN)
#elif (CDC0_HAS_NOTIF_EP || CDC1_HAS_NOTIF_EP) // 2 devices, one has NOTIF_EP,
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_NOTIF_LEN + TUD_CDC_DESC_LEN)
#else // 2 devices, none has NOTIF_EF
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN)
#endif
#elif CDC0_HAS_NOTIF_EP // 1 device, has NOTIF_EP
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_NOTIF_LEN)
#else // 1 device, no NOTIF_EP
#ifdef EPADDR_HID_IN
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)
#else
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#endif
#endif


uint8_t const desc_fs_configuration[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#if CDC0_HAS_NOTIF_EP
    TUD_CDC_DESCRIPTOR_NOTIF(ITF_NUM_CDC_0, 4, EPADDR_CDC_0_NOTIF, 8, EPADDR_CDC_0_DATA_OUT, EPADDR_CDC_0_DATA_IN, CFG_TUD_CDC_EP_BUFSIZE),
#else
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPADDR_CDC_0_DATA_OUT, EPADDR_CDC_0_DATA_IN, CFG_TUD_CDC_EP_BUFSIZE),
#endif

#if (CFG_TUD_CDC >= 2)
    // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#if CDC1_HAS_NOTIF_EP
    TUD_CDC_DESCRIPTOR_NOTIF(ITF_NUM_CDC_1, 4, EPADDR_CDC_1_NOTIF, 8, EPADDR_CDC_1_DATA_OUT, EPADDR_CDC_1_DATA_IN, CFG_TUD_CDC_EP_BUFSIZE),
#else
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPADDR_CDC_1_DATA_OUT, EPADDR_CDC_1_DATA_IN, CFG_TUD_CDC_EP_BUFSIZE),
#endif
#endif

#ifdef EPADDR_HID_IN
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 4, HID_PROTOCOL_NONE, sizeof(desc_hid_report), EPADDR_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 50)
#endif
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations
    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors (UTF-16 string)
// [0] for bLength and bDescriptorType
RAM_DATA uint16_t usLangID[] = { u'\t', 0x0409, u'\0'};
RAM_DATA uint16_t usManufacturer[] = u"\tListenAI";
RAM_DATA uint16_t usProduct[] = u"\tCSK6_CDC";
RAM_DATA uint16_t usSerialNumber[] = u"\t88AB88CD";
RAM_DATA uint16_t usItfVCom[] = u"\tVirtCOM";
RAM_DATA uint16_t usItfUHID[] = u"\tUHID";

static uint16_t *ustr_desc_arr [] = {
        usLangID, // LangID, English (0x0409)
        usManufacturer,  // Manufacturer
        usProduct,   // Product
        usSerialNumber,  // SerialNumber
        usItfVCom, // CDC Interface
#ifdef EPADDR_HID_IN
        usItfUHID, // UHID Interface
#endif
};


//unicode string, terminated with '\u0'
static uint32_t ustrlen(uint16_t *pu)
{
    uint32_t count = 0;
    uint16_t *cur = pu;

    if(cur == NULL) return 0;
    //while(*cur != 0x0)  cur++;
    //return ((uint32_t)cur - (uint32_t)pu);

    while(*cur++ != 0x0)  count++;
    return count;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint16_t *str;
  uint8_t chr_count;

  if ( index >= sizeof(ustr_desc_arr)/sizeof(ustr_desc_arr[0]) )
      return NULL;

  str = ustr_desc_arr[index];
  if (index == 0)
  {
    chr_count = 1;
  } else {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    // Cap at max char
    chr_count = ustrlen(str + 1);  // bypass the first uint16_t
    if ( chr_count > 31 ) chr_count = 31;
  }

  // first byte is length (including header), second byte is string type
  if (str[0] == u'\t')
      str[0] = (TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2);

  return str;
}
