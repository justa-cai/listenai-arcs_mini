/**
  ******************************************************************************
  * @file    Driver_TRAP.h
  * @author  ListenAI Application Team
  * @brief   Header file of TRAP HAL module.
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
#ifndef __CSK_DRIVER_TRAP_H
#define __CSK_DRIVER_TRAP_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"


/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup TRAP
  * @{
  */


/* Exported types ------------------------------------------------------------*/
/** @defgroup TRAP_Exported_Types TRAP Exported Types
  * @{
  */

/**
  * @brief  add description
  */
typedef void (*CSK_TRAP_SignalEvent_t) (void* workspace);

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/

/** @defgroup TRAP_Exported_Constants TRAP Exported Constants
  * @{
  */
#define CSK_TRAP_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)
#define CSK_TRAP_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

/**
  * @}
  */

/* Exported macro ------------------------------------------------------------*/
/** @defgroup TRAP_Exported_Macro TRAP Exported Macro
  * @{
  */

/**
  * @}
  */


/* Exported functions --------------------------------------------------------*/
/** @addtogroup TRAP_Exported_Functions TRAP Exported Functions
  * @{
  */

/** @addtogroup TRAP_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
void* TRAP(void);
int HAL_TRAP_Initialize(void *res);
int HAL_TRAP_Uninitialize(void *res);

/**
  * @}
  */


/** @addtogroup TRAP_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */
CSK_DRIVER_VERSION HAL_TRAP_GetVersion(void);
int HAL_TRAP_Channel_Enable(void* res, uint32_t channels);
int HAL_TRAP_Channel_Disable(void* res, uint32_t channels);
int HAL_TRAP_SwapFunction(void* res, void* orifun, void* newfun);
int HAL_TRAP_DataInstead(void* res, void* oriaddr, void* newaddr);

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

#endif /* __CSK_DRIVER_TRAP_H */
