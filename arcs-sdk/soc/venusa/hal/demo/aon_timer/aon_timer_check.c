/*
 * aon_timer_check.c
 *
 *  Created on: 2025年5月15日
 *      Author: USER
 */

 #include <string.h>
 #include <assert.h>
 #include <string.h>
 #include <stdbool.h>

#include "log_print.h"
#include "Driver_AON_TIMER.h"
//#include "Driver_AON_WDT.h"
#include "ClockManager.h"
#include "PowerManager.h"
#include "systick.h"

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 1000000){\
            break;\
        }\
    }\
    }while(0)


static void *AON_TIMER_Handler = NULL;

static void AON_TIMER_Init_Handler(void) {
    AON_TIMER_Handler = AON_TIMER();
}

static void aon_timer_test_normal_mode_rc32k_interrupt_EventCallback(uint32_t event, void* workspace){
    CLOGD("Aon timer trigger");

    uint32_t counter;

    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);

    CLOGD("Current counter -> 0x%x", counter);
}

static void aon_timer_test_normal_mode_rc32k_interrupt(void) {
	AON_TIMER_Initialize(AON_TIMER_Handler, aon_timer_test_normal_mode_rc32k_interrupt_EventCallback, NULL);

	AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);

	AON_TIMER_Control(AON_TIMER_Handler, HAL_AON_TIMER_MODE_Normal | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32K);
    
	AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, 32000);

	AON_TIMER_StartTimer(AON_TIMER_Handler);

    FAKE_WHILE();

    AON_TIMER_StopTimer(AON_TIMER_Handler);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);

    AON_TIMER_Uninitialize(AON_TIMER_Handler);
}

static uint32_t g_total_period_count = 32000;

static void aon_timer_test_wrap_mode_rc32k_interrupt_EventCallback(uint32_t event, void* workspace){
    CLOGD("Aon timer trigger");

    uint32_t counter;
    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);

    g_total_period_count += 16000;

    AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, g_total_period_count);

    CLOGD("Current counter = 0x%x, new period = %u", counter, g_total_period_count);
}


static void aon_timer_test_wrap_mode_rc32k_interrupt(void) {
    AON_TIMER_Initialize(AON_TIMER_Handler, aon_timer_test_wrap_mode_rc32k_interrupt_EventCallback, NULL);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);

    AON_TIMER_Control(AON_TIMER_Handler, HAL_AON_TIMER_MODE_Wrapping | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32K);

    AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, 32000);

    AON_TIMER_StartTimer(AON_TIMER_Handler);

    FAKE_WHILE();

    AON_TIMER_StopTimer(AON_TIMER_Handler);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);

    AON_TIMER_Uninitialize(AON_TIMER_Handler);
}


static void aon_timer_test_repeat_mode_rc32k_interrupt_EventCallback(uint32_t event, void* workspace){
    CLOGD("Aon timer rc32k_interrupt trigger");

    uint32_t counter;

    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);

    CLOGD("Current counter -> 0x%x", counter);
}


static void aon_timer_test_repeat_mode_rc32k_interrupt(void) {
    AON_TIMER_Initialize(AON_TIMER_Handler, aon_timer_test_repeat_mode_rc32k_interrupt_EventCallback, NULL);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);

    AON_TIMER_Control(AON_TIMER_Handler, HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32K);

    AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, 32768);

    AON_TIMER_StartTimer(AON_TIMER_Handler);

    SysTick_Delay_Ms(1);

    uint32_t cur_value = 0;
    for(uint32_t i = 0; i < 5; i ++) {
    	AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &cur_value);
    	CLOGD("Current counter -> 0x%x", cur_value);
    	SysTick_Delay_Ms(100);
    }

    FAKE_WHILE();

    AON_TIMER_StopTimer(AON_TIMER_Handler);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);

    AON_TIMER_Uninitialize(AON_TIMER_Handler);
}

static void aon_timer_test_repeat_mode_xo24m_div32k_interrupt_EventCallback(uint32_t event, void* workspace) {
    CLOGD("Aon timer xo24m_div32k_interrupt trigger");

    uint32_t counter;

    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);

    CLOGD("Current counter -> 0x%x", counter);
}

static void aon_timer_test_repeat_mode_xo24m_div32k_interrupt(void) {
	AON_TIMER_Initialize(AON_TIMER_Handler, aon_timer_test_repeat_mode_xo24m_div32k_interrupt_EventCallback, NULL);

	AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);

	AON_TIMER_Control(AON_TIMER_Handler, HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_XO24M_Div32K);

	AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, 32768);

	AON_TIMER_StartTimer(AON_TIMER_Handler);

	SysTick_Delay_Ms(1);

    uint32_t cur_value = 0;
    for(uint32_t i = 0; i < 5; i ++) {
    	AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &cur_value);
    	CLOGD("Current counter -> 0x%x", cur_value);
    	SysTick_Delay_Ms(100);
    }

    FAKE_WHILE();

    AON_TIMER_StopTimer(AON_TIMER_Handler);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);

    AON_TIMER_Uninitialize(AON_TIMER_Handler);
}

static void aon_timer_test_repeat_mode_rc24m_div32k_interrupt_EventCallback(uint32_t event, void* workspace) {
    CLOGD("Aon timer rc24m_div32k_interrupt trigger");

    uint32_t counter;

    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);

    CLOGD("Current counter -> 0x%x", counter);
}

static void aon_timer_test_repeat_mode_rc24m_div32k_interrupt(void) {
	AON_TIMER_Initialize(AON_TIMER_Handler, aon_timer_test_repeat_mode_rc24m_div32k_interrupt_EventCallback, NULL);

	AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);

	AON_TIMER_Control(AON_TIMER_Handler, HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_RC24M_Div32K);

	AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, 32768);

	AON_TIMER_StartTimer(AON_TIMER_Handler);

	SysTick_Delay_Ms(1);

    uint32_t cur_value = 0;
    for(uint32_t i = 0; i < 5; i ++) {
    	AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &cur_value);
    	CLOGD("Current counter -> 0x%x", cur_value);
    	SysTick_Delay_Ms(100);
    }

    FAKE_WHILE();

    AON_TIMER_StopTimer(AON_TIMER_Handler);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);

    AON_TIMER_Uninitialize(AON_TIMER_Handler);
}

typedef struct 
{
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
//	 {aon_timer_test_normal_mode_rc32k_interrupt, "aon_timer_test_normal_mode_rc32k_interrupt"},
//	 {aon_timer_test_wrap_mode_rc32k_interrupt, "aon_timer_test_wrap_mode_rc32k_interrupt"},
//   {aon_timer_test_repeat_mode_rc32k_interrupt, "aon_timer_test_repeat_mode_rc32k_interrupt"},
//	 {aon_timer_test_repeat_mode_xo24m_div32k_interrupt, "aon_timer_test_repeat_mode_xo24m_div32k_interrupt"},
	 {aon_timer_test_repeat_mode_rc24m_div32k_interrupt, "aon_timer_test_repeat_mode_rc24m_div32k_interrupt"},
};

int main(void) {
	logInit(0, 115200);
	CLOGD("enter main: aon timer test\r\n");
	uint32_t index;

	AON_TIMER_Init_Handler();

	for (index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++)
	{
		test_case_t item = test_array[index];
		CLOGD("test case: %s", item.name);
    	item.function();
	}

	while (1);

	return 0;
}
