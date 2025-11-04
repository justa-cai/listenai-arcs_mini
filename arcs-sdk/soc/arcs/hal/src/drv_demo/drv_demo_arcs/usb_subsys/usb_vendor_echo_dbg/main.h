/*
 * main.h
 *
 *  Created on: Oct. 4, 2023
 *
 */

#ifndef SRC_USB_SUBSYS_DEMO_USB_VENDOR_ECHO2_H_
#define SRC_USB_SUBSYS_DEMO_USB_VENDOR_ECHO2_H_

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

//----------------------------------
//USB global configuration parameters
#define USB_ECHO_BUF_SIZE	256
#define USB_VID         0x1234
#define USB_PID         0x5678
//#define USB_VID         0x2046
//#define USB_PID         0x9527

/*
#define EPNUM_USB20_TOTAL	4
#define EPNUM_VEND_STRIDX	2
#define EPNUM_VEND_ADDR_STEP	1
#define EPNUM_VEND_ADDRBASE_IN	0x80
#define EPNUM_VEND_ADDRBASE_OUT	0x0

#define EPNUM_VEN0_IN   0x01
#define EPNUM_VEN0_OUT  0x01

#define EPNUM_VEN0_IN_EXT	(EPNUM_VEN0_IN + EPNUM_VEND_ADDR_STEP)
#define EPNUM_VEN0_OUT_EXT	(EPNUM_VEN0_OUT + EPNUM_VEND_ADDR_STEP)
#define EPNUM_VEN0_LAST_IN	EPNUM_VEN0_IN_EXT
#define EPNUM_VEN0_LAST_OUT	EPNUM_VEN0_OUT_EXT

#define EPNUM_VEN1_IN		(EPNUM_VEN0_LAST_IN + EPNUM_VEND_ADDR_STEP)
#define EPNUM_VEN1_OUT		(EPNUM_VEN0_LAST_OUT + EPNUM_VEND_ADDR_STEP)
#define EPNUM_VEN1_LAST_IN	EPNUM_VEN1_IN
#define EPNUM_VEN1_LAST_OUT	EPNUM_VEN1_OUT

#define EPNUM_VEN2_IN		(EPNUM_VEN1_LAST_IN + EPNUM_VEND_ADDR_STEP)
#define EPNUM_VEN2_OUT		(EPNUM_VEN1_LAST_OUT + EPNUM_VEND_ADDR_STEP)

#define EPADDR_VEN0_IN		(EPNUM_VEND_ADDRBASE_IN | EPNUM_VEN0_IN)
#define EPADDR_VEN0_OUT		(EPNUM_VEND_ADDRBASE_OUT | EPNUM_VEN0_OUT)
#define EPADDR_VEN0_1_IN	(EPNUM_VEND_ADDRBASE_IN | EPNUM_VEN0_1_IN)
#define EPADDR_VEN0_1_OUT	(EPNUM_VEND_ADDRBASE_OUT | EPNUM_VEN0_1_OUT)
#define EPADDR_VEN0_IN_EXT	(EPNUM_VEND_ADDRBASE_IN | EPNUM_VEN0_IN_EXT)
#define EPADDR_VEN0_OUT_EXT	(EPNUM_VEND_ADDRBASE_OUT | EPNUM_VEN0_OUT_EXT)
#define EPADDR_VEN1_IN		(EPNUM_VEND_ADDRBASE_IN | EPNUM_VEN1_IN)
#define EPADDR_VEN1_OUT		(EPNUM_VEND_ADDRBASE_OUT | EPNUM_VEN1_OUT)
#define EPADDR_VEN2_IN		(EPNUM_VEND_ADDRBASE_IN | EPNUM_VEN2_IN)
#define EPADDR_VEN2_OUT		(EPNUM_VEND_ADDRBASE_OUT | EPNUM_VEN2_OUT)
*/


//Vendor 0 interface
#define VEN0_EP_CNT         4
#define VEN0_STR_IDX        2 // interface string index
#define EPNUM_VEN0_IN       0x01
#define EPNUM_VEN0_OUT      0x01
#define EPNUM_VEN0_IN_EXT   0x2
#define EPNUM_VEN0_OUT_EXT  0x2

//Vendor 1 interface (for DBG EP)
#define VEN1_EP_CNT         2
#define VEN1_STR_IDX        4 // 0 // NO interface string
#define EPNUM_VEN1_IN       0x8 // 0x3
#define EPNUM_VEN1_OUT      0x8 // 0x3

//Vendor 2 interface
#define VEN2_EP_CNT         1 // 2
#define VEN2_STR_IDX        0 // NO interface string
//#define EPNUM_VEN2_IN       0x4
#define EPNUM_VEN2_OUT      0x4

#define EPADDR_VEN0_IN      (0x80 | EPNUM_VEN0_IN)
#define EPADDR_VEN0_OUT     (EPNUM_VEN0_OUT)
#define EPADDR_VEN0_IN_EXT  (0x80 | EPNUM_VEN0_IN_EXT)
#define EPADDR_VEN0_OUT_EXT (EPNUM_VEN0_OUT_EXT)
#define EPADDR_VEN1_IN      (0x80 | EPNUM_VEN1_IN)
#define EPADDR_VEN1_OUT     (EPNUM_VEN1_OUT)
//#define EPADDR_VEN2_IN      (0x80 | EPNUM_VEN2_IN)
#define EPADDR_VEN2_OUT     (EPNUM_VEN2_OUT)


//#define EP_MPS_USB11_BULK	64
//#define EP_MPS_USB20_BULK	512

#define EP_MPS_VEN0_IN          (TUD_OPT_HIGH_SPEED ? 512 : 64) //96 //
#define EP_MPS_VEN0_OUT         (TUD_OPT_HIGH_SPEED ? 512 : 64) //96 //
#define EP_MPS_VEN0_IN_EXT      64 // 4
#define EP_MPS_VEN0_OUT_EXT     64 // 4

#define EP_MPS_VEN1_IN      64 //16
#define EP_MPS_VEN1_OUT     64 //16

#define EP_MPS_VEN2_IN      8
#define EP_MPS_VEN2_OUT     8

#endif /* SRC_USB_SUBSYS_DEMO_USB_VENDOR_ECHO2_H_ */
