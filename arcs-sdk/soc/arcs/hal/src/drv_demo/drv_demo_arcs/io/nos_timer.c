/*
 * nos_timer.c
 *
 *  Created on: Aug 14, 2020
 *
 */


#include "nos_timer.h"
#include <ARMCM33_DSP_FP.h>
#include "core_cm33.h"


extern uint32_t SystemCoreClock;
static uint32_t tm_count = 0; // in ms
//static uint32_t tm_unit = 0; // 0=sec, 1=ms, 2=us

//SysTick ISR, see startup_ARMCM33.S
void SysTick_Handler(void)
{
    tm_count++;
}


void nos_timer_start()
{
    tm_count = 0;
    SysTick_Config(SystemCoreClock / 1000);
}

uint32_t nos_timer_elapsed() // in ms
{
    return tm_count;
}

void nos_timer_stop()
{
    SysTick->CTRL &= ~(SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
}

void nos_delay_ms(uint32_t nms)
{
    nos_timer_start();
    while(tm_count < nms);
    nos_timer_stop();
}
