/**
  ******************************************************************************
  * @file    gpt_timer_nos_chk.c
  * @author  ListenAI Application Team
  * @brief   GPT Test Case.
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

#include "Driver_GPT_Common.h"
#include "Driver_GPT_IC.h"
#include "Driver_GPT_PWM.h"
#include "Driver_GPT_TIMER.h"

#include "log_print.h"
#include "IOMuxManager.h"
#include "chip.h"
#include "unity.h"

#include "ClockManager.h"

#include "systick.h"
#include "Driver_GPIO.h"




/* Private typedef -----------------------------------------------------------*/
typedef void (*function)(void);
typedef void (*ic_func_cb)(uint32_t event, void* param);
typedef void (*timer_func_cb)(uint32_t event, void* param);

typedef struct pad_info{
	uint32_t pad_group;
	uint32_t pad_num;
	uint32_t pad_func;
}Pad_Typedef;

typedef struct ic_info{
	uint32_t channel_num;
	Pad_Typedef pad;
	ic_func_cb cb;
}Ic_Info_Typedef;

/* Private define ------------------------------------------------------------*/
#define GPT_PERIOD_32BIT 	  (0xE3600)
#define GPT_PERIOD_16BIT 	  (0x55558888)
#define GPT_PERIOD_8BIT 	  (0x3388BBFF)

#define LEDC1_OUT_PAD          (CSK_IOMUX_PAD_A)
#define LEDC1_OUT_PIN          (30)
#define LEDC1_OUT_SEL          (12)

#define LEDC2_OUT_PAD          (CSK_IOMUX_PAD_A)
#define LEDC2_OUT_PIN          (31)
#define LEDC2_OUT_SEL          (12)

#define PWM_CH0_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH0_PIN           (30)
#define PWM_CH0_SEL           (12)

#define PWM_CH1_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH1_PIN           (31)
#define PWM_CH1_SEL           (12)

#define PWM_CH2_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH2_PIN           (2)
#define PWM_CH2_SEL           (12)

#define PWM_CH3_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH3_PIN           (11)
#define PWM_CH3_SEL           (12)

#define PWM_CH4_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH4_PIN           (04)
#define PWM_CH4_SEL           (12)

#define PWM_CH5_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH5_PIN           (5)
#define PWM_CH5_SEL           (12)

#define PWM_CH6_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH6_PIN           (6)
#define PWM_CH6_SEL           (12)

#define PWM_CH7_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH7_PIN           (7)
#define PWM_CH7_SEL           (12)

#define IC_CH0_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH0_PIN            (10)
#define IC_CH0_SEL            (11)

#define IC_CH1_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH1_PIN            (11)
#define IC_CH1_SEL            (11)

#define IC_CH2_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH2_PIN            (4)
#define IC_CH2_SEL            (11)

#define IC_CH3_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH3_PIN            (13)
#define IC_CH3_SEL            (11)

#define IC_CH4_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH4_PIN            (14)
#define IC_CH4_SEL            (11)

#define IC_CH5_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH5_PIN            (15)
#define IC_CH5_SEL            (11)

#define IC_CH6_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH6_PIN            (16)
#define IC_CH6_SEL            (11)

#define IC_CH7_PAD            (CSK_IOMUX_PAD_A)
#define IC_CH7_PIN            (17)
#define IC_CH7_SEL            (11)


/* Private function prototypes -----------------------------------------------*/
void GPT_TIMER_Interrupt_Repeat_AllChannel_32bit(void);
void GPT_TIMER_Interrupt_Repeat_AllChannel_16bit(void);
void GPT_TIMER_Interrupt_Repeat_AllChannel_8bit(void);
void GPT_TIMER_Interrupt_Single_AllChannel_32bit(void);
void GPT_TIMER_Interrupt_Single_AllChannel_16bit(void);
void GPT_TIMER_Interrupt_Single_AllChannel_8bit(void);
void GPT_TIMER_Interrupt_Repeat_Channel0_32bit(void);
void GPT_TIMER_Interrupt_single_Channel1_16bit(void);
void GPT_TIMER_Interrupt_single_Channel2_8bit(void);
void GPT_TIMER_Interrupt_Repeat_Channel3_32bit(void);
void GPT_TIMER_Interrupt_single_Channel4_16bit(void);
void GPT_TIMER_Interrupt_single_Channel5_8bit(void);
void GPT_TIMER_Interrupt_Repeat_Channel6_32bit(void);
void GPT_TIMER_Interrupt_single_Channel7_16bit(void);

void GPT_PWM_Output_AllChannel(void);
void GPT_PWM_Output_Channel0(void);
void GPT_PWM_Output_Channel1(void);
void GPT_PWM_Output_Channel2(void);
void GPT_PWM_Output_Channel3(void);
void GPT_PWM_Output_Channel4(void);
void GPT_PWM_Output_Channel6(void);

void GPT_InputCapture_Polling_AllChannel(void);
void GPT_InputCapture_Polling_Channel2(void);

void GPT_InputCapture_Interrupt_AllChannel(void);
void GPT_InputCapture_Interrupt_Channel2(void);
void GPT_InputCapture_Interrupt_Channel4(void);
void GPT_InputCapture_Interrupt_Channel5(void);
void GPT_InputCapture_Interrupt_Channel6(void);
void GPT_InputCapture_Interrupt_Channel7(void);

void GPT_InputCapture_Dma_AllChannel(void);
void GPT_InputCapture_Dma_Channel0(void);
void GPT_InputCapture_Dma_Channel1(void);
void GPT_InputCapture_Dma_Channel2(void);
void GPT_InputCapture_Dma_Channel3(void);

void GPT_LEDC_Polling_Output(void);
void GPT_LEDC_Interrupt_Output(void);
void GPT_LEDC_Dma_Output(void);

void GPT_PWM_MotoTest(void);

void WaitgPassFlag(uint32_t count);

//callback functions
static void GPT_Timer_Channel0_Event(uint32_t event, void* param);
static void GPT_Timer_Channel1_Event(uint32_t event, void* param);
static void GPT_Timer_Channel2_Event(uint32_t event, void* param);
static void GPT_Timer_Channel3_Event(uint32_t event, void* param);
static void GPT_Timer_Channel4_Event(uint32_t event, void* param);
static void GPT_Timer_Channel5_Event(uint32_t event, void* param);
static void GPT_Timer_Channel6_Event(uint32_t event, void* param);
static void GPT_Timer_Channel7_Event(uint32_t event, void* param);
static void GPT_Ledc_Channel1_Sendout_Event(uint32_t event, void* param);
static void GPT_Ledc_Channel2_Sendout_Event(uint32_t event, void* param);
static void GPT_Ic_Channel0_Event(uint32_t event, void* param);
static void GPT_Ic_Channel1_Event(uint32_t event, void* param);
static void GPT_Ic_Channel2_Event(uint32_t event, void* param);
static void GPT_Ic_Channel3_Event(uint32_t event, void* param);
static void GPT_Ic_Channel4_Event(uint32_t event, void* param);
static void GPT_Ic_Channel5_Event(uint32_t event, void* param);
static void GPT_Ic_Channel6_Event(uint32_t event, void* param);
static void GPT_Ic_Channel7_Event(uint32_t event, void* param);


