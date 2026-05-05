/**
 * @file modem_endpoint_runtime.h
 * @brief Shared endpoint buffering and backpressure helpers
 */

#ifndef LISA_MODEM_DRIVERS_COMMON_MODEM_ENDPOINT_RUNTIME_H
#define LISA_MODEM_DRIVERS_COMMON_MODEM_ENDPOINT_RUNTIME_H

#include "ring_buffer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct ring_buf *ring_buf;
    uint32_t high_watermark;
    uint32_t low_watermark;
    uint32_t drop_count;
    bool rx_pending;
    bool rx_overflow;
    bool pull_paused;
} modem_endpoint_runtime_t;

void modem_endpoint_runtime_init(modem_endpoint_runtime_t *runtime, struct ring_buf *ring_buf,
                                 uint32_t high_watermark, uint32_t low_watermark);
uint32_t modem_endpoint_runtime_write_rx(modem_endpoint_runtime_t *runtime,
                                         const uint8_t *data, uint32_t length);
int modem_endpoint_runtime_read_rx(modem_endpoint_runtime_t *runtime, uint8_t *buffer, uint32_t length);
uint32_t modem_endpoint_runtime_buffered_size(const modem_endpoint_runtime_t *runtime);
bool modem_endpoint_runtime_note_read(modem_endpoint_runtime_t *runtime);
void modem_endpoint_runtime_mark_pending(modem_endpoint_runtime_t *runtime, bool pending);
bool modem_endpoint_runtime_should_pull(const modem_endpoint_runtime_t *runtime);
uint32_t modem_endpoint_runtime_effective_pull_timeout(uint32_t requested_ms,
                                                       uint32_t fallback_ms,
                                                       uint32_t minimum_ms);

#ifdef __cplusplus
}
#endif

#endif
