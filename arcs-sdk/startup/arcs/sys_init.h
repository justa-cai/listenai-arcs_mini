/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sys_init.h
 * @brief ARCS System Initialization Framework
 *
 * Provides a Zephyr SYS_INIT-like system initialization mechanism
 * with multi-level priorities and sub-priorities support.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Initialization Priority Levels
 * ========================================================================
 *
 * ARCS system initialization is divided into phases based on actual boot flow:
 *
 * Boot Flow:
 *   startup.S (_start) -> platform_pre_startup [PRE_SYSTEM_INIT] ->
 *   scatload -> SystemInit -> entry() -> lisa_device_init() ->
 *   vTaskStartScheduler() -> main_task -> main()
 *
 * Initialization Levels:
 * - PRE_SYSTEM_INIT:  BEFORE scatload (in platform_pre_startup)
 *                     ⚠️ CRITICAL: .data NOT copied, .bss NOT zeroed!
 *                     ⚠️ DO NOT USE GLOBAL VARIABLES!
 *                     Interrupts disabled, no RTOS
 *                     Use case: Critical hardware that must run before memory init
 *
 * - PRE_DEVICES_INIT: Before lisa_device_init(), in entry()
 *                     Interrupts disabled, no RTOS, heap ready
 *                     Use case: Preparation before device initialization
 *
 * - PRE_KERNEL:       Before vTaskStartScheduler(), in entry()
 *                     Interrupts enabled, no RTOS, all resources ready
 *                     Use case: Final setup before kernel starts
 *
 * - POST_KERNEL:      After vTaskStartScheduler(), in main_task
 *                     RTOS fully running, can create tasks/semaphores
 *                     Use case: Driver initialization requiring RTOS
 *
 * - PRE_APPLICATION:  Before main(), in main_task
 *                     RTOS fully running, last init before app
 *                     Use case: Application-level initialization
 *
 * Within each level, sub-priority (0-99) provides fine-grained control
 * ======================================================================== */

#define SYS_INIT_LEVEL_PRE_SYSTEM_INIT   0   /* BEFORE scatload ⚠️ NO GLOBALS! */
#define SYS_INIT_LEVEL_PRE_DEVICES_INIT  1   /* Before lisa_device_init() */
#define SYS_INIT_LEVEL_PRE_KERNEL        2   /* Before vTaskStartScheduler() */
#define SYS_INIT_LEVEL_POST_KERNEL       3   /* After vTaskStartScheduler() */
#define SYS_INIT_LEVEL_PRE_APPLICATION   4   /* Before main() */
#define SYS_INIT_LEVEL_MAX               (SYS_INIT_LEVEL_PRE_APPLICATION + 1)   /* Total number of levels */

/* Sub-priority range: 0-99 (lower value = higher priority) */
#define SYS_INIT_SUB_PRIORITY_MIN      0
#define SYS_INIT_SUB_PRIORITY_MAX      99

/* Common sub-priority constants */
#define SYS_INIT_SUB_PRIORITY_FIRST    0   /* Initialize first */
#define SYS_INIT_SUB_PRIORITY_EARLY    10  /* Early initialization */
#define SYS_INIT_SUB_PRIORITY_DEFAULT  50  /* Default priority */
#define SYS_INIT_SUB_PRIORITY_LATE     90  /* Late initialization */
#define SYS_INIT_SUB_PRIORITY_LAST     99  /* Initialize last */

/* ========================================================================
 * Initialization Function Type Definition
 * ======================================================================== */

/**
 * @brief System initialization function type
 * @return 0 on success, negative error code on failure
 */
typedef int (*sys_init_fn_t)(void);

/**
 * @brief System initialization entry structure
 */
typedef struct {
    sys_init_fn_t init_fn;      /* Initialization function */
    uint8_t level;              /* Priority level (0-4) */
    uint8_t sub_priority;       /* Sub-priority (0-99) */
    const char *name;           /* Function name (for debugging) */
} sys_init_entry_t;

/* ========================================================================
 * Section Attribute Definition
 * ======================================================================== */

/**
 * @brief Calculate section sorting value
 *
 * Format: level(1 digit) + sub_priority(2 digits, zero-padded)
 * Example: level=1, sub=5  -> "105"
 *          level=2, sub=99 -> "299"
 */
#define _SYS_INIT_SECTION_NAME(level, sub_priority) \
    ".sys_init." #level #sub_priority

/* Section attribute macro */
#define _SYS_INIT_SECTION(level, sub_priority) \
    __attribute__((used, section(_SYS_INIT_SECTION_NAME(level, sub_priority))))

/* ========================================================================
 * System Initialization Registration Macro
 * ========================================================================
 *
 * @brief Register a system initialization function
 *
 * @param _init_fn       Initialization function, type: int (*)(void)
 * @param _level         Priority level (SYS_INIT_LEVEL_xxx)
 * @param _sub_priority  Sub-priority (0-99)
 *
 * @note Init function returns 0 on success, negative on failure
 * @note Lower numeric value = higher priority (executes earlier)
 * @note Sub-priority must be 0-99, compile error otherwise
 *
 * Usage example:
 * @code
 * static int my_early_init(void) {
 *     // Early initialization code
 *     return 0;
 * }
 *
 * // Initialize before SystemInit, sub-priority 10
 * SYS_INIT(my_early_init, SYS_INIT_LEVEL_PRE_SYSTEM_INIT, 10);
 * @endcode
 * ======================================================================== */

#define SYS_INIT(_init_fn, _level, _sub_priority)                                                     \
    /* Compile-time check: level range */                                                             \
    _Static_assert((_level) >= SYS_INIT_LEVEL_PRE_SYSTEM_INIT &&                                      \
                   (_level) <= SYS_INIT_LEVEL_PRE_APPLICATION,                                        \
                   "SYS_INIT level must be between 0-4");                                             \
    /* Compile-time check: sub-priority range */                                                      \
    _Static_assert((_sub_priority) >= SYS_INIT_SUB_PRIORITY_MIN &&                                    \
                   (_sub_priority) <= SYS_INIT_SUB_PRIORITY_MAX,                                      \
                   "SYS_INIT sub_priority must be between 0-99");                                     \
    /* Generate unique variable name */                                                               \
    static const sys_init_entry_t                                                                     \
        __sys_init_entry_##_init_fn##_##_level##_##_sub_priority                                      \
        _SYS_INIT_SECTION(_level, _sub_priority) = {                                                  \
            .init_fn = (_init_fn),                                                                    \
            .level = (_level),                                                                        \
            .sub_priority = (_sub_priority),                                                          \
            .name = #_init_fn,                                                                        \
        }

/* ========================================================================
 * System Initialization Management API
 * ======================================================================== */

/**
 * @brief Execute initialization functions at specified level
 *
 * Calls all init functions at the given level in priority order
 *
 * @param level Initialization level to execute (SYS_INIT_LEVEL_xxx)
 * @return Number of successfully initialized functions, negative on error
 */
int sys_init_run_level(uint8_t level);

/**
 * @brief Execute all initialization levels
 *
 * Executes all init functions in level and sub-priority order
 *
 * @return Total number of successfully initialized functions, negative on error
 */
int sys_init_run_all(void);

/**
 * @brief Get system initialization statistics
 *
 * @param level Initialization level, pass 0xFF to get count for all levels
 * @return Number of registered init functions at this level
 */
int sys_init_get_count(uint8_t level);

#ifdef __cplusplus
}
#endif
