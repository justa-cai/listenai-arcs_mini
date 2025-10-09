/**
  ******************************************************************************
  * @file    Driver_PWM.h
  * @author  ListenAI Application Team
  * @brief   Header file of PWM HAL module.
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
#ifndef __DRIVER_GPT_PWM_H
#define __DRIVER_GPT_PWM_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"
#include "Driver_GPT_Common.h"


/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup PWM
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/** @defgroup PWM_Exported_Types PWM Exported Types
  * @{
  */

/**
  * @brief  add description
  */

/**
  * @}
  */


/* Exported constants --------------------------------------------------------*/

/** @defgroup PWM_Exported_Constants PWM Exported Constants
  * @{
  */

/*---------------------Control mode for application---------------------------------*/
/****** GPT PWM Control Codes *****/
#define CSK_GPT_PWM_CONTROL_Pos             0
#define CSK_GPT_PWM_CONTROL_Msk             (0xFFUL << CSK_GPT_PWM_CONTROL_Pos)
/*----- GPT PWM Control Codes: Mode -----*/
#define CSK_GPT_PWM_MODE            		(0x04UL << CSK_GPT_PWM_CONTROL_Pos)

/*----- GPT PWM Control Codes: Configure Parameters: Clock Source -----*/
#define CSK_GPT_PWM_CLKSRC_Pos              9
#define CSK_GPT_PWM_CLKSRC_Msk             (3UL << CSK_GPT_PWM_CLKSRC_Pos)
#define CSK_GPT_PWM_CLKSRC_PCLK            (2UL << CSK_GPT_PWM_CLKSRC_Pos)       ///< peripheral clock (default)
#define CSK_GPT_PWM_CLKSRC_EXT             (1UL << CSK_GPT_PWM_CLKSRC_Pos)       ///< external clock (default)
#define CSK_GPT_PWM_CLKSRC_XTAL            (0UL << CSK_GPT_PWM_CLKSRC_Pos)       ///< crystal clock

/*----- GPT PWM Output Codes: Mode -----*/
#define CSK_GPT_PWM_OUTMODE_Pos             11
#define CSK_GPT_PWM_OUTMODE_Msk             (1UL << CSK_GPT_PWM_OUTMODE_Pos)
#define CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED    (0UL << CSK_GPT_PWM_OUTMODE_Pos) 
#define CSK_GPT_PWM_OUTMODE_CENTRAL_ALIGNED (1UL << CSK_GPT_PWM_OUTMODE_Pos)

/*----- GPT PWM Output Codes: Polarity -----*/
#define CSK_GPT_PWM_OUTPOLARITY_Pos         12
#define CSK_GPT_PWM_OUTPOLARITY_Msk         (1UL << CSK_GPT_PWM_OUTPOLARITY_Pos)
#define CSK_GPT_PWM_OUTPOLARITY_LOW    		(0UL << CSK_GPT_PWM_OUTPOLARITY_Pos)
#define CSK_GPT_PWM_OUTPOLARITY_HIGH 		(1UL << CSK_GPT_PWM_OUTPOLARITY_Pos)

/*----- GPT PWM Control Codes: Configure Parameters: Clock divider -----*/
#define CSK_GPT_PWM_CLKDIV_Pos            	13
#define CSK_GPT_PWM_CLKDIV_Msk              (7UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_1           		(0UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_2         		(1UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_4         		(2UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_8         		(3UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_16         		(4UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_32         		(5UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_64         		(6UL << CSK_GPT_PWM_CLKDIV_Pos)
#define CSK_GPT_PWM_CLKDIV_128         		(7UL << CSK_GPT_PWM_CLKDIV_Pos)

/*----- GPT PWM opration Codes: Configure Parameters: One pulse/PWM/LEDC -----*/
#define CSK_GPT_PWM_OPERATION_MODE_Pos			16
#define CSK_GPT_PWM_OPERATION_MODE_Msk			(7UL << CSK_GPT_PWM_OPERATION_MODE_Pos)
#define CSK_GPT_PWM_OPERATION_MODE_ONEPULSE 	(2UL << CSK_GPT_PWM_OPERATION_MODE_Pos)
#define CSK_GPT_PWM_OPERATION_MODE_PWM 			(3UL << CSK_GPT_PWM_OPERATION_MODE_Pos) 
#define CSK_GPT_PWM_OPERATION_MODE_LEDC 		(4UL << CSK_GPT_PWM_OPERATION_MODE_Pos)
#define CSK_GPT_PWM_OPERATION_MODE_LEDC_NODMA 	(4UL << CSK_GPT_PWM_OPERATION_MODE_Pos)
#define CSK_GPT_PWM_OPERATION_MODE_LEDC_DMA 	(5UL << CSK_GPT_PWM_OPERATION_MODE_Pos)

