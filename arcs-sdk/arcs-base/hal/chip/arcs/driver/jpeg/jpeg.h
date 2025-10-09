#ifndef __DRIVER_JPEG_INTERNAL_H
#define __DRIVER_JPEG_INTERNAL_H


#include "arcs_ap.h"
#include "ClockManager.h"
#include "assert.h"

#include "jpeg_reg.h"
#include "Driver_JPEG.h"


/**
  * @brief  jpeg handle Structure definition
  */
typedef struct __Jpeg_DEV
{
    JPEG_RegDef            *Instance;           /*!< jpeg Register base address  */
    Jpeg_InitTypeDef       Init;                /*!< jpeg parameters             */
    Jpeg_SignalEvent_t     cb_event;            /*!< jpegDVP callback               */
}Jpeg_DEV;


#endif /* __DRIVER_JPEG_INTERNAL_H */