#include "Driver_GPIO.h"
#include "IOMuxManager.h"


static void* GPIOA_Handler = NULL;

static void GPIOA_PIN5_OUT(void){
    GPIOA_Handler = GPIOA();

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, NULL, NULL);
//
//    GPIO_Control(GPIOA_Handler, CSK_GPIO_MODE_PULL_NONE | \
//                                CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN11);

    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN5, CSK_GPIO_DIR_OUTPUT);
while(1){
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN5, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN5, 0);
    SysTick_Delay_Ms(100);
}
    GPIO_Uninitialize(GPIOA_Handler);
}



/* Private variables ---------------------------------------------------------*/
static volatile uint32_t gPassFlag = 0;
static volatile uint32_t gInteruptCnt = 0;
//SYSCFG_RegDef*  gSYSTEM_CLKreg;

const uint32_t gpt_pwm_pin_array[8][3] = {{PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL},
										  {PWM_CH1_PAD, PWM_CH1_PIN, PWM_CH1_SEL},
										  {PWM_CH2_PAD, PWM_CH2_PIN, PWM_CH2_SEL},
										  {PWM_CH3_PAD, PWM_CH3_PIN, PWM_CH3_SEL},
										  {PWM_CH4_PAD, PWM_CH4_PIN, PWM_CH4_SEL},
										  {PWM_CH5_PAD, PWM_CH5_PIN, PWM_CH5_SEL},
										  {PWM_CH6_PAD, PWM_CH6_PIN, PWM_CH6_SEL},
										  {PWM_CH7_PAD, PWM_CH7_PIN, PWM_CH7_SEL},};

const timer_func_cb gpt_timer_cb_array[8] = {GPT_Timer_Channel0_Event,
											 GPT_Timer_Channel1_Event,
											 GPT_Timer_Channel2_Event,
											 GPT_Timer_Channel3_Event,
											 GPT_Timer_Channel4_Event,
											 GPT_Timer_Channel5_Event,
											 GPT_Timer_Channel6_Event,
											 GPT_Timer_Channel7_Event,};

const Ic_Info_Typedef input_channel_info[8] = {{.channel_num = GPT_CHANNEL0, .pad.pad_group = IC_CH0_PAD, .pad.pad_num = IC_CH0_PIN, .pad.pad_func = IC_CH0_SEL, .cb = GPT_Ic_Channel0_Event},
						   	   	  	  	  	   {.channel_num = GPT_CHANNEL1, .pad.pad_group = IC_CH1_PAD, .pad.pad_num = IC_CH1_PIN, .pad.pad_func = IC_CH1_SEL, .cb = GPT_Ic_Channel1_Event},
											   {.channel_num = GPT_CHANNEL2, .pad.pad_group = IC_CH2_PAD, .pad.pad_num = IC_CH2_PIN, .pad.pad_func = IC_CH2_SEL, .cb = GPT_Ic_Channel2_Event},
											   {.channel_num = GPT_CHANNEL3, .pad.pad_group = IC_CH3_PAD, .pad.pad_num = IC_CH3_PIN, .pad.pad_func = IC_CH3_SEL, .cb = GPT_Ic_Channel3_Event},
											   {.channel_num = GPT_CHANNEL4, .pad.pad_group = IC_CH4_PAD, .pad.pad_num = IC_CH4_PIN, .pad.pad_func = IC_CH4_SEL, .cb = GPT_Ic_Channel4_Event},
											   {.channel_num = GPT_CHANNEL5, .pad.pad_group = IC_CH5_PAD, .pad.pad_num = IC_CH5_PIN, .pad.pad_func = IC_CH5_SEL, .cb = GPT_Ic_Channel5_Event},
											   {.channel_num = GPT_CHANNEL6, .pad.pad_group = IC_CH6_PAD, .pad.pad_num = IC_CH6_PIN, .pad.pad_func = IC_CH6_SEL, .cb = GPT_Ic_Channel6_Event},
											   {.channel_num = GPT_CHANNEL7, .pad.pad_group = IC_CH7_PAD, .pad.pad_num = IC_CH7_PIN, .pad.pad_func = IC_CH7_SEL, .cb = GPT_Ic_Channel7_Event},};

//PWM Channel number <----> IC Channel number
//const uint32_t pwm_ic_channel_connect[8][2] = {{GPT_CHANNEL7, GPT_CHANNEL0},
//											   {GPT_CHANNEL0, GPT_CHANNEL1},
//											   {GPT_CHANNEL1, GPT_CHANNEL2},
//											   {GPT_CHANNEL2, GPT_CHANNEL3},
//											   {GPT_CHANNEL3, GPT_CHANNEL4},
//											   {GPT_CHANNEL4, GPT_CHANNEL5},
//											   {GPT_CHANNEL5, GPT_CHANNEL6},
//											   {GPT_CHANNEL6, GPT_CHANNEL7},};

//for arcs test
const uint32_t pwm_ic_channel_connect[1][2] = {{GPT_CHANNEL0, GPT_CHANNEL2},};

/*
PWM_CH0:LC7			IC_CH1:GPIB7:LC4
PWM_CH1:PC0			IC_CH2:GPIB6:PC4
PWM_CH2:PC1			IC_CH3:GPIB5:PC3
PWM_CH3:DC0			IC_CH4:GPIB4:PC2
PWM_CH4:TC6			IC_CH5:GPIB3:LC3
PWM_CH5:TC7			IC_CH6:GPIB2:LC2
PWM_CH6:TC2			IC_CH7:GPIB1:LC1
PWM_CH7:TC3			IC_CH0:GPIB8:LC5

*/


/* Private functions ---------------------------------------------------------*/
void setUp(void) {
    // test configure
}

void tearDown(void) {
    // test clear
}

uint32_t test_function_should_doBlahAndBlah(void) {
    // test content
    return 2;
}


int main(void)
{
    logInit(0, 115200);

//    CLOGD("Test begin");

    UNITY_BEGIN();
//    RUN_TEST(GPT_TIMER_Interrupt_Repeat_AllChannel_32bit);
//    RUN_TEST(GPT_TIMER_Interrupt_Single_AllChannel_32bit);
//    RUN_TEST(GPT_TIMER_Interrupt_Repeat_AllChannel_16bit);
//    RUN_TEST(GPT_TIMER_Interrupt_Single_AllChannel_16bit);
//    RUN_TEST(GPT_TIMER_Interrupt_Repeat_AllChannel_8bit);
//    RUN_TEST(GPT_TIMER_Interrupt_Single_AllChannel_8bit);
//    RUN_TEST(GPT_PWM_Output_AllChannel);
//    RUN_TEST(GPT_InputCapture_Polling_AllChannel);
//    RUN_TEST(GPT_InputCapture_Interrupt_AllChannel);
//    RUN_TEST(GPT_InputCapture_Dma_AllChannel);
//    RUN_TEST(GPT_LEDC_Polling_Output);
//    RUN_TEST(GPT_LEDC_Interrupt_Output);
    RUN_TEST(GPT_LEDC_Dma_Output);
//    RUN_TEST(GPT_PWM_MotoTest);

    return UNITY_END();
	while(1);
}




