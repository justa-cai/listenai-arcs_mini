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


enum
{
    VRTC_TIMER_IDX_LOCAL = 0,  /*local timer*/
    VRTC_TIMER_IDX_IPC,    /*ipc timer*/
    VRTC_TIMER_IDX_MAX,
    VRTC_TIMER_IDX_PERM,   /*permanent timer*/
};


int32_t vrtc_init(void);
int32_t vrtc_set_timer(int32_t timer_idx, uint32_t duration_us, void (*handler) (void));
uint64_t vrtc_get_time_us(void);
int32_t vrtc_get_time(int32_t origin, uint32_t *sec, uint32_t *usec);
int32_t vrtc_set_timer_from_ipc(void);

#endif

