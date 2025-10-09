#include "Driver_DUAL_TIMER.h"
#include "chip.h"
#include <stdio.h>

static void DUAL_TIMER_Interrupt_Periodic(void);

/*
 * dual timer handler
 * */
static void *DUAL_TIMER_Handler = NULL;

static void DUAL_TIMER_Init_Handler()
{
    DUAL_TIMER_Handler = DUALTIMERS1();
}

/*Global control macro*/
#define DUAL_TIMER_TEST_CHANNEL (CSK_TIMER_CHANNEL_1)
#define DUAL_TIMER_RELOAD_COUNT (16000) // 设置定时器重载值为16000

volatile uint32_t intFlag = 0;
/*Callback*/
static void DUAL_TIMER_EventCallback(uint32_t event, void *workspace)
{
    printf("Trigger dual timer interrupt: %d\n", event);
    intFlag = 1;
}

static void DUAL_TIMER_Interrupt_Periodic(void)
{
    printf("DUAL_TIMER_Interrupt_Periodic begin\n");

    DUALTIMERS_Initialize(DUAL_TIMER_Handler);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);

    DUALTIMERS_Control(DUAL_TIMER_Handler,
                       CSK_TIMER_PRESCALE_Divide_1 | CSK_TIMER_SIZE_32Bit | CSK_TIMER_MODE_Periodic |
                       CSK_TIMER_INTERRUPT_Enabled,
                       DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_EventCallback, NULL);

    DUALTIMERS_SetTimerPeriodByCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, DUAL_TIMER_RELOAD_COUNT);

    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    while (intFlag == 0);

    intFlag = 0;

    uint32_t count = 0;

    DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);

    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);

    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);

    printf("DUAL_TIMER_Interrupt_Periodic end");
}

int main(int argc, char **argv)
{
    printf("Hello, world! dualtimer\n");

    DUAL_TIMER_Init_Handler();

    // 周期性定时器中断模式（按设定周期重复触发中断）
    DUAL_TIMER_Interrupt_Periodic();

    return 0;
}
