/*
 * Copyright (c) 2012-2017 Andes Technology Corporation
 * All rights reserved.
 *
 */

#ifndef __GPIO_AE210P_H
#define __GPIO_AE210P_H

#include "Driver_GPIO.h"

#include "arcs_ap.h"

#define ARCS_MAX_GPIOA         (32)
#define ARCS_MAX_GPIOB         (10)

// GPIO flags
#define GPIO_FLAG_INITIALIZED                (1U << 0)
#define GPIO_FLAG_POWERED                    (1U << 1)
#define GPIO_FLAG_CONFIGURED                 (1U << 2)

typedef struct {
    CSK_GPIO_SignalEvent_t cb_event;  // event callback
    void* workspace;
    _GPIO_ *gpio_info;
} _GPIO_INFO;

// GPIO information
typedef struct
{
    GPIO_RegDef* reg;
    uint32_t irq_num;                 // GPIO IRQ Number
    void (*irq_handler)(void);
    uint32_t max_num;
    _GPIO_INFO *info;
}const GPIO_RESOURCES;

#endif /* __GPIO_AE210P_H */
