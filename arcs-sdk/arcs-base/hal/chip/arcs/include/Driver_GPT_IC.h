/**
  ******************************************************************************
  * @file    Driver_IC.h
  * @author  ListenAI Application Team
  * @brief   Header file of IC HAL module.
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
#ifndef __DRIVER_GPT_IC_H
#define __DRIVER_GPT_IC_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"
#include "Driver_GPT_Common.h"


/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup IC
  * @{
  */


/* Exported types ------------------------------------------------------------*/
/** @defgroup IC_Exported_Types IC Exported Types
  * @{
  */

/**
  * @brief  add description
  */

 /**
  * @}
  */


/* Exported constants --------------------------------------------------------*/

/** @defgroup IC_Exported_Constants IC Exported Constants
  * @{
  */

/*---------------------Control mode for application---------------------------------*/
/****** GPT Input Capture Control Codes *****/
/*----- GPT Input Capture Control Codes: Mode -----*/
#define CSK_GPT_IC_CONTROL_Pos             0
#define CSK_GPT_IC_CONTROL_Msk             (0xFFUL << CSK_GPT_IC_CONTROL_Pos)
#define CSK_GPT_IC_TIME_MODE               (0x00UL << CSK_GPT_IC_CONTROL_Pos)
#define CSK_GPT_IC_COUNT_MODE              (0x01UL << CSK_GPT_IC_CONTROL_Pos)


/*----- GPT Input Capture Clock Control Codes: Configure Parameters: Clock Source -----*/
#define CSK_GPT_IC_CLKSRC_Pos             9
#define CSK_GPT_IC_CLKSRC_Msk             (3UL << CSK_GPT_IC_CLKSRC_Pos)
#define CSK_GPT_IC_CLKSRC_PCLK            (2UL << CSK_GPT_IC_CLKSRC_Pos)       ///< peripheral clock (default)
#define CSK_GPT_IC_CLKSRC_CLKT1           (1UL << CSK_GPT_IC_CLKSRC_Pos)       ///< clock1 clock (default)
#define CSK_GPT_IC_CLKSRC_CLKT0           (0UL << CSK_GPT_IC_CLKSRC_Pos)       ///< clock0 clock



/*----- GPT Input Capture Control Codes: Configure Parameters: Trigger Source -----*/
#define CSK_GPT_IC_TRIGGER_Pos             	11
#define CSK_GPT_IC_TRIGGER_Msk             	(1UL << CSK_GPT_IC_TRIGGER_Pos)
#define CSK_GPT_IC_TRIGGER_EXT         	 	(0UL << CSK_GPT_IC_TRIGGER_Pos)
#define CSK_GPT_IC_TRIGGER_SOFTWARE			(1UL << CSK_GPT_IC_TRIGGER_Pos)

/*----- GPT Input Capture Control Codes: Configure Parameters: Edge Selection -----*/
#define CSK_GPT_IC_EDGE_Pos             	12
#define CSK_GPT_IC_EDGE_Msk             	(3UL << CSK_GPT_IC_EDGE_Pos)
#define CSK_GPT_IC_EDGE_RISING        	 	(0UL << CSK_GPT_IC_EDGE_Pos)
#define CSK_GPT_IC_EDGE_FALLING				(1UL << CSK_GPT_IC_EDGE_Pos)
#define CSK_GPT_IC_EDGE_BOTH				(2UL << CSK_GPT_IC_EDGE_Pos)

/*----- GPT Input Capture Control Codes: Configure Parameters: Filter Selection -----*/
#define CSK_GPT_IC_FILTER_Pos            	15
#define CSK_GPT_IC_FILTER_Msk           	(7UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_1					(1UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_2					(2UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_3					(3UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_4					(4UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_5					(5UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_6					(6UL << CSK_GPT_IC_FILTER_Pos)
#define CSK_GPT_IC_FILTER_7					(7UL << CSK_GPT_IC_FILTER_Pos)


