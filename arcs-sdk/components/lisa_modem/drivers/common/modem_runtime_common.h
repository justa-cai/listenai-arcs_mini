/**
 * @file modem_runtime_common.h
 * @brief Shared helpers for modem endpoint runtimes
 */

#ifndef LISA_MODEM_DRIVERS_COMMON_MODEM_RUNTIME_COMMON_H
#define LISA_MODEM_DRIVERS_COMMON_MODEM_RUNTIME_COMMON_H

#include "ring_buffer.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODEM_NETWORK_STATUS_DISCONNECTED,
    MODEM_NETWORK_STATUS_REGISTERED_HOME,
    MODEM_NETWORK_STATUS_SEARCHING,
    MODEM_NETWORK_STATUS_DENIED,
    MODEM_NETWORK_STATUS_UNKNOWN,
    MODEM_NETWORK_STATUS_REGISTERED_ROAMING,
    MODEM_NETWORK_STATUS_READY,
    MODEM_NETWORK_STATUS_ERROR
} modem_network_status_t;

typedef modem_network_status_t network_status_t;

#define NETWORK_STATUS_DISCONNECTED       MODEM_NETWORK_STATUS_DISCONNECTED
#define NETWORK_STATUS_REGISTERED_HOME    MODEM_NETWORK_STATUS_REGISTERED_HOME
#define NETWORK_STATUS_SEARCHING          MODEM_NETWORK_STATUS_SEARCHING
#define NETWORK_STATUS_DENIED             MODEM_NETWORK_STATUS_DENIED
#define NETWORK_STATUS_UNKNOWN            MODEM_NETWORK_STATUS_UNKNOWN
#define NETWORK_STATUS_REGISTERED_ROAMING MODEM_NETWORK_STATUS_REGISTERED_ROAMING
#define NETWORK_STATUS_READY              MODEM_NETWORK_STATUS_READY
#define NETWORK_STATUS_ERROR              MODEM_NETWORK_STATUS_ERROR

typedef void (*modem_runtime_wait_hook_t)(void *user_data);

/**
 * @brief Read from ring buffer, blocking until any data is available or timeout.
 *
 * @param ring_buf    Ring buffer to read from
 * @param buffer      Destination buffer
 * @param length      Desired read length (partial reads are allowed)
 * @param timeout_ms  Maximum wait time in milliseconds
 * @param data_sem    Semaphore signalled when new data arrives (may be NULL)
 * @param wait_hook   Fallback polling hook called when data_sem is NULL
 * @param user_data   Context for wait_hook
 * @return bytes read, or -1 on error/timeout
 */
int modem_runtime_recv_ring(struct ring_buf *ring_buf, char *buffer, size_t length,
                            uint32_t timeout_ms, SemaphoreHandle_t data_sem,
                            modem_runtime_wait_hook_t wait_hook, void *user_data);

#ifdef __cplusplus
}
#endif

#endif
