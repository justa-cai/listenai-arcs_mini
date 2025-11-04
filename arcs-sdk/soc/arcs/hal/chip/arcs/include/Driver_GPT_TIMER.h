/**
  ******************************************************************************
  * @file    Driver_TIMER.h
  * @author  ListenAI Application Team
  * @brief   Header file of TIMER HAL module.
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
#ifndef __DRIVER_GPT_TIMER_H
#define __DRIVER_GPT_TIMER_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"
#include "Driver_GPT_Common.h"


/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup TIMER
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/** @defgroup TIMER_Exported_Types TIMER Exported Types
  * @{
  */

/**
  * @brief  add description
  */



/**
  * @}
  */


/* Exported constants --------------------------------------------------------*/

/** @defgroup TIMER_Exported_Constants TIMER Exported Constants
  * @{
  */

/*---------------------Control mode for application---------------------------------*/
/****** GPT Timer Control Codes *****/
#define CSK_GPT_TIMER_CONTROL_Pos              0
#define CSK_GPT_TIMER_CONTROL_Msk             (0xFFUL << CSK_GPT_TIMER_CONTROL_Pos)

/*----- GPT Timer Control Codes: Mode -----*/
#define CSK_GPT_TIMER_32_BIT_TIMER            (0x01UL << CSK_GPT_TIMER_CONTROL_Pos) 
#define CSK_GPT_TIMER_16_BIT_TIMER            (0x02UL << CSK_GPT_TIMER_CONTROL_Pos)
#define CSK_GPT_TIMER_8_BIT_TIMER            	(0x03UL << CSK_GPT_TIMER_CONTROL_Pos)

/*----- GPT Timer Control Codes: Configure Parameters: Clock Source -----*/
#define CSK_GPT_TIMER_CLKSRC_Pos               9
#define CSK_GPT_TIMER_CLKSRC_Msk             (3UL << CSK_GPT_TIMER_CLKSRC_Pos)
#define CSK_GPT_TIMER_CLKSRC_PCLK          	 (2UL << CSK_GPT_TIMER_CLKSRC_Pos)       ///< peripheral clock (default)
#define CSK_GPT_TIMER_CLKSRC_CLKT1           (1UL << CSK_GPT_TIMER_CLKSRC_Pos)       ///< clock1 clock (default)
#define CSK_GPT_TIMER_CLKSRC_CLKT0         	 (0UL << CSK_GPT_TIMER_CLKSRC_Pos)       ///< clock0 clock

/*----- GPT Timer Control Codes: Configure Parameters: Clock divider -----*/
#define CSK_GPT_TIMER_CLKDIV_Pos               11
#define CSK_GPT_TIMER_CLKDIV_Msk              (7UL << CSK_GPT_TIMER_CLKDIV_Pos)
#define CSK_GPT_TIMER_CLKDIV_1           			(0UL << CSK_GPT_TIMER_CLKDIV_Pos)       
#define CSK_GPT_TIMER_CLKDIV_2         				(1UL << CSK_GPT_TIMER_CLKDIV_Pos) 
#define CSK_GPT_TIMER_CLKDIV_4         				(2UL << CSK_GPT_TIMER_CLKDIV_Pos)
#define CSK_GPT_TIMER_CLKDIV_8         				(3UL << CSK_GPT_TIMER_CLKDIV_Pos)
#define CSK_GPT_TIMER_CLKDIV_16         			(4UL << CSK_GPT_TIMER_CLKDIV_Pos)
#define CSK_GPT_TIMER_CLKDIV_32         			(5UL << CSK_GPT_TIMER_CLKDIV_Pos)
#define CSK_GPT_TIMER_CLKDIV_64         			(6UL << CSK_GPT_TIMER_CLKDIV_Pos)
#define CSK_GPT_TIMER_CLKDIV_128         			(7UL << CSK_GPT_TIMER_CLKDIV_Pos)

/*----- GPT Timer Run Codes: Configure Parameters: Run mode setting -----*/
#define CSK_GPT_TIMER_RUNMODE_Pos               14
#define CSK_GPT_TIMER_RUNMODE_Msk              (3UL << CSK_GPT_TIMER_RUNMODE_Pos)
#define CSK_GPT_TIMER_RUNMODE_SINGLE           (0UL << CSK_GPT_TIMER_RUNMODE_Pos)  
#define CSK_GPT_TIMER_RUNMODE_REPEAT           (1UL << CSK_GPT_TIMER_RUNMODE_Pos)  
#define CSK_GPT_TIMER_RUNMODE_FREERUN          (2UL << CSK_GPT_TIMER_RUNMODE_Pos)  
#define CSK_GPT_TIMER_RUNMODE_KEEPGO           (3UL << CSK_GPT_TIMER_RUNMODE_Pos)  