void WaitgPassFlag(uint32_t count)
{
	for(uint32_t i=0; i<count; i++){
		while(gPassFlag == 0);
		gPassFlag = 0;
	}
}


static void GPT_Timer_Channel0_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel0 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL0);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}

static void GPT_Timer_Channel1_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel1 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL1);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}

static void GPT_Timer_Channel2_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel2 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL2);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}

}

static void GPT_Timer_Channel3_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel3 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL3);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}


uint32_t toggle = 1;

static void GPT_Timer_Channel3_Test_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
	    //add gpio toggle
	    if(toggle == 1){
	        toggle = 0;
	        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 1);
	        HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), 0, 8000, 50);
	        HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), 1, 8000, 50);
	        HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), 2, 8000, 50);

            GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 0);
	    }else{
            toggle = 1;
            GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 1);
            HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), 0, 8000, 20);
            HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), 1, 8000, 20);
            HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), 2, 8000, 20);

            GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 0);
	    }
	    //shadow load
        HAL_GPT_ShadowLoad(GPT0_PWM(), (GPT_CHANNEL0_SYNC | GPT_CHANNEL1_SYNC | GPT_CHANNEL2_SYNC));
	}
}





static void GPT_Timer_Channel4_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel4 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL4);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}

static void GPT_Timer_Channel5_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel5 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL5);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}


static void GPT_Timer_Channel6_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel6 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL6);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}


static void GPT_Timer_Channel7_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_OVERFLOW )
	{
//		CLOGD("gpt timer channel7 overflow event generated");
		uint32_t runmode = *(uint32_t*)(param);

		if(runmode == CSK_GPT_TIMER_RUNMODE_REPEAT){
			gInteruptCnt++;
			if(gInteruptCnt == 3){
				gInteruptCnt = 0;
				HAL_GPT_StopTimer(GPT0_TIMER(), GPT_CHANNEL7);
				gPassFlag = 1;
			}
		}else{
			gInteruptCnt++;
			if(gInteruptCnt == 1){
				gInteruptCnt = 0;
				gPassFlag = 1;
			}
		}
	}
}


static void GPT_Ledc_Channel0_Sendout_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_LEDC_TX_DONE )
	{
		HAL_GPT_DisableLEDC(GPT0_PWM(), GPT_CHANNEL0);
		CLOGD("gpt ledc sendout event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ledc_Channel1_Sendout_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_LEDC_TX_DONE )
	{
		HAL_GPT_DisableLEDC(GPT0_PWM(), GPT_CHANNEL1);
		CLOGD("gpt ledc sendout event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ledc_Channel2_Sendout_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_LEDC_TX_DONE )
	{
		HAL_GPT_DisableLEDC(GPT0_PWM(), GPT_CHANNEL2);
		CLOGD("gpt ledc sendout event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel0_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel0 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel1_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel1 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel2_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel2 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel3_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel3 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel4_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel4 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel5_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel5 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel6_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel6 event generated");
		gPassFlag = 1;
	}
}


static void GPT_Ic_Channel7_Event(uint32_t event, void* param){
	if( event & CSK_GPT_EVENT_INPUTCAPTURE )
	{
		CLOGD("gpt input capture channel7 event generated");
		gPassFlag = 1;
	}
}

void GPT_TIMER_Interrupt_Repeat_AllChannel_32bit(void)
{
	/*********GPT timer repeat mode all channel, 32bit timer********/
//   CLOGD("GPT timer repeat mode all channel, 32bit timer, test begin");
	uint32_t ret;
	for(uint32_t channel=0; channel<8; channel++){
		uint32_t runmode = CSK_GPT_TIMER_RUNMODE_REPEAT;

		ret = HAL_GPT_TimerInitialize(GPT0_TIMER(), &runmode);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_2 |
														 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_UP, channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), channel, gpt_timer_cb_array[channel]);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), channel, GPT_PERIOD_32BIT);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_StartTimer(GPT0_TIMER(), channel);
		WaitgPassFlag(1);
		HAL_GPT_TimerUninitialize(GPT0_TIMER());
	}

//    CLOGD("GPT timer repeat mode all channel, 32bit timer, test end");
}


void GPT_TIMER_Interrupt_Repeat_AllChannel_16bit(void)
{
	/*********GPT timer repeat mode all channel, 16bit timer********/
//    CLOGD("GPT timer repeat mode all channel, 16bit timer, test begin");
	uint32_t ret;
	for(uint32_t channel=0; channel<8; channel++){
		uint32_t runmode = CSK_GPT_TIMER_RUNMODE_REPEAT;

		ret = HAL_GPT_TimerInitialize(GPT0_TIMER(), &runmode);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_16_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_64 |
														 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_DOWN, channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), channel, gpt_timer_cb_array[channel]);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), channel, GPT_PERIOD_16BIT);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_StartTimer(GPT0_TIMER(), channel);
		WaitgPassFlag(1);
		HAL_GPT_TimerUninitialize(GPT0_TIMER());
	}

//    CLOGD("GPT timer repeat mode all channel, 16bit timer, test end\n");
}


void GPT_TIMER_Interrupt_Repeat_AllChannel_8bit(void)
{
	/*********GPT timer repeat mode all channel, 8bit timer********/
//    CLOGD("GPT timer repeat mode all channel, 8bit timer, test begin");
	uint32_t ret;
	for(uint32_t channel=0; channel<8; channel++){
		uint32_t runmode = CSK_GPT_TIMER_RUNMODE_REPEAT;

		ret = HAL_GPT_TimerInitialize(GPT0_TIMER(), &runmode);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_8_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_128 |
														 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_UPDOWN, channel);

		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), channel, gpt_timer_cb_array[channel]);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), channel, GPT_PERIOD_8BIT);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_StartTimer(GPT0_TIMER(), channel);
		WaitgPassFlag(1);
		HAL_GPT_TimerUninitialize(GPT0_TIMER());
	}

//    CLOGD("GPT timer repeat mode all channel, 8bit timer, test end\n");
}