/*----- GPT LEDC transfer mode: Configure Parameters: polling/interrupt/dma -----*/
#define CSK_GPT_LEDC_TRANSFER_MODE_Pos			19
#define CSK_GPT_LEDC_TRANSFER_MODE_Msk			(3UL << CSK_GPT_LEDC_TRANSFER_MODE_Pos)
#define CSK_GPT_LEDC_TRANSFER_MODE_POLLING 		(0UL << CSK_GPT_LEDC_TRANSFER_MODE_Pos)
#define CSK_GPT_LEDC_TRANSFER_MODE_INTERRUPT 	(1UL << CSK_GPT_LEDC_TRANSFER_MODE_Pos)
#define CSK_GPT_LEDC_TRANSFER_MODE_DMA 			(2UL << CSK_GPT_LEDC_TRANSFER_MODE_Pos)

/**
  * @}
  */


/* Exported macro ------------------------------------------------------------*/
/** @defgroup PWM_Exported_Macro PWM Exported Macro
  * @{
  */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @addtogroup PWM_Exported_Functions PWM Exported Functions
  * @{
  */

/** @addtogroup PWM_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
void* GPT0_PWM(void);


/**
 \fn          int32_t HAL_GPT_PWMInitialize(void)
 \brief		  Initialize GPT pwm interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMInitialize(void *pGpt, void *user_param);


/**
 \fn          int32_t HAL_GPT_PWMUninitialize(void)
 \brief		  De-initialize GPT pwm interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMUninitialize(void *pGpt);


/**
 \fn          int32_t HAL_GPT_PWMPowerControl(CSK_POWER_STATE state)
 \brief		  GPT power control
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMPowerControl(void *pGpt, CSK_POWER_STATE state);


/**
 \fn          int32_t HAL_GPT_PWMControl(void *pGpt, uint32_t control, uint32_t channel)
 \brief		  Control GPT Pwm interface
 \param[in]   control: Operation
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMControl(void *pGpt, uint32_t control, GPT_CHANNEL_TYPE channel);

/**
  * @}
  */

/** @addtogroup PWM_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */

/**
 \fn          int32_t HAL_GPT_SetPWMFreqDuty(void *pGpt, uint32_t channel, uint32_t freq, uint8_t duty)
 \brief		  Set PWM output frequency and duty cycles
 \param[in]   *pGpt: GPT0_PWM()
 \param[in]   channel: Pwm channel number
 \param[in]   freq: frequency setting
 \param[in]   duty: duty cycles setting(0<duty<100 represent 0%<duty<100%), duty should not be 0 or 100!!!!!!
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetPWMFreqDuty(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t freq, uint8_t duty);


/**
 \fn          int32_t HAL_GPT_SetPWMFreqDuty_NoShadow(void *pGpt, uint32_t channel, uint32_t freq, uint8_t duty)
 \brief       Set PWM output frequency and duty cycles without shadow load, must call HAL_GPT_ShadowLoad later
 \param[in]   *pGpt: GPT0_PWM()
 \param[in]   channel: Pwm channel number
 \param[in]   freq: frequency setting
 \param[in]   duty: duty cycles setting(0<duty<100 represent 0%<duty<100%), duty should not be 0 or 100!!!!!!
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetPWMFreqDuty_NoShadow(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t freq, uint8_t duty);


/**
 \fn          int32_t HAL_GPT_SetPWMFreqDutyByCounter_NoShadow(void *pGpt, GPT_CHANNEL_TYPE channel, uint16_t high16bits, uint16_t low16bits)
 \brief       Set PWM output frequency and duty cycles without shadow load, must call HAL_GPT_ShadowLoad later,
 \brief       Edge mode: Frequency = PCLK/DIV/(high16bits+low16bits), Center mode: Frequency = PCLK/DIV/high16bits,  low16bits represents high duty cycle counter
 \brief       Example: Edge mode: 16000 = 100000000/1/(high16bits+low16bits), Center mode: 16000 = 100000000/1/high16bits,  low16bits = high16bits*30%
 \param[in]   *pGpt: GPT0_PWM()
 \param[in]   high16bits: high duty cycle
 \param[in]   low16bits: low duty cycle
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetPWMFreqDutyByCounter_NoShadow(void *pGpt, GPT_CHANNEL_TYPE channel, uint16_t high16bits, uint16_t low16bits);



/**
 \fn          int32_t HAL_GPT_ShadowLoad(void *pGpt, GPT_CHANNEL_SYNC_TYPE channel)
 \brief       GPT counter load shadow, must called after HAL_GPT_SetPWMFreqDuty_NoShadow to load all shadow registers simultaneously.
 \param[in]   channel: GPT_CHANNEL_SYNC_TYPE
 \return      \ref execution_status
 */
int32_t HAL_GPT_ShadowLoad(void *pGpt, GPT_CHANNEL_SYNC_TYPE channel);



/**
 \fn          int32_t HAL_GPT_EnablePWM(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  	Enable PWM channel
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_EnablePWM(void *pGpt, GPT_CHANNEL_TYPE channel);


/**
 \fn          int32_t HAL_GPT_EnablePWM_NoCntStart(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief       Enable PWM channel without counter start, must call HAL_GPT_CntStart later
 \param[in]   channel: GPT_CHANNEL_TYPE
 \return      \ref execution_status
 */
int32_t HAL_GPT_EnablePWM_NoCntStart(void *pGpt, GPT_CHANNEL_TYPE channel);


/**
 \fn          int32_t HAL_GPT_CntStart(void *pGpt, GPT_CHANNEL_SYNC_TYPE channel)
 \brief       GPT counter start, must called after HAL_GPT_EnablePWM_NoCntStart to start all PWM simultaneously.
 \param[in]   channel: GPT_CHANNEL_SYNC_TYPE
 \return      \ref execution_status
 */
int32_t HAL_GPT_CntStart(void *pGpt, GPT_CHANNEL_SYNC_TYPE channel);


/**
 \fn          int32_t HAL_GPT_DisablePWM(void *pGpt, uint32_t channel)
 \brief		  Disable PWM channel
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_DisablePWM(void *pGpt, GPT_CHANNEL_TYPE channel);

/**
 \fn          int32_t HAL_GPT_EnableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  Enable LEDC channel
 \param[in]   channel: LEDC channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_EnableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel);

/**
 \fn          int32_t HAL_GPT_DisableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  Disable LEDC channel
 \param[in]   channel: LEDC channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_DisableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel);

/**
 \fn          int32_t HAL_GPT_LEDCOut(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t *senddata, uint32_t length)
 \brief		  Set LEDC output
 \param[in]   channel: LEDC channel number
 \param[in]   senddata: data sent to LEDC
 \param[in]   length: data length
 \return      \ref execution_status
 */
int32_t HAL_GPT_LEDCOut(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t *senddata, uint32_t length);


/**
 \fn          int32_t HAL_GPT_LEDCDutyCycle(void *pGpt, float highDutyCycle, float lowDutyCycle, GPT_CHANNEL_TYPE channel)
 \brief       Set LEDC duty cycle
 \param[in]   float highDutyCycle(us), Example 0.85
 \param[in]   float lowDutyCycle(us), Example 0.4
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_LEDCDutyCycle(void *pGpt, float highDutyCycle, float lowDutyCycle, GPT_CHANNEL_TYPE channel);


/*!
 \fn         int32_t HAL_GPT_RegisterLEDCCallback(GPT_CHANNEL_TYPE channel, CSK_GPT_SignalEvent_t cb_event)
 \brief		 Sets the user callback when the ledc data send finished
 \param[in]  channel: PWM channel number
 \param[in]	 cb_event: User callback
 \return      \ref execution_status
 */
int32_t HAL_GPT_RegisterLEDCCallback(void *pGpt, GPT_CHANNEL_TYPE channel, CSK_GPT_SignalEvent_t cb_event);

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

#endif /* __DRIVER_GPT_PWM_H */
