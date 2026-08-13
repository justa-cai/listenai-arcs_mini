/**
 ****************************************************************************************
 *
 * @file ipc_print_policy.h
 *
 * @brief IPC print policy helpers.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#ifndef IPC_PRINT_POLICY_H
#define IPC_PRINT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#ifndef CONFIG_ARCS_HAL_IPC_PRINT_NOTIFY_THRESHOLD
#define CONFIG_ARCS_HAL_IPC_PRINT_NOTIFY_THRESHOLD 128
#endif

#ifndef CONFIG_ARCS_HAL_IPC_PRINT_POLL_TIMEOUT_MS
#define CONFIG_ARCS_HAL_IPC_PRINT_POLL_TIMEOUT_MS 3000
#endif

static inline uint32_t ipc_print_notify_threshold_get(void)
{
    return (uint32_t)CONFIG_ARCS_HAL_IPC_PRINT_NOTIFY_THRESHOLD;
}

static inline int32_t ipc_print_poll_timeout_ms_get(void)
{
    return (int32_t)CONFIG_ARCS_HAL_IPC_PRINT_POLL_TIMEOUT_MS;
}

static inline bool ipc_print_should_notify(uint32_t pending_bytes, bool reader_active,
                                           bool reader_progressed)
{
    return (pending_bytes > ipc_print_notify_threshold_get()) && !reader_active &&
           reader_progressed;
}

#endif /* IPC_PRINT_POLICY_H */