/*----- GPT Timer Counter Codes: Configure Parameters: Counter mode -----*/
#define CSK_GPT_TIMER_COUNTERMODE_Pos          16
#define CSK_GPT_TIMER_COUNTERMODE_Msk          (3UL << CSK_GPT_TIMER_COUNTERMODE_Pos)
#define CSK_GPT_TIMER_COUNTER_UP           		 (0UL << CSK_GPT_TIMER_COUNTERMODE_Pos)  
#define CSK_GPT_TIMER_COUNTER_DOWN           	 (1UL << CSK_GPT_TIMER_COUNTERMODE_Pos)  
#define CSK_GPT_TIMER_COUNTER_UPDOWN           (2UL << CSK_GPT_TIMER_COUNTERMODE_Pos) 

/**
  * @}
  */


/* Exported macro ------------------------------------------------------------*/
/** @defgroup TIMER_Exported_Macro TIMER Exported Macro
  * @{
  */

/**
  * @}
  */



/* Exported functions --------------------------------------------------------*/
/** @addtogroup TIMER_Exported_Functions TIMER Exported Functions
  * @{
  */

/** @addtogroup TIMER_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */

/*!
 \brief	 Get GPT0_TIMER resource instance
 */
void* GPT0_TIMER(void);


/**
 \fn          int32_t HAL_GPT_TimerInitialize(void)
 \brief		  Initialize GPT timer interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerInitialize(void *pGpt, void *user_param);


/**
 \fn          int32_t HAL_GPT_TimerUninitialize(void)
 \brief		  De-initialize GPT timer interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerUninitialize(void *pGpt);


/**
 \fn          int32_t GPT_TimerPowerControl(CSK_POWER_STATE state)
 \brief		  GPT power control
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerPowerControl(void *pGpt, CSK_POWER_STATE state);


/**
 \fn          int32_t GPT_TimerControl(void *pGpt, uint32_t control, uint32_t channel)
 \brief		  Control GPT interface
 \param[in]   control: Operation
 \param[in]   channel: Timer channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerControl(void *pGpt, uint32_t control, GPT_CHANNEL_TYPE channel);

/**
  * @}
  */


/** @addtogroup TIMER_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */

/*!
 \brief		 Sets the user callback when the timer time out
 \param[in]  channel Timer channel number
 \param[in]  cb_event User callback
 */
int32_t HAL_GPT_RegisterTimerCallback(void *pGpt, GPT_CHANNEL_TYPE channel, CSK_GPT_SignalEvent_t cb_event);


/*!
 \brief		 Sets the timer period in units of count.
 \param[in]  channel Timer channel number
 \param[in]  count Timer period in units of count
 */
int32_t HAL_GPT_SetTimerPeriodByCount(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t count);

/*!
 \brief      Sets the timer period in units of count directly(reload value update immediately).
 \param[in]  channel: Timer channel number
 \param[in]  count: Timer period in units of count
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetTimerPeriodDirectByCount(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t count);


/*!
 \brief		 Sets the timer period in units of ms.
 \param[in]  channel Timer channel number
 \param[in]  count Timer period in units of ms
 */
int32_t HAL_GPT_SetTimerPeriodByMs(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t ms);


/*!
 \brief      Sets the timer period in units of ms directly(reload value update immediately)..
 \param[in]  channel: Timer channel number
 \param[in]  count: Timer period in units of ms
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetTimerPeriodDirectByMs(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t ms);


/*!
 \brief		 Starts the timer counting.
 \param[in]  channel Timer channel number.
 */
int32_t HAL_GPT_StartTimer(void *pGpt, GPT_CHANNEL_TYPE channel);


/*!
 \brief		  Reads the current timer counting value.
 \param[in]   channel Timer channel number
 \param[out]  *count  32-bit result
 */
int32_t HAL_GPT_ReadTimerCount(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t *count);


/*!
 \brief		 Stops the timer counting.
 \param[in]  channel Timer channel number.
 */
int32_t HAL_GPT_StopTimer(void *pGpt, GPT_CHANNEL_TYPE channel);

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

#endif /* __DRIVER_GPT_TIMER_H */
