/**
  ******************************************************************************
  * @file    Driver_GPADC.h
  * @author  ListenAI Application Team
  * @brief   Header file of GPADC HAL module.
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
#ifndef __CSK_DRIVER_GPADC_H
#define __CSK_DRIVER_GPADC_H


#ifdef __cplusplus
 extern "C" {
#endif

 /* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"

/**
  * @brief  add description
  */
typedef void (*CSK_GPADC_SignalEvent_t) (uint32_t event, void* workspace);

/*----- GPADC channel define -----*/
typedef enum {
    CSK_GPADC_VBAT                      = 0,                /**< GPADC channel bat */
    CSK_GPADC_KEYSENSE0                 = 1,                /**< GPADC channel keysense0 */
    CSK_GPADC_KEYSENSE1                 = 2,                /**< GPADC channel keysense1 */
    CSK_GPADC_TEMPSENSOR                = 3,                /**< GPADC channel tempsensor */
    CSK_GPADC_CHANNEL0                  = 4,                /**< GPADC channel 0 */
    CSK_GPADC_CHANNEL1                  = 5,                /**< GPADC channel 1 */
    CSK_GPADC_CHANNEL2                  = 6,                /**< GPADC channel 2 */
    CSK_GPADC_CHANNEL3                  = 7,                /**< GPADC channel 3 */
    CSK_GPADC_CHANNEL4                  = 8,                /**< GPADC channel 4 */
    CSK_GPADC_CHANNEL5                  = 9,                /**< GPADC channel 5 */
	CSK_GPADC_CVD0                  	= 10,                /**< GPADC channel cvd0 */
	CSK_GPADC_CVD1                		= 11,				/**< GPADC channel cvd1 */
	CSK_GPADC_CVD2                  	= 12,                /**< GPADC channel cvd2 */
	CSK_GPADC_CVD3                		= 13,				/**< GPADC channel cvd3 */
	CSK_GPADC_CVD4                  	= 14,                /**< GPADC channel cvd4 */
	CSK_GPADC_CVD5                		= 15,				/**< GPADC channel cvd5 */
} GPADC_CHANNEL_TYPE;


typedef enum {
	CSK_GPADC_FIFO_EMPTY                  = 0,
	CSK_GPADC_FIFO_FULL                  = 1,
	CSK_GPADC_FIFO_THD                  = 2,
}GPADC_FIFOINT_TYPE;


#define CSK_GPADC_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)
#define CSK_GPADC_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)


#define CSK_GPADC_CHANNEL_NUM					  0x08

/*---------------------Control mode for application---------------------------------*/

/*----- GPADC Channel select : (VBAT/3) / (KEYSENSE0/3) / (KEYSENSE1/3) / TEMP_SENSOR / (VIN0) / (VIN1) / VIN2 / VIN3 / VIN4 / VIN5 -----*/
#define CSK_GPADC_CHANNEL_SEL_Pos             	  0
#define CSK_GPADC_CHANNEL_SEL_Msk             	  (0xFFFFUL << CSK_GPADC_CHANNEL_SEL_Pos)  // bit[15:0]
#define CSK_GPADC_CHANNEL_SEL_VBAT                (0x01UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_KEYSENSE0           (0x02UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_KEYSENSE1           (0x04UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_TEMP                (0x08UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_0                   (0x10UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_1                   (0x20UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_2                   (0x40UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_3                   (0x80UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_4                   (0x100UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_5                   (0x200UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_CVD0				  (0x400UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_CVD1				  (0x800UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_CVD2				  (0x1000UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_CVD3				  (0x2000UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_CVD4				  (0x4000UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_CVD5				  (0x8000UL << CSK_GPADC_CHANNEL_SEL_Pos)
#define CSK_GPADC_CHANNEL_SEL_ALL                 (0xFFFFUL << CSK_GPADC_CHANNEL_SEL_Pos)

/*----- GPADC data transfer mode : dma mode enable/disable -----*/
#define CSK_GPADC_DMA_ENABLE_Pos             	  16
#define CSK_GPADC_DMA_ENABLE_Msk             	  (0xFFFFUL << CSK_GPADC_DMA_ENABLE_Pos)   //bit[31:16]
#define CSK_GPADC_DMA_ENABLE(n)                   (((n) & 0xFFFFUL) << CSK_GPADC_DMA_ENABLE_Pos)