void GPT_TIMER_Interrupt_Single_AllChannel_32bit(void)
{
	/*********GPT timer single mode all channel, 32bit timer********/
//    CLOGD("GPT timer single mode all channel, 32bit timer, test begin");
	uint32_t ret;
	for(uint32_t channel=0; channel<8; channel++){
		uint32_t runmode = CSK_GPT_TIMER_RUNMODE_SINGLE;

		ret = HAL_GPT_TimerInitialize(GPT0_TIMER(), &runmode);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_2 |
							   CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_UP, channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), channel, gpt_timer_cb_array[channel]);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), channel, GPT_PERIOD_32BIT);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_StartTimer(GPT0_TIMER(), channel);
		WaitgPassFlag(1);
		HAL_GPT_TimerUninitialize(GPT0_TIMER());
	}

//    CLOGD("GPT timer single mode all channel, 32bit timer, test end\n");
}


void GPT_TIMER_Interrupt_Single_AllChannel_16bit(void)
{
	/*********GPT timer single mode all channel, 16bit timer********/
//    CLOGD("GPT timer single mode all channel, 16bit timer, test begin");
	uint32_t ret;
	for(uint32_t channel=0; channel<8; channel++){
		uint32_t runmode = CSK_GPT_TIMER_RUNMODE_SINGLE;

		ret = HAL_GPT_TimerInitialize(GPT0_TIMER(), &runmode);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_16_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_64 |
							   CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), channel, gpt_timer_cb_array[channel]);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), channel, GPT_PERIOD_16BIT);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_StartTimer(GPT0_TIMER(), channel);
		WaitgPassFlag(1);
		HAL_GPT_TimerUninitialize(GPT0_TIMER());
	}

//    CLOGD("GPT timer single mode all channel, 16bit timer, test end\n");
}

void GPT_TIMER_Interrupt_Single_AllChannel_8bit(void)
{
	/*********GPT timer single mode all channel, 8bit timer********/
//    CLOGD("GPT timer single mode all channel, 8bit timer, test begin");
	uint32_t ret;
	for(uint32_t channel=0; channel<8; channel++){
		uint32_t runmode = CSK_GPT_TIMER_RUNMODE_SINGLE;

		ret = HAL_GPT_TimerInitialize(GPT0_TIMER(), &runmode);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_8_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_128 |
							   CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_UPDOWN, channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), channel, gpt_timer_cb_array[channel]);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), channel, GPT_PERIOD_8BIT);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_StartTimer(GPT0_TIMER(), channel);
		WaitgPassFlag(1);
		HAL_GPT_TimerUninitialize(GPT0_TIMER());
	}

//    CLOGD("GPT timer single mode all channel, 8bit timer, test end\n");
}

void GPT_TIMER_Interrupt_Repeat_Channel0_32bit(void)
{
	/*********GPT timer repeat mode channel0, 32bit timer********/
//    CLOGD("GPT timer repeat mode channel0, 32bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_2 |
													 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_UP, GPT_CHANNEL0);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL0, GPT_Timer_Channel0_Event);
	HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL0, GPT_PERIOD_32BIT);

	HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL0);
	WaitgPassFlag(1);
	HAL_GPT_TimerUninitialize(GPT0_TIMER());
//    CLOGD("GPT timer repeat mode channel0, 32bit timer, test end\n");
}


void GPT_TIMER_Interrupt_single_Channel1_16bit(void)
{
	/*********GPT timer single mode channel1, 16bit timer********/
//    CLOGD("GPT timer single mode channel1, 16bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_16_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_64 |
    													 CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, GPT_CHANNEL1);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL1, GPT_Timer_Channel1_Event);
    HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL1, GPT_PERIOD_16BIT);
    HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL1);
    WaitgPassFlag(1);
    HAL_GPT_TimerUninitialize(GPT0_TIMER());
//    CLOGD("GPT timer single mode channel1, 16bit timer, test end\n");
}


void GPT_TIMER_Interrupt_single_Channel2_8bit(void)
{
	/*********GPT timer single mode channel2, 8bit timer********/
    CLOGD("GPT timer single mode channel2, 8bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_8_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_128 |
    													 CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, GPT_CHANNEL2);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL2, GPT_Timer_Channel2_Event);
    HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL2, GPT_PERIOD_8BIT);
    HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL2);
    WaitgPassFlag(1);
    HAL_GPT_TimerUninitialize(GPT0_TIMER());
    CLOGD("GPT timer single mode channel2, 8bit timer, test end\n");
}


//void GPT_TIMER_Interrupt_Repeat_Channel3_32bit(void)
//{
//	/*********GPT timer repeat mode channel3, 32bit timer********/
//    CLOGD("GPT timer repeat mode channel3, 32bit timer, test begin");
//    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
//    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
//    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_2 |
//													 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_UP, GPT_CHANNEL3);
//    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL3, GPT_Timer_Channel3_Event);
//	HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL3, GPT_PERIOD_32BIT);
//
//	HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL3);
//	WaitgPassFlag(1);
//	HAL_GPT_TimerUninitialize(GPT0_TIMER());
//    CLOGD("GPT timer repeat mode channel3, 32bit timer, test end\n");
//}


void GPT_TIMER_Interrupt_Repeat_Channel3_32bit(void)
{
	/*********GPT timer repeat mode channel3, 32bit timer********/

//    CLOGD("GPT timer repeat mode channel3, 32bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_2 |
													 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_UP, GPT_CHANNEL3);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL3, GPT_Timer_Channel3_Test_Event);
//	HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL3, GPT_PERIOD_32BIT);
	HAL_GPT_SetTimerPeriodDirectByMs(GPT0_TIMER(), GPT_CHANNEL3, 1);

	HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL3);

	while(1);
}


void GPT_TIMER_Interrupt_single_Channel4_16bit(void)
{
	/*********GPT timer single mode channel4, 16bit timer********/
    CLOGD("GPT timer single mode channel4, 16bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_16_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_64 |
    													 CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, GPT_CHANNEL4);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL4, GPT_Timer_Channel4_Event);
    HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL4, GPT_PERIOD_16BIT);
    HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL4);
    WaitgPassFlag(1);
    HAL_GPT_TimerUninitialize(GPT0_TIMER());
    CLOGD("GPT timer single mode channel4, 16bit timer, test end\n");
}


void GPT_TIMER_Interrupt_single_Channel5_8bit(void)
{
	/*********GPT timer single mode channel5, 8bit timer********/
    CLOGD("GPT timer single mode channel5, 8bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_8_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_128 |
    													 CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, GPT_CHANNEL5);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL5, GPT_Timer_Channel5_Event);
    HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL5, GPT_PERIOD_8BIT);
    HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL5);
    WaitgPassFlag(1);
    HAL_GPT_TimerUninitialize(GPT0_TIMER());
    CLOGD("GPT timer single mode channel5, 8bit timer, test end\n");
}


void GPT_TIMER_Interrupt_Repeat_Channel6_32bit(void)
{
	/*********GPT timer repeat mode channel6, 32bit timer********/
    CLOGD("GPT timer repeat mode channel6, 32bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_2 |
													 CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_UP, GPT_CHANNEL6);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL6, GPT_Timer_Channel6_Event);
	HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL6, GPT_PERIOD_32BIT);

	HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL6);
	WaitgPassFlag(1);
	HAL_GPT_TimerUninitialize(GPT0_TIMER());
    CLOGD("GPT timer repeat mode channel6, 32bit timer, test end\n");
}


