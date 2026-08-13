/*
 * nos_timer.h
 *
 *  Created on: Aug 14, 2020
 *
 */

#ifndef INCLUDE_BSP_NOS_TIMER_H_
#define INCLUDE_BSP_NOS_TIMER_H_

#include <stddef.h>
#include <stdint.h>

//NOTE: only used when there's NO OS support!!

void nos_timer_init();

void nos_timer_start();
uint32_t nos_timer_elapsed(); // in ms
void nos_timer_stop();

void nos_delay_ms(uint32_t nms);

#endif /* INCLUDE_BSP_NOS_TIMER_H_ */
