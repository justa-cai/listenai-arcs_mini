/**
  ******************************************************************************
  * @file    trng_nos_chk.c
  * @author  ListenAI Application Team
  * @brief   TRNG module examples.
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


/* Includes ------------------------------------------------------------------*/
#include "stdio.h"
#include <string.h>

#include "Driver_TRNG.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "arcs_ap.h"

#include "nos_timer.h"

#include "log_print.h"


/** @addtogroup CSK_Periph_Examples
  * @{
  */


/* Private typedef -----------------------------------------------------------*/
typedef void (*function)(void);

/* Private define ------------------------------------------------------------*/


/* Private function prototypes -----------------------------------------------*/
void TRNG_Nist_evaluation(void);
void TRNG_Test_Polling(void);
void TRNG_Test_Interrupt(void);

/* Private macro -------------------------------------------------------------*/
#define TRNGCOUNT	50

//32000*32 = 1024000 bits
#define TEST_WORD_NUM 32000

/* Private variables ---------------------------------------------------------*/
volatile uint32_t testCnt = 0;

static function test_function_array[] = {
		TRNG_Test_Polling,
		TRNG_Test_Interrupt,
		TRNG_Nist_evaluation,
};

extern void BootClock_Init();


/**
 * @brief  Main program
 * @return
 */
int main(void)
{
    logInit(0, 115200);
    CLOGD("start");

    __HAL_CRM_TRNG_CLK_ENABLE();

    uint32_t times;
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }

	while(1);
}


static void TRNG_DataGenerate_Event(void* param){
    uint32_t trngdata;
    testCnt++;
	trngdata = HAL_TRNG_GetData(TRNG());
	CLOGD("trng data is 0x%x", trngdata);
	HAL_TRNG_Enable(TRNG());
	if(testCnt > TRNGCOUNT)
		HAL_TRNG_Disable(TRNG());

}


/** @addtogroup TRNG_Examples
  *  @brief    TRNG module examples
  *
 ===============================================================================
              		TRNG module features&examples list
 ===============================================================================
    <ul>
      <li><h2>TRNG module includes the following functions:</h2></li>
      <ul>
		  <li> Generate true random data</li>
		  <li> Write protect for Trng start</li>
		  <li> Delay timer can be configured for POR/TRNG analog/digital circuit</li>
		  <li> Random date interrupt status</li>
		  <li> Trng module ECO version and feature parameter(read only)</li>
		  <li> Trng module test/debug mode</li>
      </ul>
	</ul>

    <ul>
      <li><h2>TRNG examples includes the following functions:</h2></li>
      <ul>
		  <li> Delay timer and write protect registers are configured by all examples</li>
		  <li> Get true random data in polling mode, interrupt is masked:</li>
		  	  <ul><li>  void TRNG_Test_Polling(void);</li></ul>
		  <li> Get true random data in interrupt mode, interrupt is enabled:</li>
			  <ul><li>  void TRNG_Test_Interrupt(void);</li></ul>
		  <li> Get enough true random data for NIST test and get NIST report:</li>
			  <ul><li>  void TRNG_Nist_evaluation(void);</li></ul>
		  <li> Check trng ECO version and feature register:</li>
			  <ul><li>  void TRNG_Version_Check(void);</li></ul>
		  <li> Check trng test mode:</li>
			  <ul><li>  void TRNG_Test_DebugPort(void);</li></ul>
      </ul>
	</ul>
  * @{
  */


/**
  * @brief Get true random data in polling mode, interrupt is masked.
  * @param None
  * @retval None
  */
void TRNG_Test_Polling(void)
{
	/*********TRNG test all modes********/
    CLOGD("TRNG test all modes, test begin");
    uint32_t trngdata;

    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    //initialize
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
    HAL_TRNG_InterruptDisable(TRNG());
    CLOGD("trng data is below:");
    for(uint32_t cnt=0; cnt<TRNGCOUNT; cnt++)
    {
    	HAL_TRNG_Enable(TRNG());
    	while(HAL_TRNG_GetDataReady(TRNG()) == 0);
    	trngdata = HAL_TRNG_GetData(TRNG());
    	CLOGD("0x%x", trngdata);
    }

    CLOGD("trng polling test end!!!!");
}


/**
  * @brief Get true random data in interrupt mode, interrupt is enabled.
  * @param None
  * @retval None
  */
void TRNG_Test_Interrupt(void)
{
	/*********TRNG test interrupt modes********/
    CLOGD("TRNG test interrupt modes, test begin");
    testCnt = 0;

    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    //initialize
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);

    HAL_TRNG_RegisterCallback(TRNG(), TRNG_DataGenerate_Event);
    HAL_TRNG_InterruptEnable(TRNG());

    CLOGD("trng data is below:");
    HAL_TRNG_Enable(TRNG());

    while(1){
		if(testCnt > TRNGCOUNT){
			HAL_TRNG_Disable(TRNG());
			break;
		}
    }
	CLOGD("trng interrupt test end!!!!");
}

/**
  * @brief Get enough true random data for NIST test and get NIST report.
  * @param None
  * @retval None
  */
void TRNG_Nist_evaluation(void)
{
    logInit(0, 115200);

	/*********TRNG test all modes********/
    CLOGD("TRNG output data for NIST evaluation, begin");
    uint32_t trngdata;

    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    //initialize
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
    HAL_TRNG_InterruptDisable(TRNG());
    CLOG("python_array = [");
    for(uint32_t cnt=0; cnt<TEST_WORD_NUM; cnt++)
    {
    	HAL_TRNG_Enable(TRNG());
    	while(HAL_TRNG_GetDataReady(TRNG()) == 0);
    	trngdata = HAL_TRNG_GetData(TRNG());

    	CLOG("0x%02X, 0x%02X, 0x%02X, 0x%02X, ", trngdata & 0xFF, trngdata >>8 & 0xFF, trngdata >>16 & 0xFF, trngdata >>24 & 0xFF);
    }
	CLOG("]");
	CLOG("f=open('6001_randomDat.bin',mode='wb')");
	CLOG("print(len(python_array))");
	CLOG("f.write(bytearray(python_array))");
	CLOG("f.close()");

}

/**
  * @}
  */


/**
  * @}
  */

/************************ (C) COPYRIGHT ListenAI *****END OF FILE****/