void GPT_TIMER_Interrupt_single_Channel7_16bit(void)
{
	/*********GPT timer single mode channel7, 16bit timer********/
    CLOGD("GPT timer single mode channel7, 16bit timer, test begin");
    HAL_GPT_TimerInitialize(GPT0_TIMER(), NULL);
    HAL_GPT_TimerPowerControl(GPT0_TIMER(), CSK_POWER_FULL);
    HAL_GPT_TimerControl(GPT0_TIMER(), CSK_GPT_TIMER_16_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_64 |
    													 CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, GPT_CHANNEL7);
    HAL_GPT_RegisterTimerCallback(GPT0_TIMER(), GPT_CHANNEL7, GPT_Timer_Channel7_Event);
    HAL_GPT_SetTimerPeriodByCount(GPT0_TIMER(), GPT_CHANNEL7, GPT_PERIOD_16BIT);
    HAL_GPT_StartTimer(GPT0_TIMER(), GPT_CHANNEL7);
    WaitgPassFlag(1);
    HAL_GPT_TimerUninitialize(GPT0_TIMER());
    CLOGD("GPT timer single mode channel7, 16bit timer, test end\n");
}



static void GPIO_Init_Handler(void) {
	GPIOA_Handler = GPIOA();
}

static void GPIOA_PIN30_OUT(void) {

	GPIO_Init_Handler();


	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN30);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN30, CSK_GPIO_DIR_OUTPUT);

	while(1){
	GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN30, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN30, 0);
    SysTick_Delay_Ms(100);
	}


    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN30, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}


void GPT_PWM_Output_AllChannel(void)
{
	/*********GPT PWM output all channel********/
    CLOGD("GPT pwm output all channel Channel0-Channel7, test begin");

	uint32_t ret;

	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	uint32_t frequency = 1000;
	uint32_t duty = 50;
	for(uint8_t channel=0; channel<2; channel++){
		ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
					   CSK_GPT_PWM_CLKDIV_1 | CSK_GPT_PWM_OPERATION_MODE_PWM, channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), channel, frequency, duty);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_EnablePWM(GPT0_PWM(), channel);

		IOMuxManager_PinConfigure(gpt_pwm_pin_array[channel][0], gpt_pwm_pin_array[channel][1], gpt_pwm_pin_array[channel][2]);

		frequency += 1000;
		duty += 10;
	}

    CLOGD("GPT pwm output all channel Channel0-Channel7, test end\n");
}

void GPT_PWM_Output_Channel0(void)
{
	uint32_t ret;

	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
				   CSK_GPT_PWM_CLKDIV_1 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL0,1000, 50);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL0);

	IOMuxManager_PinConfigure(gpt_pwm_pin_array[GPT_CHANNEL0][0], gpt_pwm_pin_array[GPT_CHANNEL0][1], gpt_pwm_pin_array[GPT_CHANNEL0][2]);

}

void GPT_PWM_Output_Channel1(void)
{
	uint32_t ret;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
				  CSK_GPT_PWM_CLKDIV_2 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL1);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL1, 2000, 40);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL1);

	IOMuxManager_PinConfigure(PWM_CH1_PAD, PWM_CH1_PIN, PWM_CH1_SEL);

}

void GPT_PWM_Output_Channel2(void)
{
	uint32_t ret;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL2);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL2, 3000, 50);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL2);

	IOMuxManager_PinConfigure(PWM_CH2_PAD, PWM_CH2_PIN, PWM_CH2_SEL);

}


void GPT_PWM_Output_Channel3(void)
{
	uint32_t ret;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_8 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL3);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL3, 4000, 60);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL3);

	IOMuxManager_PinConfigure(PWM_CH3_PAD, PWM_CH3_PIN, PWM_CH3_SEL);

}


void GPT_PWM_Output_Channel4(void)
{
	uint32_t ret;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_16 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL4);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL4, 5000, 70);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL4);

	IOMuxManager_PinConfigure(PWM_CH4_PAD, PWM_CH4_PIN, PWM_CH4_SEL);

}


void GPT_PWM_Output_Channel6(void)
{
	uint32_t ret;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_32 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL6);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL6,6000, 80);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL6);

	IOMuxManager_PinConfigure(PWM_CH6_PAD, PWM_CH6_PIN, PWM_CH6_SEL);
}