/*----- GPT Input Capture Control Codes: Configure Parameters: Clock divider -----*/
#define CSK_GPT_IC_CLKDIV_Pos               18
#define CSK_GPT_IC_CLKDIV_Msk               (7UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_1           	    (0UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_2         		(1UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_4         		(2UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_8         		(3UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_16         		(4UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_32         		(5UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_64         		(6UL << CSK_GPT_IC_CLKDIV_Pos)
#define CSK_GPT_IC_CLKDIV_128         		(7UL << CSK_GPT_IC_CLKDIV_Pos)

#define CSK_GPT_IC_TRANSFER_MODE_Pos		21
#define CSK_GPT_IC_TRANSFER_MODE_Msk        (3UL << CSK_GPT_IC_TRANSFER_MODE_Pos)
#define CSK_GPT_IC_TRANSFER_MODE_POLLING	(0UL << CSK_GPT_IC_TRANSFER_MODE_Pos)
#define CSK_GPT_IC_TRANSFER_MODE_INTERRUPT	(1UL << CSK_GPT_IC_TRANSFER_MODE_Pos)
#define CSK_GPT_IC_TRANSFER_MODE_DMA		(2UL << CSK_GPT_IC_TRANSFER_MODE_Pos)

#define CSK_GPT_IC_TRIGGER_RESET_Pos		23
#define CSK_GPT_IC_TRIGGER_RESET_Msk        (1UL << CSK_GPT_IC_TRIGGER_RESET_Pos)
#define CSK_GPT_IC_TRIGGER_NORESET			(0UL << CSK_GPT_IC_TRIGGER_RESET_Pos)
#define CSK_GPT_IC_TRIGGER_RESET			(1UL << CSK_GPT_IC_TRIGGER_RESET_Pos)


/**
  * @}
  */


/* Exported macro ------------------------------------------------------------*/
/** @defgroup IC_Exported_Macro IC Exported Macro
  * @{
  */

/**
  * @}
  */


/* Exported functions --------------------------------------------------------*/
/** @addtogroup IC_Exported_Functions IC Exported Functions
  * @{
  */

/** @addtogroup IC_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */

/*!
 \brief	 Get GPT0_IC resource instance
 */
void* GPT0_IC(void);

/**
 \fn          int32_t HAL_GPT_IcInitialize(void *pGpt, void *user_param)
 \brief		  Initialize GPT input capture interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcInitialize(void *pGpt, void *user_param);


/**
 \fn          int32_t HAL_GPT_IcUninitialize(void)
 \brief		  De-initialize GPT input capture interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcUninitialize(void *pGpt);


/**
 \fn          int32_t HAL_GPT_IcPowerControl(void *pGpt, CSK_POWER_STATE state)
 \brief		  GPT power control
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcPowerControl(void *pGpt, CSK_POWER_STATE state);


/**
 \fn          int32_t HAL_GPT_IcControl(void *pGpt, uint32_t control, uint32_t channel)
 \brief		  Control GPT input capture interface
 \param[in]   control: Operation
 \param[in]   channel: Input capture channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcControl(void *pGpt, uint32_t control, uint32_t channel);

/**
  * @}
  */

/** @addtogroup IC_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */

/*!
 \fn         int32_t HAL_GPT_RegisterIcCallback(void *pGpt, uint32_t channel, CSK_GPT_SignalEvent_t cb_event)
 \brief		 Sets the user callback when input capture generate
 \param[in]  channel: Input capture channel number
 \param[in]	 cb_event: User callback
 \return      \ref execution_status
 */
int32_t HAL_GPT_RegisterIcCallback(void *pGpt, uint32_t channel, CSK_GPT_SignalEvent_t cb_event);



/**
 \fn          HAL_GPT_GetIcData(void *pGpt, uint32_t channel, uint32_t *capdata, uint32_t length)
 \brief		  Get input capture data
 \param[in]   channel: Input capture channel number
 \param[in]   *capdata: data point for input capture data
 \param[in]   length: capture length
 \return      \ref execution_status
 */
int32_t HAL_GPT_GetIcData(void *pGpt, uint32_t channel, uint32_t *capdata, uint32_t length);

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

#endif /* __DRIVER_GPT_IC_H */
