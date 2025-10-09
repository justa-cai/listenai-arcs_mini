#ifndef __DRIVER_BLENDER_INTERNAL_H
#define __DRIVER_BLENDER_INTERNAL_H

#include "dma.h"
#include "arcs_ap.h"
#include "ClockManager.h"
#include "assert.h"

#include "image_d2blender_reg.h"
#include "Driver_Blender.h"


#define BLENDER_BACK_BURST_LEN_MAX   8
#define BLENDER_FORE_BURST_LEN_MAX   8
#define BLENDER_MASK_BURST_LEN_MAX   4
#define BLENDER_OUT_BURST_LEN_MAX    8

/**
  * @brief  Blender handle Structure definition
  */
typedef struct __Blender_DEV
{
    IMAGE_D2BLENDER_RegDef            *Instance;           /*!< Blender Register base address  */
    Blender_InitTypeDef               Init;                /*!< Blender parameters             */
}Blender_DEV;


#endif /* __DRIVER_BLENDER_INTERNAL_H */


