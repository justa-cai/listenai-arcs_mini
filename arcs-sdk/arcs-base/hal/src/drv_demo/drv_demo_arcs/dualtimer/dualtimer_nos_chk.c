/*
 * timer_nos_chk.c
 *
 *  Created on: 2020年8月24日
 *      Author: USER
 */

#include "Driver_DUAL_TIMER.h"
#include "log_print.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "chip.h"

#include "unity.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

typedef void (*function)(void);

static void DUAL_TIMER_Interrupt_One_shot(void);
static void DUAL_TIMER_Interrupt_Free_running(void);
static void DUAL_TIMER_Interrupt_Periodic(void);
static void DUAL_TIMER_Interrupt_Free_running_In_div16(void);
static void DUAL_TIMER_Interrupt_Free_running_In_div256(void);
static void DUAL_TIMER_Interrupt_Free_running_In_div1_32bit(void);


/*
 * dual timer handler
 * */
static void* DUAL_TIMER_Handler = NULL;

static void DUAL_TIMER_Init_Handler(){
    DUAL_TIMER_Handler = DUALTIMERS1();
}

/*Global control macro*/
#define DUAL_TIMER_TEST_CHANNEL   (CSK_TIMER_CHANNEL_1)
#define DUAL_TIMER_RELOAD_COUNT   (16000)

uint32_t intFlag=0;

/*Callback*/
static void DUAL_TIMER_EventCallback(uint32_t event, void* workspace){
    CLOGD("Trigger dual timer interrupt: %d", event);
    intFlag = 1;
}

static void DUAL_TIMER_Interrupt_One_shot(void){
    DUALTIMERS_Initialize(DUAL_TIMER_Handler);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

    DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_OneShot | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

    DUALTIMERS_SetTimerPeriodByCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_RELOAD_COUNT);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    uint32_t count = 0xffff;

    while(count != 0){
    	DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);
    }

    while(intFlag == 0);
    intFlag = 0;

	CLOGD("%s dualtimer channel: %d PASS", __func__, DUAL_TIMER_TEST_CHANNEL);
}

static void DUAL_TIMER_Interrupt_Free_running(void){
	DUALTIMERS_Initialize(DUAL_TIMER_Handler);

	DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

	DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_16Bit | \
                                    CSK_TIMER_MODE_FreeRunning | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

	DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

	DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    while(intFlag == 0);
    intFlag = 0;

    uint32_t count = 0;

    DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);

    TEST_ASSERT_NOT_EQUAL_UINT16(0, count);

    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);
}

static void DUAL_TIMER_Interrupt_Periodic(void){
	DUALTIMERS_Initialize(DUAL_TIMER_Handler);

	DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

	DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_Periodic | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

	DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

	DUALTIMERS_SetTimerPeriodByCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_RELOAD_COUNT);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    while(intFlag == 0);
    intFlag = 0;

    uint32_t count = 0;

    DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);

    TEST_ASSERT_NOT_EQUAL_UINT16(0, count);

    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);
}

static void DUAL_TIMER_Interrupt_Free_running_In_div16(void){
	DUALTIMERS_Initialize(DUAL_TIMER_Handler);

	DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

	DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_16 | \
                                    CSK_TIMER_SIZE_16Bit | \
                                    CSK_TIMER_MODE_FreeRunning | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

	DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    while(intFlag == 0);
    intFlag = 0;

//    uint32_t count = 0;
//
//    TIMER_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);
//
//    TEST_ASSERT_EQUAL_UINT16(0, count);

    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);
}

static void DUAL_TIMER_Interrupt_Free_running_In_div256(void){
	DUALTIMERS_Initialize(DUAL_TIMER_Handler);

	DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

	DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_256 | \
                                    CSK_TIMER_SIZE_16Bit | \
                                    CSK_TIMER_MODE_FreeRunning | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

	DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    while(intFlag == 0);
    intFlag = 0;

    uint32_t count = 0;

    DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);

    CLOGD("The reside count = %d", count);

    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);
}

static void DUAL_TIMER_Interrupt_Free_running_In_div1_32bit(void){
	DUALTIMERS_Initialize(DUAL_TIMER_Handler);

	DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

	DUALTIMERS_Control(DUAL_TIMER_Handler, CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_FreeRunning | \
                                    CSK_TIMER_INTERRUPT_Enabled, DUAL_TIMER_TEST_CHANNEL);

	DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    while(intFlag == 0);
    intFlag = 0;

    uint32_t count = 0;

    DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);

    TEST_ASSERT_NOT_EQUAL_UINT16(0, count);

    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);
}


/* Private functions ---------------------------------------------------------*/
void setUp(void) {
    // test configure
}

void tearDown(void) {
    // test clear
}

int main(){
    logInit(0, 115200);
    enable_GINT();

    DUAL_TIMER_Init_Handler();

    UNITY_BEGIN();

    RUN_TEST(DUAL_TIMER_Interrupt_One_shot);
    RUN_TEST(DUAL_TIMER_Interrupt_Free_running);
    RUN_TEST(DUAL_TIMER_Interrupt_Periodic);
    RUN_TEST(DUAL_TIMER_Interrupt_Free_running_In_div1_32bit);
    RUN_TEST(DUAL_TIMER_Interrupt_Free_running_In_div16);

    return UNITY_END();
}



