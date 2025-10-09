/*
 * main.h
 *
 *  Created on: Mar. 13, 2013
 *
 */

#ifndef SRC_USB_SUBSYS_DEMO_USB_MSC_RAM_MAIN_H_
#define SRC_USB_SUBSYS_DEMO_USB_MSC_RAM_MAIN_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "arcs_ap.h"
#include "log_print.h"

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

//----------------------------------
//USB global configuration parameters
#define USB_VID         0x2025
#define USB_PID         0x7711

#define EPNUM_MSC_OUT   0x02
#define EPNUM_MSC_IN    0x01

#define EPADDR_MSC_OUT   EPNUM_MSC_OUT
#define EPADDR_MSC_IN    (0x80 | EPNUM_MSC_IN)

#endif /* SRC_USB_SUBSYS_DEMO_USB_MSC_RAM_MAIN_H_ */