/*------CVD control-------*/
#define CSK_GPADC_GURADRING_ENABLE					1
#define CSK_GPADC_GURADRING_DISABLE					0

#define CSK_GPADC_DOUBLE_SAPMLE_ENABLE				1
#define CSK_GPADC_DOUBLE_SAPMLE_DISABLE				0

#define CSK_GPADC_CHARGE_INV_ENABLE					1
#define CSK_GPADC_CHARGE_INV_DISABLE				0


/*----- CVD prechargeA timer -----*/
#define CSK_GPADC_PRECHARGEA_CONTROL_Pos             0
#define CSK_GPADC_PRECHARGEA_CONTROL_Msk             (0xFFUL << CSK_GPADC_PRECHARGEA_CONTROL_Pos)

/*----- CVD prechargeB timer -----*/
#define CSK_GPADC_PRECHARGEB_CONTROL_Pos             8
#define CSK_GPADC_PRECHARGEB_CONTROL_Msk             (0xFFUL << CSK_GPADC_PRECHARGEB_CONTROL_Pos)

/*----- CVD Acquicition timer -----*/
#define CSK_GPADC_ACQUICITION_CONTROL_Pos            16
#define CSK_GPADC_ACQUICITION_CONTROL_Msk            (0xFFUL << CSK_GPADC_ACQUICITION_CONTROL_Pos)

/*----- CVD Additional Capacitor -----*/
#define CSK_GPADC_ADDITIONAL_CAP_CONTROL_Pos         24
#define CSK_GPADC_ADDITIONAL_CAP_CONTROL_Msk         (0x07UL << CSK_GPADC_ADDITIONAL_CAP_CONTROL_Pos)


/****** GPADC Event *****/
#define CSK_GPADC_COMPLETE  						(0x04)
#define CSK_GPADC_EOC_ERROR  						(0x05)


/** @GPADC_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
void* GPADC(void);
int32_t HAL_GPADC_Initialize(void *res);
int32_t HAL_GPADC_Uninitialize(void *res);
uint32_t HAL_GPADC_Control(void* res, uint32_t control);
uint32_t HAL_GPADC_Start(void* res);
uint32_t HAL_GPADC_Stop(void* res);
uint32_t HAL_GPADC_PollForConversion(void* res, uint32_t Timeout);
uint32_t HAL_GPADC_Start_IT(void* res);
uint32_t HAL_GPADC_Stop_IT(void* res);
uint16_t HAL_GPADC_GetValue(void* res, uint32_t channelnum);
uint32_t HAL_GPADC_SetTriggerNum(void* res, uint32_t trignum);
uint32_t HAL_GPADC_SetSampleWaitTime(void* res, uint32_t waitTime);
uint32_t HAL_GPADC_SetSetupWaitTime(void* res, uint8_t waitTime);
uint32_t HAL_GPADC_SetFifoThd0(void* res, uint8_t channelnum, uint8_t threshold);
uint32_t HAL_GPADC_SetFifoThd1(void* res, uint8_t channelnum, uint8_t threshold);
uint32_t HAL_GPADC_EnableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type);
uint32_t HAL_GPADC_DisableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type);
uint32_t HAL_GPADC_EnableCmpInterrupt(void* res);
uint32_t HAL_GPADC_DisableCmpInterrupt(void* res);
uint32_t HAL_GPADC_SetVinBuf_Enable(void* res, uint8_t enable);
uint32_t HAL_GPADC_SetVrefSel(void* res, uint8_t vrefsel);
uint32_t HAL_GPADC_SetKeysenseTrigger_enable(void* res, uint8_t enable);
uint32_t HAL_GPADC_SetGptTrigger_enable(void* res, uint8_t enable);

int32_t HAL_GPADC_RegisterChannelCallback(void *res, GPADC_CHANNEL_TYPE channel, CSK_GPADC_SignalEvent_t cb_event);
int32_t HAL_GPADC_RegisterCompleteCallback(void *res, CSK_GPADC_SignalEvent_t cb_event);

uint32_t HAL_GPADC_CVD_Config(void* res, uint32_t gr_state, uint32_t db_sample_state, uint32_t second_charge_invert);
uint32_t HAL_GPADC_CVD_Control(void* res, uint32_t control);
uint32_t HAL_GPADC_CVD_GetValue(void* res, uint32_t channelnum);

uint32_t ls_rand(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_GPADC_H */