void GPT_LEDC_Polling_Output(void)
{
	CLOGD("GPT ledc polling mode test begin");
	uint32_t ret;
	uint32_t ledcTmpDatA[10] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
	uint32_t ledcTmpDatB[10] = {0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x00FF0000, 0x000000FF, 0x00FF0000, 0xFF000000};
	uint32_t ledcTmpDat[10] = {0};

	HAL_GPT_PWMUninitialize(GPT0_PWM());

	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_2 | CSK_GPT_PWM_OPERATION_MODE_LEDC | CSK_GPT_LEDC_TRANSFER_MODE_POLLING, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(LEDC1_OUT_PAD, LEDC1_OUT_PIN, LEDC1_OUT_SEL);

		for(uint32_t i=0; i<10; i++){
			memcpy(ledcTmpDat, ledcTmpDatA, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
			HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL0, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
			SysTick_Delay_Ms(50);
			memcpy(ledcTmpDat, ledcTmpDatB, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
			HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL0, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
			SysTick_Delay_Ms(50);
		}


	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
									   CSK_GPT_PWM_CLKDIV_2 | CSK_GPT_PWM_OPERATION_MODE_LEDC | CSK_GPT_LEDC_TRANSFER_MODE_POLLING, GPT_CHANNEL1);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(LEDC2_OUT_PAD, LEDC2_OUT_PIN, LEDC2_OUT_SEL);

		for(uint32_t i=0; i<10; i++){
			memcpy(ledcTmpDat, ledcTmpDatA, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
			HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL1, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
			SysTick_Delay_Ms(50);
			memcpy(ledcTmpDat, ledcTmpDatB, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
			HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL1, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
			SysTick_Delay_Ms(50);
		}


	CLOGD("GPT ledc polling mode test end");
}

void GPT_LEDC_Interrupt_Output(void)
{
	CLOGD("GPT ledc interrupt mode test begin");
	uint32_t ret;
	uint32_t ledcTmpDatA[10] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
	uint32_t ledcTmpDatB[10] = {0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x00FF0000, 0x000000FF, 0x00FF0000, 0xFF000000};
	uint32_t ledcTmpDat[10] = {0};

	HAL_GPT_PWMUninitialize(GPT0_PWM());
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_2 | CSK_GPT_PWM_OPERATION_MODE_LEDC | CSK_GPT_LEDC_TRANSFER_MODE_INTERRUPT, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterLEDCCallback(GPT0_PWM(), GPT_CHANNEL0, GPT_Ledc_Channel0_Sendout_Event);

	IOMuxManager_PinConfigure(LEDC1_OUT_PAD, LEDC1_OUT_PIN, LEDC1_OUT_SEL);

	for(uint32_t i=0; i<10; i++){
		memcpy(ledcTmpDat, ledcTmpDatA, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
		ret = HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL0, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);
		WaitgPassFlag(1);
		SysTick_Delay_Ms(500);
		memcpy(ledcTmpDat, ledcTmpDatB, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
		ret = HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL0, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);
		WaitgPassFlag(1);
		SysTick_Delay_Ms(500);
	}
	CLOGD("GPT ledc interrupt mode test end");
}

void GPT_LEDC_Dma_Output(void)
{
//	CLOGD("GPT ledc dma mode test begin");
	uint32_t ret;
	uint32_t ledcTmpDatA[10] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
	uint32_t ledcTmpDatB[10] = {0x0000FF00, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FF0000, 0x00FF0000, 0x000000FF, 0x00FF0000, 0xFF000000};
	uint32_t ledcTmpDat[10] = {0};

	HAL_GPT_PWMUninitialize(GPT0_PWM());
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_XTAL | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_2 | CSK_GPT_PWM_OPERATION_MODE_LEDC | CSK_GPT_LEDC_TRANSFER_MODE_DMA, GPT_CHANNEL1);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterLEDCCallback(GPT0_PWM(), GPT_CHANNEL1, GPT_Ledc_Channel1_Sendout_Event);

	IOMuxManager_PinConfigure(LEDC2_OUT_PAD, LEDC2_OUT_PIN, LEDC2_OUT_SEL);

	for(uint32_t i=0; i<10; i++){
		IOMuxManager_PinConfigure(LEDC2_OUT_PAD, LEDC2_OUT_PIN, LEDC2_OUT_SEL);
		memcpy(ledcTmpDat, ledcTmpDatA, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
		ret = HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL1, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);
		WaitgPassFlag(1);
		SysTick_Delay_Ms(500);
		IOMuxManager_PinConfigure(LEDC2_OUT_PAD, LEDC2_OUT_PIN, LEDC2_OUT_SEL);
		memcpy(ledcTmpDat, ledcTmpDatB, 4*sizeof(ledcTmpDat)/sizeof(uint32_t));
		ret = HAL_GPT_LEDCOut(GPT0_PWM(), GPT_CHANNEL1, ledcTmpDat, sizeof(ledcTmpDat)/sizeof(uint32_t));
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);
		WaitgPassFlag(1);
		SysTick_Delay_Ms(500);
	}
//	CLOGD("GPT ledc dma mode test end");
}



/*FPGA board connection
 * PA30 PWM0 ---- PA4 TRIGGER 2
 * */

void GPT_InputCapture_Polling_AllChannel(void)
{
	CLOGD("GPT input capture polling mode all channel test begin");
	uint32_t ret;
	uint8_t pwm_channel, ic_channel;
	uint32_t frequency = 2000;

	HAL_GPT_PWMUninitialize(GPT0_PWM());
//	for(uint8_t cnt = 0; cnt < 8; cnt++){
	for(uint8_t cnt = 0; cnt < 1; cnt++){
		pwm_channel = pwm_ic_channel_connect[cnt][0];
		ic_channel = pwm_ic_channel_connect[cnt][1];

		/*generate pwm wave*/
		ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							CSK_GPT_PWM_CLKDIV_1 | CSK_GPT_PWM_OPERATION_MODE_PWM, pwm_channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), pwm_channel, frequency, 50);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		IOMuxManager_PinConfigure(gpt_pwm_pin_array[pwm_channel][0], gpt_pwm_pin_array[pwm_channel][1], gpt_pwm_pin_array[pwm_channel][2]);
		HAL_GPT_EnablePWM(GPT0_PWM(), pwm_channel);

		/*set input capture mode*/
		ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
								 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_POLLING, ic_channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_RegisterIcCallback(GPT0_IC(), ic_channel, input_channel_info[ic_channel].cb);
		IOMuxManager_PinConfigure(input_channel_info[ic_channel].pad.pad_group, input_channel_info[ic_channel].pad.pad_num, input_channel_info[ic_channel].pad.pad_func);

		uint32_t ulCaptureDat[9] = {0};
		ret = HAL_GPT_GetIcData(GPT0_IC(), ic_channel, ulCaptureDat, 10);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		uint32_t ulCapDatSum = 0;
		for(uint32_t i=1; i<9; i++){
			ulCapDatSum += ulCaptureDat[i];
		}
		uint32_t ulCapDatAve = ulCapDatSum >> 3;
		CLOGD("capture data= 0x%x", ulCapDatAve);
//		CLOGD("frequency = %dHz", (BOARD_BOOTCLOCKRUN_AP_PCLK_CLK)/ulCapDatAve);

		frequency += 1000;
	}

	CLOGD("GPT input capture polling mode test end\n");
}


void GPT_InputCapture_Polling_Channel2(void)
{
	uint32_t ret;
	CLOGD("GPT input capture channel2 polling mode test begin");
	/*generate pwm wave*/
	ret =HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL0, 4000, 30);
	IOMuxManager_PinConfigure(PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL0);

	/*set input capture mode*/
	HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);

	HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_POLLING, GPT_CHANNEL2);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL2, GPT_Ic_Channel2_Event);
	IOMuxManager_PinConfigure(IC_CH2_PAD, IC_CH2_PIN, IC_CH2_SEL);

	uint32_t ucCaptureDat[10] = {0};
	HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL2, ucCaptureDat, 10);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel2 polling mode test end");
}

/*FPGA board connection
 * PA3 PWM3 ---- PA4 TRIGGER 2
 * */
void GPT_InputCapture_Interrupt_AllChannel(void)
{
	CLOGD("GPT input capture interrupt mode all channel test begin");
	uint32_t ret;
	uint8_t pwm_channel, ic_channel;
	uint32_t frequency = 1000;

	HAL_GPT_PWMUninitialize(GPT0_PWM());
//	for(uint8_t cnt = 0; cnt < 8; cnt++){
	for(uint8_t cnt = 0; cnt < 1; cnt++){
		pwm_channel = pwm_ic_channel_connect[cnt][0];
		ic_channel = pwm_ic_channel_connect[cnt][1];

		/*generate pwm wave*/
		ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_1 | CSK_GPT_PWM_OPERATION_MODE_PWM, pwm_channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), pwm_channel, frequency, 50);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		IOMuxManager_PinConfigure(gpt_pwm_pin_array[pwm_channel][0], gpt_pwm_pin_array[pwm_channel][1], gpt_pwm_pin_array[pwm_channel][2]);
		HAL_GPT_EnablePWM(GPT0_PWM(), pwm_channel);

		/*set input capture mode*/
		ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
								 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_INTERRUPT, ic_channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_RegisterIcCallback(GPT0_IC(), ic_channel, input_channel_info[ic_channel].cb);
		IOMuxManager_PinConfigure(input_channel_info[ic_channel].pad.pad_group, input_channel_info[ic_channel].pad.pad_num, input_channel_info[ic_channel].pad.pad_func);

		uint32_t ulCaptureDat[9] = {0};
		ret = HAL_GPT_GetIcData(GPT0_IC(), ic_channel, ulCaptureDat, 10);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);
		WaitgPassFlag(1);
		uint32_t ulCapDatSum = 0;
		for(uint32_t i=1; i<9; i++){
			ulCapDatSum += ulCaptureDat[i];
		}
		uint32_t ulCapDatAve = ulCapDatSum >> 3;
		CLOGD("capture data= 0x%x", ulCapDatAve);
