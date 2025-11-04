/**
  ******************************************************************************
  * @file    Driver_TRNG.h
  * @author  ListenAI Application Team
  * @brief   Header file of TRNG HAL module.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CSK_DRIVER_TRNG_H
#define __CSK_DRIVER_TRNG_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"


/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup TRNG
  * @{
  */


/* Exported types ------------------------------------------------------------*/
/** @defgroup TRNG_Exported_Types TRNG Exported Types
  * @{
  */

/**
  * @brief  add description
  */
typedef void (*CSK_TRNG_SignalEvent_t) (void* workspace);

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/

/** @defgroup TRNG_Exported_Constants TRNG Exported Constants
  * @{
  */
#define CSK_TRNG_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)
#define CSK_TRNG_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

#ifndef NULL
#define NULL                        (void *)0
#endif

/*---------------------Control mode for application---------------------------------*/
/*----- TRNG cold warm up time in apb clock : 0/1/2/3 represents 2^22/2^23/2^24/2^25 -----*/
#define CSK_TRNG_COLDTIME_CONTROL_Pos             0
#define CSK_TRNG_COLDTIME_CONTROL_Msk             (0x03UL << CSK_TRNG_COLDTIME_CONTROL_Pos)
#define CSK_TRNG_COLDTIME_2_22         			  (0x00UL << CSK_TRNG_COLDTIME_CONTROL_Pos)
#define CSK_TRNG_COLDTIME_2_23         			  (0x01UL << CSK_TRNG_COLDTIME_CONTROL_Pos)
#define CSK_TRNG_COLDTIME_2_24         			  (0x02UL << CSK_TRNG_COLDTIME_CONTROL_Pos)
#define CSK_TRNG_COLDTIME_2_25         			  (0x03UL << CSK_TRNG_COLDTIME_CONTROL_Pos)

/*----- TRNG hot warm up time in apb clock : 0/1/2/3 represents 2^16/2^17/2^18/2^19 -----*/
#define CSK_TRNG_HOTTIME_CONTROL_Pos              2
#define CSK_TRNG_HOTTIME_CONTROL_Msk              (0x03UL << CSK_TRNG_HOTTIME_CONTROL_Pos)
#define CSK_TRNG_HOTTIME_2_16         			  (0x00UL << CSK_TRNG_HOTTIME_CONTROL_Pos)
#define CSK_TRNG_HOTTIME_2_17         			  (0x01UL << CSK_TRNG_HOTTIME_CONTROL_Pos)
#define CSK_TRNG_HOTTIME_2_18         			  (0x02UL << CSK_TRNG_HOTTIME_CONTROL_Pos)
#define CSK_TRNG_HOTTIME_2_19         			  (0x03UL << CSK_TRNG_HOTTIME_CONTROL_Pos)

/*----- TRNG delay time in apb clock : 0/1/2/3/4/5/6/7 represents 2^9/2^10/2^11/2^12/2^13/2^14/2^15/2^16 -----*/
#define CSK_TRNG_DELAYTIME_CONTROL_Pos              4
#define CSK_TRNG_DELAYTIME_CONTROL_Msk              (0x07UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_09         			(0x00UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_10         			(0x01UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_11         			(0x02UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_12         			(0x03UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_13         			(0x04UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_14         			(0x05UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_15         			(0x06UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)
#define CSK_TRNG_DELAYTIME_2_16         			(0x07UL << CSK_TRNG_DELAYTIME_CONTROL_Pos)


/*----- TRNG interrupt mode : -----*/

/**
  * @}
  */

/* Exported macro ------------------------------------------------------------*/
/** @defgroup TRNG_Exported_Macro TRNG Exported Macro
  * @{
  */

/**
  * @}
  */


/* Exported functions --------------------------------------------------------*/
/** @addtogroup TRNG_Exported_Functions TRNG Exported Functions
  * @{
  */

/** @addtogroup TRNG_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
void* TRNG(void);
int32_t HAL_TRNG_Initialize(void *res);
int32_t HAL_TRNG_Uninitialize(void *res);
int32_t HAL_TRNG_PowerControl(void *res, CSK_POWER_STATE state);
int32_t HAL_TRNG_Control(void* res, uint32_t control);
/**
  * @}
  */


/** @addtogroup TRNG_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */
CSK_DRIVER_VERSION HAL_TRNG_GetVersion(void);
int32_t HAL_TRNG_InterruptEnable(void* res);
int32_t HAL_TRNG_InterruptDisable(void* res);
int32_t HAL_TRNG_Enable(void* res);
int32_t HAL_TRNG_Disable(void* res);
int32_t HAL_TRNG_GetData(void* res);
int32_t HAL_TRNG_GetDataReady(void* res);
int32_t HAL_TRNG_RegisterCallback(void *res, CSK_TRNG_SignalEvent_t cb_event);

/**
  * @}
  */


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __CSK_DRIVER_TRNG_H */
