/*
 * main.h
 *
 *  Created on: Dec. 12, 2022
 *
 */

#ifndef SRC_USB_SUBSYS_DEMO_USB_CDC_VCOM_H_
#define SRC_USB_SUBSYS_DEMO_USB_CDC_VCOM_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "arcs_ap.h"
#include "log_print.h"
#include "dbg_assert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
//#include "queue.h"

#include "tusb.h"
//#include "usb_devices.h"

#ifndef GPIO_BIT
#define STRCAT(a, b)   a##b             // concat without expand
#define XSTRCAT(a, b)  STRCAT(a, b)     // expand then concat
#define GPIO_BIT(N)    XSTRCAT(CSK_GPIO_PIN, N) // !!GPIO use pin bit mask, IOMuxManager use pin num!!
#endif // GPIO_BIT

//GPIO pin definitions, for HID test etc.
#define BTN1_PIN         4   // A04, FUNC=0, IN
#define BTN2_PIN         6   // A06, FUNC=0, IN
#define BTN3_PIN         17  // A17, FUNC=0, IN
#define BTN4_PIN         18  // A18, FUNC=0, IN
#define BTN_NUL_PIN      31  // FOR TEST ONLY!!

//----------------------------------
//USB global configuration parameters
#define USB_VID         0x2023
#define USB_PID         0x7777

#define CDC0_HAS_NOTIF_EP   1 // 1
#define CDC1_HAS_NOTIF_EP   1 // 1

//NOTE: CSK6 EP NO: IN 1~5, OUT 1~5
#if CDC0_HAS_NOTIF_EP
#define EPNUM_CDC_0_NOTIF       0x01 //0x81
#endif

#define EPNUM_CDC_0_DATA        0x02

#if CDC1_HAS_NOTIF_EP
#define EPNUM_CDC_1_NOTIF       0x04 //0x84
#endif

#define EPNUM_CDC_1_DATA        0x05

#if CDC0_HAS_NOTIF_EP
#define EPADDR_CDC_0_NOTIF      (0x80 | EPNUM_CDC_0_NOTIF)
#endif

#define EPADDR_CDC_0_DATA_OUT   EPNUM_CDC_0_DATA
#define EPADDR_CDC_0_DATA_IN    (0x80 | EPNUM_CDC_0_DATA)

#if CDC1_HAS_NOTIF_EP
#define EPADDR_CDC_1_NOTIF      (0x80 | EPNUM_CDC_1_NOTIF)
#endif

#define EPADDR_CDC_1_DATA_OUT   EPNUM_CDC_1_DATA
#define EPADDR_CDC_1_DATA_IN    (0x80 | EPNUM_CDC_1_DATA)

// There's no detection mechanism to check whether USB device is
// connected to or disconnected from USB host in the USB SPEC.
// One solution is to check level change on USB VBUS -
// level 0 => level 1, indicate "connected"
// level 1 => level 0, indicate "disconnected"
#define CHK_VBUS_HOT_PLUG   0
#if CHK_VBUS_HOT_PLUG
#define GPIO_GRP_TO_VBUS    0   // 0 = PAD A, 1 = PAD B, ...
#define GPIO_PIN_TO_VBUS    17  //TODO: change it? BSD 2023.3.8.
#endif // CHK_VBUS_HOT_PLUG

#endif /* SRC_USB_SUBSYS_DEMO_USB_CDC_VCOM_H_ */
