/**
 * @file modem_endpoint_runtime.c
 * @brief Shared endpoint buffering and backpressure helpers
 */

#include "drivers/common/modem_endpoint_runtime.h"

static uint32_t modem_endpoint_runtime_capacity(const modem_endpoint_runtime_t *runtime)
{
    return (runtime && runtime->ring_buf) ? ring_buf_capacity_get(runtime->ring_buf) : 0U;
}

void modem_endpoint_runtime_init(modem_endpoint_runtime_t *runtime, struct ring_buf *ring_buf,
                                 uint32_t high_watermark, uint32_t low_watermark)
{
    uint32_t capacity;

    if (!runtime) {
        return;
    }

    runtime->ring_buf = ring_buf;
    runtime->drop_count = 0U;
    runtime->rx_pending = false;
    runtime->rx_overflow = false;
    runtime->pull_paused = false;

    capacity = modem_endpoint_runtime_capacity(runtime);
    if (capacity == 0U) {
        runtime->high_watermark = 0U;
        runtime->low_watermark = 0U;
        return;
    }

    runtime->high_watermark = (high_watermark == 0U || high_watermark > capacity)
                            ? capacity : high_watermark;
    runtime->low_watermark = (low_watermark > runtime->high_watermark)
                           ? runtime->high_watermark : low_watermark;
}

uint32_t modem_endpoint_runtime_write_rx(modem_endpoint_runtime_t *runtime,
                                         const uint8_t *data, uint32_t length)
{
    uint32_t written;
    uint32_t buffered;

    if (!runtime || !runtime->ring_buf || !data || length == 0U) {
        return 0U;
    }

    written = ring_buf_put(runtime->ring_buf, data, length);
    if (written < length) {
        runtime->drop_count += (length - written);
        runtime->rx_overflow = true;
        runtime->pull_paused = true;
    }

    buffered = ring_buf_size_get(runtime->ring_buf);
    if (runtime->high_watermark > 0U && buffered >= runtime->high_watermark) {
        runtime->pull_paused = true;
    }

    return written;
}

int modem_endpoint_runtime_read_rx(modem_endpoint_runtime_t *runtime, uint8_t *buffer, uint32_t length)
{
    uint32_t bytes_read;

    if (!runtime || !runtime->ring_buf || !buffer || length == 0U) {
        return -1;
    }

    bytes_read = ring_buf_get(runtime->ring_buf, buffer, length);
    if (bytes_read == 0U) {
        return -1;
    }

    (void)modem_endpoint_runtime_note_read(runtime);
    return (int)bytes_read;
}

uint32_t modem_endpoint_runtime_buffered_size(const modem_endpoint_runtime_t *runtime)
{
    return (runtime && runtime->ring_buf) ? ring_buf_size_get(runtime->ring_buf) : 0U;
}

bool modem_endpoint_runtime_note_read(modem_endpoint_runtime_t *runtime)
{
    uint32_t buffered;
    bool was_paused;

    if (!runtime || !runtime->ring_buf) {
        return false;
    }

    was_paused = runtime->pull_paused;
    buffered = modem_endpoint_runtime_buffered_size(runtime);
    if (buffered <= runtime->low_watermark) {
        runtime->pull_paused = false;
        runtime->rx_overflow = false;
    }

    return was_paused && !runtime->pull_paused &&
           runtime->rx_pending && ring_buf_space_get(runtime->ring_buf) > 0U;
}

void modem_endpoint_runtime_mark_pending(modem_endpoint_runtime_t *runtime, bool pending)
{
    if (runtime) {
        runtime->rx_pending = pending;
    }
}

bool modem_endpoint_runtime_should_pull(const modem_endpoint_runtime_t *runtime)
{
    if (!runtime || !runtime->ring_buf || !runtime->rx_pending) {
        return false;
    }

    if (runtime->pull_paused || ring_buf_space_get(runtime->ring_buf) == 0U) {
        return false;
    }

    return true;
}

uint32_t modem_endpoint_runtime_effective_pull_timeout(uint32_t requested_ms,
                                                       uint32_t fallback_ms,
                                                       uint32_t minimum_ms)
{
    uint32_t timeout_ms = requested_ms;

    if (timeout_ms == 0U) {
        timeout_ms = fallback_ms;
    }
    if (timeout_ms < minimum_ms) {
        timeout_ms = minimum_ms;
    }

    return timeout_ms;
}