//		CLOGD("frequency = %dHz", (BOARD_BOOTCLOCKRUN_AP_PCLK_CLK)/ulCapDatAve);

		frequency += 1000;
	}

	CLOGD("GPT input capture interrupt mode test end\n");
}



void GPT_InputCapture_Interrupt_Channel2(void)
{
	CLOGD("GPT input capture interrupt mode test begin");
	uint32_t ret;

	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL0, 3000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL0);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_INTERRUPT, GPT_CHANNEL2);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL2, GPT_Ic_Channel2_Event);
	IOMuxManager_PinConfigure(IC_CH2_PAD, IC_CH2_PIN, IC_CH2_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL2, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture interrupt mode test end");
}


/*FPGA board connection
 * PA3 PWM3 ---- PA4 TRIGGER 2
 * */
void GPT_InputCapture_Dma_AllChannel(void)
{
	CLOGD("GPT input capture dma mode all channel test begin");
	uint32_t ret;
	uint8_t pwm_channel, ic_channel;

	HAL_GPT_PWMUninitialize(GPT0_PWM());

	uint32_t frequency = 4000;
//	for(uint8_t cnt = 0; cnt < 4; cnt++){
	for(uint8_t cnt = 0; cnt < 1; cnt++){
		pwm_channel = pwm_ic_channel_connect[cnt][0];
		ic_channel = pwm_ic_channel_connect[cnt][1];

		/*generate pwm wave*/
		ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
							   CSK_GPT_PWM_CLKDIV_1 | CSK_GPT_PWM_OPERATION_MODE_PWM, pwm_channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), pwm_channel, frequency, 50);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		IOMuxManager_PinConfigure(gpt_pwm_pin_array[pwm_channel][0], gpt_pwm_pin_array[pwm_channel][1], gpt_pwm_pin_array[pwm_channel][2]);
		HAL_GPT_EnablePWM(GPT0_PWM(), pwm_channel);

		/*set input capture mode*/
		ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
								 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_DMA, ic_channel);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);

		HAL_GPT_RegisterIcCallback(GPT0_IC(), ic_channel, input_channel_info[ic_channel].cb);
		IOMuxManager_PinConfigure(input_channel_info[ic_channel].pad.pad_group, input_channel_info[ic_channel].pad.pad_num, input_channel_info[ic_channel].pad.pad_func);

		uint32_t ulCaptureDat[10] = {0};
		ret = HAL_GPT_GetIcData(GPT0_IC(), ic_channel, ulCaptureDat, 10);
		if(ret != CSK_DRIVER_OK)
			CLOGD("Error = %d", ret);
		WaitgPassFlag(1);

		uint32_t ulCapDatSum = 0;
		for(uint32_t i=1; i<9; i++){
			ulCapDatSum += ulCaptureDat[i];
		}
		uint32_t ulCapDatAve = ulCapDatSum >> 3;
		CLOGD("capture data= 0x%x", ulCapDatAve);
//		CLOGD("frequency = %dHz", (BOARD_BOOTCLOCKRUN_AP_PCLK_CLK)/ulCapDatAve);

		frequency += 1000;
	}

	CLOGD("GPT input capture dma mode test end\n");
}


/*PWM_CH7(PB15 "fpga connector KC6") -- IC_CH0(PA24 "fpga connector PC4")*/
/*connection only 4 channels valid
 * PA16 ---- PB7
 * PA17 ---- PB6
 * PA18 ---- PB5
 * PA19 ---- PB4
 * PA04 ---- PB3
 * PA05 ---- PB2
 * PA06 ---- PB1
 * PA07 ---- PB8
 * */
void GPT_InputCapture_Dma_Channel0(void)
{
	CLOGD("GPT input capture channel0 dma mode test begin");
	uint32_t ret;

	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL7);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL7, 2000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(PWM_CH7_PAD, PWM_CH7_PIN, PWM_CH7_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL7);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_DMA, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL0, GPT_Ic_Channel0_Event);
	IOMuxManager_PinConfigure(IC_CH0_PAD, IC_CH0_PIN, IC_CH0_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL0, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel0 dma mode test end");
}


/*PWM_CH7(PB15 "fpga connector KC6") -- IC_CH1(PA25 "fpga connector PC5")*/
void GPT_InputCapture_Dma_Channel1(void)
{
	CLOGD("GPT input capture channel1 dma mode test begin");
	uint32_t ret;
	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL0, 2000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL0);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_DMA, GPT_CHANNEL1);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL1, GPT_Ic_Channel1_Event);
	IOMuxManager_PinConfigure(IC_CH1_PAD, IC_CH1_PIN, IC_CH1_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL1, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel1 dma mode test end");
}

/*PWM_CH0(PA2 "fpga connector KC0") -- IC_CH2(PA26 "fpga connector LC0")*/
void GPT_InputCapture_Dma_Channel2(void)
{
	CLOGD("GPT input capture channel2 dma mode test begin");
	uint32_t ret;
	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL0);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL0, 2000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	IOMuxManager_PinConfigure(PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL0);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_DMA, GPT_CHANNEL2);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL2, GPT_Ic_Channel2_Event);
	IOMuxManager_PinConfigure(IC_CH2_PAD, IC_CH2_PIN, IC_CH2_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL2, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel2 dma mode test end");
}


/*PWM_CH1(PA8 "fpga connector KC1") -- IC_CH3(PA27 "fpga connector LC1")*/
void GPT_InputCapture_Dma_Channel3(void)
{
	CLOGD("GPT input capture channel3 dma mode test begin");
	uint32_t ret;
	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL1);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL1, 2000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	IOMuxManager_PinConfigure(PWM_CH1_PAD, PWM_CH1_PIN, PWM_CH1_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL1);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_DMA, GPT_CHANNEL3);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL3, GPT_Ic_Channel3_Event);
	IOMuxManager_PinConfigure(IC_CH3_PAD, IC_CH3_PIN, IC_CH3_SEL);

	uint32_t ucCaptureDat[10] = {0};
	HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL3, ucCaptureDat, 10);
	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel3 dma mode test end");
}


