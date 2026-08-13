/*
 * Copyright (c) 2012-2017 Andes Technology Corporation
 * All rights reserved.
 *
 * @file    gpio.h
 * @author  Andes Technology Corporation
 * @brief   GPIO Hardware Abstraction Layer (HAL) Peripheral Definitions
 *          This header defines platform-specific GPIO parameters, data structures,
 *          and resource configurations for the AE210P microcontroller family.
 *
 * @details Contains maximum pin counts, interrupt vector definitions,
 *          status flags, and core data structures used by the GPIO driver.
 */

#ifndef __GPIO_H
#define __GPIO_H

#include "Driver_GPIO.h"
#include "venusa_ap.h"

/**
 * @brief Maximum Number of GPIO Pins in Port A
 * @details Based on physical hardware capabilities of the AE210P SoC.
 */
#define VENUSA_MAX_GPIOA         (32)

/**
 * @brief Maximum Number of GPIO Pins in Port B
 * @details Based on physical hardware capabilities of the AE210P SoC.
 */
#define VENUSA_MAX_GPIOB         (21)

// GPIO Status Flags Bit Manipulation Macros
/** @brief Indicates successful initialization sequence completed */
#define GPIO_FLAG_INITIALIZED                (1U << 0)
/** @brief Power supply to GPIO module is stable */
#define GPIO_FLAG_POWERED                    (1U << 1)
/** @brief GPIO port has been fully configured */
#define GPIO_FLAG_CONFIGURED                 (1U << 2)

/**
 * @brief Internal GPIO Information Container
 * @details Stores callback function, user workspace pointer, and per-pin state array.
 *          Maintained privately by the driver for operational context.
 */
typedef struct {
    CSK_GPIO_SignalEvent_t cb_event;  /*!< Event callback function pointer */
    void* workspace;                   /*!< User-provided working memory block */
    _GPIO_ *gpio_info;                 /*!< Array of individual pin states       */
} _GPIO_INFO;

/**
 * @brief GPIO Peripheral Resource Descriptor
 * @details Central data structure defining hardware resources and configuration:
 *          - Base address register mapping
 *          - Interrupt vector number
 *          - IRQ handler function pointer
 *          - Maximum pin count
 *          - Associated info container
 *          Used by driver to manage multiple GPIO instances (Port A/B).
 */
typedef struct
{
    GPIO_RegDef* reg;                   /*!< Mapped register base address      */
    uint32_t irq_num;                   /*!< Interrupt request line number     */
    void (*irq_handler)(void);           /*!< Registered IRQ handler function    */
    uint32_t max_num;                   /*!< Maximum supported pin count       */
    _GPIO_INFO *info;                   /*!< Associated info container         */
} const GPIO_RESOURCES;

#endif /* __GPIO_H */
