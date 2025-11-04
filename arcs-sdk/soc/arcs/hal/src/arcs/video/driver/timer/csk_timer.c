/*
 * csk_timer.c
 *
 *  Created on: Aug 14, 2020
 *
 */

#include "chip.h"
#include "csk_timer.h"
#include "log_print.h"
#include "systick.h"

#if 0
#include "lvgl.h"

extern uint32_t SystemCoreClock;  // 300000000
static volatile uint32_t tm_count = 0; // in ms
//static uint32_t tm_unit = 0; // 0=sec, 1=ms, 2=us

//SysTick ISR, see startup_ARMCM33.S
void SysTick_Handler(void)
{
    tm_count++;
    lv_tick_inc(1);
}

void csk_timer_init()
{
    //TODO: Add any timer initialization...
}

/* SystemCoreClock = 300000000 */
void csk_timer_start()
{
    tm_count = 0;
    SysTick_Config(SystemCoreClock / 1000);
}

uint32_t csk_timer_elapsed() // in ms
{
    return tm_count;
}

void csk_timer_stop()
{
    SysTick->CTRL &= ~(SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
}

/* must csk_timer_start first */
void csk_delay_ms(uint32_t nms)
{
	uint32_t cnt = tm_count;
    while(tm_count < (cnt + nms));
}

void csk_delay_us(uint32_t nus)
{
    uint32_t ticks = 0;
    uint32_t tcnt = 0;
    uint32_t told = 0;
    uint32_t tnow = 0;
    uint32_t reload = SysTick->LOAD;

    ticks = nus * (SystemCoreClock / 1000000);
    told = SysTick->VAL;

    while(1)
    {
        tnow = SysTick->VAL;
        if(tnow != told)
        {
            if (tnow < told) {
                tcnt += told - tnow;
            } else {
                tcnt += reload - tnow + told;
            }

            told = tnow;

            if(tcnt >= ticks) {
                break;
            }
        }
    };
}
#else
void csk_timer_init()
{
    return;
}

void csk_timer_start()
{
    return;
}

uint32_t csk_timer_elapsed()
{
    return 0;
}

void csk_timer_stop()
{
    return;
}

void csk_delay_ms(uint32_t nms)
{
    SysTick_Delay_Ms(nms);
}

void csk_delay_us(uint32_t nus)
{
    SysTick_Delay_Us(nus);
}
#endif

void csk_sw_delay_us(uint32_t cnt)
{
    volatile uint32_t i,j;

    for(i = 0; i < cnt; i++)
    {
        for(j = 0; j < 1; j++);   // VENUS:300MHz 28    ARCS-C FPGA 48MHz: 3   ARCS-D FPGA 24MHz: 1   FPGA 80MHz: 5
    }
}


