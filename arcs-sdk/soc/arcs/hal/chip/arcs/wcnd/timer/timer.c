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
//#include "timer.h"        // timer definition
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
/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
typedef void (*timer_cb) (void);

/// timer environment structure
typedef struct timer_env_
{
    /// callback to call when timer expires
    timer_cb timeout_cb;
    /// Callback to call periodically
    timer_cb periodic_cb;
} timer_env_t;

/// timer environment structure
timer_env_t timer_env;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void timer_init(void)
{
    timer_env.periodic_cb = NULL;
    timer_env.timeout_cb  = NULL;
}

void timer_set_timeout(uint32_t to, timer_cb cb)
{

}

void timer_set_periodic_timeout(uint32_t period, timer_cb cb)
{

}


uint32_t timer_get_time(void)
{
    // round timer up
    return 0;
}

void timer_enable(bool enable)
{

}


void timer_isr(void)
{

}

/// @} TIMER
