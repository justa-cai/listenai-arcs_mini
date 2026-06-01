#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keep the latest 512KB of business logs for on-demand upload. */
#define LOG_BUFFER_CAPACITY (512U * 1024U)

struct log_buffer_span {
    const uint8_t *data;
    uint32_t size;
};

struct log_buffer_snapshot {
    struct log_buffer_span first;
    struct log_buffer_span second;
    uint32_t capacity;
    uint32_t valid_bytes;
    uint32_t next_write_offset;
};

/*
 * Acquire a stable upload view by temporarily pausing this backend.
 * New logs are dropped until log_buffer_snapshot_release() is called.
 */
int log_buffer_snapshot_acquire(struct log_buffer_snapshot *snapshot);
void log_buffer_snapshot_release(void);
void log_buffer_reset(void);

#ifdef __cplusplus
}
#endif
