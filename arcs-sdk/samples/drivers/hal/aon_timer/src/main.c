#include "Driver_AON_TIMER.h"
#include <stdio.h>

volatile int aonTimerTriggered = 0;

#define AON_TIMER_RELOAD_COUNT (32000) // 设置定时器重载值为32000

typedef void (*function)(void);

static void *AON_TIMER_Handler = NULL;

static void AON_TIMER_Init_Handler()
{
    AON_TIMER_Handler = AON_TIMER();
}

static void AON_TIMER_EventCallback(uint32_t event, void *workspace)
{
    aonTimerTriggered = 1;

    printf("aon timer trigger\n");

    uint32_t counter;

    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);
}

static void AON_TIMER_RepeatMode_RC32K_Test() // 重复模式，使用RC32作为时钟源
{
    AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);

    AON_TIMER_Control(AON_TIMER_Handler,
                      HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32k);

    // 使用AON_TIMER_RELOAD_COUNT作为重装载值
    AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, AON_TIMER_RELOAD_COUNT);

    AON_TIMER_StartTimer(AON_TIMER_Handler);

    while (aonTimerTriggered == 0);

    AON_TIMER_StopTimer(AON_TIMER_Handler);

    AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);

    AON_TIMER_Uninitialize(AON_TIMER_Handler);

    printf("aon_timer end\n");
}

int main(int argc, char **argv)
{
    printf("Hello, world! aon_timer\n");

    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK = 1;

    AON_TIMER_Init_Handler();

    AON_TIMER_RepeatMode_RC32K_Test();
}
