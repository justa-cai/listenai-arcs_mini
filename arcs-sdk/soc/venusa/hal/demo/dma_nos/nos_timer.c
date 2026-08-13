/*
 * nos_timer.c
 *
 *  Created on: Aug 14, 2020  for VENUS
 *  Ported on: Feb. 3, 2024 for MARS
 *  Ported on: Mar. 26, 2025 for VENUSA
 *
 */


#include "nos_timer.h"
#include "venusa_ap.h"
//#include "core_feature_timer.h"

extern volatile uint32_t SystemCoreClock;

static volatile uint32_t tm_count = 0; // in ms
static uint32_t ticks_per_ms = 0;

//void SysTick_Handler(void)
void eclic_mtip_handler(void)
{
    tm_count++;
    SysTick_Reload(ticks_per_ms);
}

void nos_timer_init()
{
//    //TODO: Add any timer initialization...
//    ECLIC_DisableIRQ(SysTimer_IRQn);
//    register_ISR(SysTimer_IRQn, My_SysTimer_Handler, NULL);
//
//    ECLIC_SetShvIRQ(SysTimer_IRQn, ECLIC_NON_VECTOR_INTERRUPT);
//    ECLIC_SetLevelIRQ(SysTimer_IRQn, 0);
//    ECLIC_EnableIRQ(SysTimer_IRQn);

    uint32_t mtime_div = IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_MTIME_TOGGLE_M;
    if (mtime_div == 0) mtime_div = 48;

    // double edge, no *2 in hardware internal
    ticks_per_ms = SystemCoreClock / 1000 / mtime_div * 2;
    SysTick_Config(ticks_per_ms);
}

void nos_timer_start()
{
    tm_count = 0;
    //SysTick_Config(SystemCoreClock / 1000);

    SysTick_Reload(ticks_per_ms);
    SysTimer_Start();
}

uint32_t nos_timer_elapsed() // in ms
{
    return tm_count;
}

void nos_timer_stop()
{
    SysTimer_Stop();
}

void nos_delay_ms(uint32_t nms)
{
    nos_timer_start();
    while(tm_count < nms);
    nos_timer_stop();

/*
    int64_t  delta_mtime;
    uint64_t start_mtime, delay_ticks = ticks_per_ms * nms;

    start_mtime = SysTimer_GetLoadValue();
    do {
        delta_mtime = SysTimer_GetLoadValue() - start_mtime;
        if (delta_mtime < 0) {
            delta_mtime = ~delta_mtime;
            delta_mtime++;
        }
    } while (delta_mtime < delay_ticks);
*/
}
