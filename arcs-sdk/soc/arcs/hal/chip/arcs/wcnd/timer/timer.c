/**
 ****************************************************************************************
 *
 * @file timer.c
 *
 * @brief TIMER driver
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup TIMER
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stddef.h> // for NULL etc.
#include <stdint.h>
#include <stdbool.h>
#include "log_print.h"
#include "btos_al.h"
#include "timers.h"        // timer definition
//#include "reg_timer.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
#define TIMER_23BITS_MASK  ((1<<24)-1)
#define TIMER_GET_23BITS_TIME_STAMP(_time) ((_time) & TIMER_23BITS_MASK)

#ifndef NULL
#define NULL                        (void *)0
#endif

extern uint8_t bt_send_schedule_notify(void);

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
typedef void (*timer_cb) (void);
typedef void (*os_timer_cb) (void *time_id);

/// timer environment structure
typedef struct timer_env_
{
    /// timer id
    void *timer_id;

    /// callback to call when timer expires
    timer_cb timeout_cb;
    /// Callback to call periodically
    timer_cb periodic_cb;
} timer_env_t;

/// timer environment structure
timer_env_t timer_env;


//extern int btos_timer_cancel(TimerHandle_t timer);
//extern void *btos_timer_creat(uint32_t timer_type, uint32_t milli_seconds, os_timer_cb call_back_func);
//extern int btos_get_time(uint32_t *sec, uint32_t *usec);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
 void time_cb(void *time_id)
{
    CLOGI("time_cb, time_id = 0x%x", time_id);
    if(timer_env.timeout_cb != NULL)
        timer_env.timeout_cb();
    bt_send_schedule_notify();
}
 
void timer_init(void)
{
    timer_env.timer_id = NULL;

    timer_env.periodic_cb = NULL;
    timer_env.timeout_cb  = NULL;
}

void timer_set_timeout(uint32_t to, timer_cb cb)
{
    CLOGI("timer_set_timeout, to = %d, cb = 0x%x", to, cb);
    if(timer_env.timer_id != NULL)
        btos_timer_cancel(timer_env.timer_id);
    timer_env.timer_id = NULL;
    if(to != 0 && cb != NULL)
    {
        timer_env.timeout_cb = cb;
        timer_env.timer_id = btos_timer_creat(0, to, (TimerCallbackFunction_t)time_cb);
    }
}

void timer_set_periodic_timeout(uint32_t period, timer_cb cb)
{

}


uint32_t timer_get_time(void)
{
    uint32_t time_us;
    uint32_t sec = 0, usec = 0; 
    btos_get_time(&sec, &usec);

    time_us = sec*1000000 + usec;
    CLOGI("timer_get_time, time_ms = %d", time_us);
    return time_us;
}

uint32_t timer_get_time_ms(void)
{
    uint32_t time_ms;
    uint32_t sec = 0, msec = 0;
    btos_get_time_ms(&sec, &msec);

    time_ms = sec*1000 + msec;
    return time_ms;
}
void timer_enable(bool enable)
{

}


void timer_isr(void)
{

}

/// @} TIMER
