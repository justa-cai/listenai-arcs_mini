#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

// Board Specific Configuration
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      1
#endif

#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_HIGH_SPEED
#endif

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUSB_MCU          OPT_MCU_ARCS
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG        0

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#error CFG_TUSB_OS must be defined
#endif

#ifndef CFG_TUSB_DEBUG
#error CFG_TUSB_DEBUG must be defined
#endif

#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__ ((aligned(4)))
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

// Class configuration
#define CFG_TUD_CDC              0
#define CFG_TUD_MSC              1
#define CFG_TUD_HID              0
#define CFG_TUD_MIDI             0
#define CFG_TUD_VENDOR           0

#define CFG_TUD_MSC_EP_BUFSIZE   512

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