/*PWM_CH2(PA9 "fpga connector KC2") -- IC_CH4(PA28 "fpga connector LC2")*/
void GPT_InputCapture_Interrupt_Channel4(void)
{
	CLOGD("GPT input capture channel4 interrupt mode test begin");
	uint32_t ret;

	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL2);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL2, 2000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(PWM_CH2_PAD, PWM_CH2_PIN, PWM_CH2_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL2);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_INTERRUPT, GPT_CHANNEL4);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL4, GPT_Ic_Channel4_Event);
	IOMuxManager_PinConfigure(IC_CH4_PAD, IC_CH4_PIN, IC_CH4_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL4, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel4 interrupt mode test end");
}

/*PWM_CH3(PA10 "fpga connector KC3") -- IC_CH5(PA29 "fpga connector LC3")*/
void GPT_InputCapture_Interrupt_Channel5(void)
{
	CLOGD("GPT input capture channel5 interrupt mode test begin");
	uint32_t ret;
	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL3);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL3, 2000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(PWM_CH3_PAD, PWM_CH3_PIN, PWM_CH3_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL3);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_INTERRUPT, GPT_CHANNEL5);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL5, GPT_Ic_Channel5_Event);
	IOMuxManager_PinConfigure(IC_CH5_PAD, IC_CH5_PIN, IC_CH5_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL5, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel5 interrupt mode test end");
}


/*PWM_CH4(PB13 "fpga connector KC4") -- IC_CH6(PA30 "fpga connector LC4")*/
void GPT_InputCapture_Interrupt_Channel6(void)
{
	CLOGD("GPT input capture channel6 interrupt mode test begin");
	uint32_t ret;
	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL4);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL4, 3000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	IOMuxManager_PinConfigure(PWM_CH4_PAD, PWM_CH4_PIN, PWM_CH4_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL4);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_INTERRUPT, GPT_CHANNEL6);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL6, GPT_Ic_Channel6_Event);
	IOMuxManager_PinConfigure(IC_CH6_PAD, IC_CH6_PIN, IC_CH6_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL6, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel6 interrupt mode test end");
}

/*PWM_CH6(PB14 "fpga connector KC5") -- IC_CH7(PA31 "fpga connector LC5")*/
void GPT_InputCapture_Interrupt_Channel7(void)
{
	CLOGD("GPT input capture channel7 interrupt mode test begin");
	uint32_t ret;

	/*generate pwm wave*/
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
						   CSK_GPT_PWM_CLKDIV_4 | CSK_GPT_PWM_OPERATION_MODE_PWM, GPT_CHANNEL6);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), GPT_CHANNEL6, 4000, 30);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);
	IOMuxManager_PinConfigure(PWM_CH6_PAD, PWM_CH6_PIN, PWM_CH6_SEL);
	HAL_GPT_EnablePWM(GPT0_PWM(), GPT_CHANNEL6);

	/*set input capture mode*/
	ret = HAL_GPT_IcInitialize(GPT0_IC(), NULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcPowerControl(GPT0_IC(), CSK_POWER_FULL);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	ret = HAL_GPT_IcControl(GPT0_IC(), CSK_GPT_IC_TIME_MODE | CSK_GPT_IC_CLKSRC_PCLK | CSK_GPT_IC_TRIGGER_EXT |
							 CSK_GPT_IC_EDGE_RISING | CSK_GPT_IC_FILTER_1 | CSK_GPT_IC_TRIGGER_RESET | CSK_GPT_IC_TRANSFER_MODE_INTERRUPT, GPT_CHANNEL7);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	HAL_GPT_RegisterIcCallback(GPT0_IC(), GPT_CHANNEL7, GPT_Ic_Channel7_Event);
	IOMuxManager_PinConfigure(IC_CH7_PAD, IC_CH7_PIN, IC_CH7_SEL);

	uint32_t ucCaptureDat[10] = {0};
	ret = HAL_GPT_GetIcData(GPT0_IC(), GPT_CHANNEL7, ucCaptureDat, 10);
	if(ret != CSK_DRIVER_OK)
		CLOGD("Error = %d", ret);

	WaitgPassFlag(1);
	for(uint32_t i=0; i<10; i++){
		CLOGD("capture data= 0x%x", ucCaptureDat[i]);
		CLOGD("frequency = %d", 48000000/ucCaptureDat[i]);
	}
	CLOGD("GPT input capture channel7 interrupt mode test end");
}



static void GPIOA_PIN2_OUT(void){
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, NULL, NULL);

    GPIO_Control(GPIOA_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN2);

    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN2, CSK_GPIO_DIR_OUTPUT);
}


/**
  * @brief description
  * @param None
  * @retval None
  */
__attribute__((section(".sramcode"))) void GPT_PWM_MotoTest(void)
{
    //Channel0 & Channel1 & Channel2 for PWM output
    //GPIOA_02 for test

    GPIO_Init_Handler();
    GPIOA_PIN2_OUT();

    uint32_t ret;
    HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

    ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
    if(ret != CSK_DRIVER_OK)
        CLOGD("Error = %d", ret);

    uint32_t duty = 60;
    uint8_t channel=0;
    //set all channel pwm duty, but don't load shadow register
    for(channel=0; channel<2; channel++){
            ret = HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK | CSK_GPT_PWM_OUTMODE_CENTRAL_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
                    CSK_GPT_PWM_CLKDIV_32 | CSK_GPT_PWM_OPERATION_MODE_PWM, channel);
            if(ret != CSK_DRIVER_OK)
                CLOGD("Error = %d", ret);

            ret = HAL_GPT_SetPWMFreqDuty_NoShadow(GPT0_PWM(), channel, 8000, duty);
            if(ret != CSK_DRIVER_OK)
                CLOGD("Error = %d", ret);

            IOMuxManager_PinConfigure(gpt_pwm_pin_array[channel][0], gpt_pwm_pin_array[channel][1], gpt_pwm_pin_array[channel][2]);
            duty += 10;
      }
      //shadow load to make duty cycle setting valid
      HAL_GPT_ShadowLoad(GPT0_PWM(), (GPT_CHANNEL0_SYNC | GPT_CHANNEL1_SYNC | GPT_CHANNEL2_SYNC));

      //enable pwm channel one by one, don't start conter, later counter will be start simultaneously
      HAL_GPT_EnablePWM_NoCntStart(GPT0_PWM(), GPT_CHANNEL0);
      HAL_GPT_EnablePWM_NoCntStart(GPT0_PWM(), GPT_CHANNEL1);
      HAL_GPT_EnablePWM_NoCntStart(GPT0_PWM(), GPT_CHANNEL2);

      //counter start
      HAL_GPT_CntStart(GPT0_PWM(), (GPT_CHANNEL0_SYNC | GPT_CHANNEL1_SYNC | GPT_CHANNEL2_SYNC));

      //start timer for test
      GPT_TIMER_Interrupt_Repeat_Channel3_32bit();
}



