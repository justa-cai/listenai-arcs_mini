/****************************************************************************************
 *
 * @file vrtc.h
 *
 *
 * Copyright (C) ListenAI 2023
 *
 * Created on: Jan 11, 2024
 *
 *
 ****************************************************************************************
 */

#ifndef __VRTC_H__
#define __VRTC_H__

int32_t vrtc_init(void);
int32_t vrtc_set_timer(uint32_t duration, void (*handler) (void));
uint64_t vrtc_get_time_us(void);
int32_t vrtc_get_time(int32_t origin, uint32_t *sec, uint32_t *usec);
#endif

