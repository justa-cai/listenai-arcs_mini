#ifndef _CSK_TIMER_H_
#define _CSK_TIMER_H_

#include "chip.h"
#include "config.h"

#ifndef DELAY_MS
//#define DELAY_MS(_dat)  csk_sw_delay_us((_dat) * 1000)
#define DELAY_MS(_dat)  csk_delay_ms(_dat)
#endif

#ifndef DELAY_US
//#define DELAY_US(_dat)  csk_sw_delay_us(_dat)
#define DELAY_US(_dat)  csk_delay_us(_dat)
#endif

void csk_timer_init();
void csk_timer_start();
uint32_t csk_timer_elapsed();
void csk_timer_stop();
void csk_delay_ms(uint32_t nms);
void csk_delay_us(uint32_t nus);
void csk_sw_delay_us(uint32_t cnt);


#endif
