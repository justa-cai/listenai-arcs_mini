/**
 ****************************************************************************************
 *
 * @file wifi_rtos.h
 *
 * @brief Definitions of the wifi task info.
 *
 * Copyright (C) ListenAI 2024-2099
 *
 ****************************************************************************************
 */

#ifndef _WIFI_RTOS_H_
#define _WIFI_RTOS_H_

/**
 ****************************************************************************************
 * @defgroup WIFI_RTOS WIFI_RTOS
 * @ingroup WIFI
 * @brief Task priorities and stack size definitions.
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "rtos_al.h"

/*
 * DEFINITIONS
 ****************************************************************************************
 */
/// Definitions of the different FHOST task priorities
enum
{
    /// Priority of the WiFi task
    FHOST_WIFI_PRIORITY = RTOS_TASK_PRIORITY(7),
    /// Priority of the WiFi task (when high priority is set)
    FHOST_WIFI_PRIORITY_HIGH = RTOS_TASK_PRIORITY(10),
    /// Priority of the control task
    FHOST_CNTRL_PRIORITY = RTOS_TASK_PRIORITY(7),
    /// Priority of the RX task
    FHOST_RX_PRIORITY = RTOS_TASK_PRIORITY(7),
    /// Priority of the TX task
    FHOST_TX_PRIORITY = RTOS_TASK_PRIORITY(9),
    /// Priority of the WPA task
    FHOST_WPA_PRIORITY = RTOS_TASK_PRIORITY(8),
    /// Priority of the smartconfig task
    FHOST_SMARTCONF_PRIORITY = RTOS_TASK_PRIORITY(7),
    FHOST_AGING_TEST_PRIORITY = RTOS_TASK_PRIORITY(6),
    FHOST_TRACE_DUMP_PRIORITY = RTOS_TASK_PRIORITY(6),
};

/// Definitions of the different FHOST task stack size requirements
enum
{
    /// WiFi task stack size
    FHOST_WIFI_STACK_SIZE = 768,
    /// Control task stack size
    FHOST_CNTRL_STACK_SIZE = 512,
    /// RX task stack size
    FHOST_RX_STACK_SIZE = 768,
    /// TX task stack size
    FHOST_TX_STACK_SIZE = 384,
    /// WPA task stack size
    FHOST_WPA_STACK_SIZE = 1024,
    /// Smartconfig task stack size
    FHOST_SMARTCONF_STACK_SIZE = 512,
    FHOST_AGING_STACK_SIZE = 256,
    FHOST_TRACE_DUMP_STACK_SIZE = 256,

};

/// @}

#endif // _WIFI_RTOS_H_
