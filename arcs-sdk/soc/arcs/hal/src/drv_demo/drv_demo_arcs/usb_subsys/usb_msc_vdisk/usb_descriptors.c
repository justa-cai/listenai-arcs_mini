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

//#include "tusb.h"
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
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
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

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
  ITF_NUM_MSC,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPADDR_MSC_OUT, EPADDR_MSC_IN, 64),
};

#if TUD_OPT_HIGH_SPEED
uint8_t const desc_hs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPADDR_MSC_OUT, EPADDR_MSC_IN, 512),
};
#endif

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors (UTF-16 string)
// [0] for bLength and bDescriptorType
RAM_DATA uint16_t usLangID[] = { u'\t', 0x0409, u'\0'};
RAM_DATA uint16_t usManufacturer[] = u"\tListenAI";
RAM_DATA uint16_t usProduct[] = u"\tCSK6_VDISK";
RAM_DATA uint16_t usSerialNumber[] = u"\t99887766";

static uint16_t *ustr_desc_arr [] = {
        usLangID, // LangID, English (0x0409)
        usManufacturer,  // Manufacturer
        usProduct,   // Product
        usSerialNumber,  // SerialNumber
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
