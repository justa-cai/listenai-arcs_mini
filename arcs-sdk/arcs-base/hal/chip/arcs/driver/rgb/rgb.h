#ifndef __DRIVER_RGB_INTERNAL_H
#define __DRIVER_RGB_INTERNAL_H

#include "arcs_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "Driver_RGB.h"


#define RGB_CLK_OUT_SEL_100MHz  100000000
#define RGB_CLK_OUT_SEL_24MHz   24000000
#define RGB_CLK_OUT_DIV_MAX     15
#define RGB_CLK_OUT_DIV_MIN     1
#define RGB_CLK_OUT_HZ_MAX      (RGB_CLK_OUT_SEL_100MHz / RGB_CLK_OUT_DIV_MIN)
#define RGB_CLK_OUT_HZ_MIN      (RGB_CLK_OUT_SEL_24MHz / RGB_CLK_OUT_DIV_MAX)
#define RGB_BUF                 (RGB_BASE + 0xF4)

/**
  * @brief  RGB handle Structure definition
  */
typedef struct __RGB_DEV
{
    RGB_INTERFACE_RegDef          *Instance;           /*!< RGB Register base address  */

    RGB_InitTypeDef               Init;                /*!< RGB parameters             */

    CSK_RGB_SignalEvent_t         cb_event;            /*!< RGB callback               */

    volatile RGB_emState          State;               /*!< RGB state                  */

    volatile uint32_t             ErrorCode;           /*!< RGB Error code             */
}RGB_DEV;


#endif /* __DRIVER_RGB_INTERNAL_H */


