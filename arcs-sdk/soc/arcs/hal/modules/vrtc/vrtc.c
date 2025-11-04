/****************************************************************************************
 *
 * @file vrtc.c
 *
 *
 * Copyright (C) ListenAI 2023
 *
 * Created on: Jan 11, 2024
 *
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Driver_AON_TIMER.h"
#include "ClockManager.h"
#include "arcs_ap.h"

#define VRTC_PERIOD_DEFAULT     20

#define VRTC_PERIOD_CAL         20

#define VRTC_MODE_DEFAULT       0
#define VRTC_MODE_TIMER         1

#define VRTC_UPDATE_MARGIN 1

#define GLOBAL_INT_DISABLE() vPortEnterCritical()
#define GLOBAL_INT_RESTORE() vPortExitCritical()

struct vrtc_info
{
    uint32_t sec;
    uint32_t usec;
    uint32_t origin_period;
    uint32_t current_period;
    uint32_t last_cal_time;
    uint32_t mode;
    uint32_t freq;
    void (*handler) (void);
};

struct vrtc_info vrtc_env;

static void *time_handle = NULL;
int32_t vrtc_get_time(int32_t origin, uint32_t *sec, uint32_t *usec);

extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);

static void vrtc_calc_time(uint32_t value, uint32_t *sec, uint32_t *usec)
{
    uint32_t freq;
    uint64_t tmp = value;

    freq  = vrtc_env.freq;
    *sec  = value / freq;
    tmp   = (tmp % freq) * 1000000 / freq;
    *usec = (uint32_t)tmp;
}

static void vrtc_cal_rcclk(void)
{
    uint32_t freq;

    if ((vrtc_env.sec - vrtc_env.last_cal_time) >= VRTC_PERIOD_CAL)
    {
        freq = CRM_GetSrcFreq(CRM_IpSrcAon32kClk);
        if (freq != vrtc_env.freq)
            vrtc_env.freq = freq;
        vrtc_env.last_cal_time = vrtc_env.sec;
    }
}

static void vrtc_update_time(uint32_t value)
{
    uint32_t freq;
    uint64_t tmp = value;

    freq = vrtc_env.freq;
    vrtc_env.sec += value / freq;
    tmp = (tmp % freq) * 1000000 / freq;
    vrtc_env.usec += (uint32_t)tmp;
    if (vrtc_env.usec >= 1000000)
    {
        vrtc_env.sec  += vrtc_env.usec / 1000000;
        vrtc_env.usec %= 1000000;
    }

    vrtc_cal_rcclk();
}

static void vrtc_isr(uint32_t event, void* workspace)
{
    uint32_t val = 0;

    switch (vrtc_env.mode)
    {
        case VRTC_MODE_TIMER:
            AON_TIMER_ReadTimerCount(time_handle, &val);
            AON_TIMER_SetTimerPeriodByCount(time_handle, vrtc_env.origin_period);
            if (val >= vrtc_env.current_period - 1)/*Maybe wrap*/
                vrtc_update_time(vrtc_env.current_period + (vrtc_env.current_period - val + 1 + VRTC_UPDATE_MARGIN));
            else
                vrtc_update_time(vrtc_env.current_period - val + VRTC_UPDATE_MARGIN);

            vrtc_env.mode = VRTC_MODE_DEFAULT;
            vrtc_env.current_period = vrtc_env.origin_period;
            if (vrtc_env.handler)
                vrtc_env.handler();

            break;
        default:
            vrtc_update_time(vrtc_env.origin_period);
            break;
    }
}

int32_t vrtc_get_time(int32_t origin, uint32_t *sec, uint32_t *usec)
{
    uint32_t  secl = 0, usecl = 0, val = 0;

    AON_TIMER_ReadTimerCount(time_handle, &val);
    vrtc_calc_time((vrtc_env.current_period - val), &secl, &usecl);

    *usec = vrtc_env.usec + usecl;
    *sec  = vrtc_env.sec + secl;

    if (*usec >= 1000000)
    {
        *sec  += *usec / 1000000;
        *usec %= 1000000;
    }

    return 0;
}

uint64_t vrtc_get_time_us(void)
{
    uint32_t sec, usec;
    uint64_t time;

    vrtc_get_time(0, &sec, &usec);
    time = ((uint64_t)sec) * 1000000 + usec;

    return time;
}

int32_t vrtc_set_timer(uint32_t duration, void (*handler) (void))
{
    uint32_t freq, period, val = 0;

    freq = vrtc_env.freq;

    /*105us*/
    period = vrtc_env.freq * duration / 1000000 - 1;
    GLOBAL_INT_DISABLE();
    AON_TIMER_ReadTimerCount(time_handle, &val);
    AON_TIMER_SetTimerPeriodByCount(time_handle, period);
    GLOBAL_INT_RESTORE();

    /*29us29us*/
    if (val >= vrtc_env.current_period - 1)/*Maybe wrap*/
        vrtc_update_time(vrtc_env.current_period + (vrtc_env.current_period - val + 1) + VRTC_UPDATE_MARGIN);
    else
        vrtc_update_time(vrtc_env.current_period - val + VRTC_UPDATE_MARGIN);

    vrtc_env.current_period = period;
    vrtc_env.mode    = VRTC_MODE_TIMER;
    vrtc_env.handler = handler;

    return 0;
}

int32_t vrtc_init(void)
{
    uint32_t freq;

    time_handle = AON_TIMER();

    memset(&vrtc_env, 0, sizeof(struct vrtc_info));
    freq = CRM_GetSrcFreq(CRM_IpSrcAon32kClk);
    vrtc_env.origin_period  = freq * VRTC_PERIOD_DEFAULT;
    vrtc_env.current_period = vrtc_env.origin_period;
    vrtc_env.mode = VRTC_MODE_DEFAULT;
    vrtc_env.freq = freq;

    AON_TIMER_Initialize(time_handle, vrtc_isr, NULL);
    AON_TIMER_PowerControl(time_handle, CSK_POWER_FULL);
    AON_TIMER_Control(time_handle, HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32k);
    AON_TIMER_SetTimerPeriodByCount(time_handle, vrtc_env.origin_period);
    AON_TIMER_StartTimer(time_handle);

    return 0;
}
