#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "sys_init.h"
#include "sysutils.h"

#define TAG "log_buffer"
#include "lisa_log.h"
#include "log_buffer.h"

#define LOG_BUFFER_BACKEND_NAME "log.upload.buffer"

static uint8_t g_log_buffer_storage[LOG_BUFFER_CAPACITY] __psram_noinit__;

struct log_buffer_state {
    uint32_t next_write_offset;
    uint32_t valid_bytes;
    bool snapshot_active;
};

static struct log_buffer_state g_log_buffer_state;
static SemaphoreHandle_t g_log_buffer_ctrl_mutex;

static uint32_t log_buffer_min_u32(uint32_t lhs, uint32_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

static bool log_buffer_is_ansi_color_param_byte(uint8_t byte)
{
    return (byte >= '0' && byte <= '9') || byte == ';';
}

static bool log_buffer_match_ansi_color_code(const uint8_t *log, uint32_t len, uint32_t *color_code_len)
{
    uint32_t offset = 2;

    if (!log || !color_code_len || len < 3 || log[0] != '\033' || log[1] != '[') {
        return false;
    }

    while (offset < len && log_buffer_is_ansi_color_param_byte(log[offset])) {
        offset++;
    }

    if (offset < len && log[offset] == 'm') {
        *color_code_len = offset + 1;
        return true;
    }

    return false;
}

static void log_buffer_append(const uint8_t *log, uint32_t len)
{
    uint32_t first_chunk_size;
    uint32_t wrapped_chunk_size;

    if (!log || len == 0) {
        return;
    }

    if (len >= LOG_BUFFER_CAPACITY) {
        memcpy(g_log_buffer_storage, log + len - LOG_BUFFER_CAPACITY, LOG_BUFFER_CAPACITY);
        g_log_buffer_state.next_write_offset = 0;
        g_log_buffer_state.valid_bytes = LOG_BUFFER_CAPACITY;
        return;
    }

    first_chunk_size = log_buffer_min_u32(len, LOG_BUFFER_CAPACITY - g_log_buffer_state.next_write_offset);
    memcpy(&g_log_buffer_storage[g_log_buffer_state.next_write_offset], log, first_chunk_size);

    wrapped_chunk_size = len - first_chunk_size;
    if (wrapped_chunk_size > 0) {
        memcpy(g_log_buffer_storage, log + first_chunk_size, wrapped_chunk_size);
    }

    g_log_buffer_state.next_write_offset =
        (g_log_buffer_state.next_write_offset + len) % LOG_BUFFER_CAPACITY;
    g_log_buffer_state.valid_bytes =
        log_buffer_min_u32(LOG_BUFFER_CAPACITY, g_log_buffer_state.valid_bytes + len);
}

static void log_buffer_append_without_color_code(const uint8_t *log, uint32_t len)
{
    uint32_t segment_start = 0;
    uint32_t offset = 0;

    while (offset < len) {
        uint32_t color_code_len = 0;

        if (!log_buffer_match_ansi_color_code(log + offset, len - offset, &color_code_len)) {
            offset++;
            continue;
        }

        if (offset > segment_start) {
            log_buffer_append(log + segment_start, offset - segment_start);
        }

        offset += color_code_len;
        segment_start = offset;
    }

    if (segment_start < len) {
        log_buffer_append(log + segment_start, len - segment_start);
    }
}

static void log_buffer_fill_snapshot(struct log_buffer_snapshot *snapshot)
{
    uint32_t oldest_log_offset;

    snapshot->capacity = LOG_BUFFER_CAPACITY;
    snapshot->valid_bytes = g_log_buffer_state.valid_bytes;
    snapshot->next_write_offset = g_log_buffer_state.next_write_offset;
    snapshot->first.data = NULL;
    snapshot->first.size = 0;
    snapshot->second.data = NULL;
    snapshot->second.size = 0;

    if (g_log_buffer_state.valid_bytes == 0) {
        return;
    }

    if (g_log_buffer_state.valid_bytes < LOG_BUFFER_CAPACITY) {
        snapshot->first.data = g_log_buffer_storage;
        snapshot->first.size = g_log_buffer_state.valid_bytes;
        return;
    }

    oldest_log_offset = g_log_buffer_state.next_write_offset;
    snapshot->first.data = &g_log_buffer_storage[oldest_log_offset];
    snapshot->first.size = LOG_BUFFER_CAPACITY - oldest_log_offset;

    if (oldest_log_offset > 0) {
        snapshot->second.data = g_log_buffer_storage;
        snapshot->second.size = oldest_log_offset;
    }
}

static void log_buffer_store_log(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;

    /*
     * Keep the backend best-effort and nonblocking:
     * - task-context logs are serialized by lisa_log itself
     * - logs from ISR/critical sections are skipped
     */
    if (xPortIsInsideInterrupt() || xPortIsInsideCritical()) {
        return;
    }

    log_buffer_append_without_color_code(log, len);
}

static int log_buffer_init(void)
{
    if (g_log_buffer_ctrl_mutex == NULL) {
        g_log_buffer_ctrl_mutex = xSemaphoreCreateMutex();
        if (g_log_buffer_ctrl_mutex == NULL) {
            return -ENOMEM;
        }
    }

    return lisa_log_backend_add(LOG_BUFFER_BACKEND_NAME, log_buffer_store_log, NULL);
}

int log_buffer_snapshot_acquire(struct log_buffer_snapshot *snapshot)
{
    int ret;

    if (!snapshot) {
        return -EINVAL;
    }

    if (xPortIsInsideInterrupt()) {
        return -EPERM;
    }

    xSemaphoreTake(g_log_buffer_ctrl_mutex, portMAX_DELAY);

    if (g_log_buffer_state.snapshot_active) {
        xSemaphoreGive(g_log_buffer_ctrl_mutex);
        return -EBUSY;
    }

    g_log_buffer_state.snapshot_active = true;
    ret = lisa_log_backend_pause(LOG_BUFFER_BACKEND_NAME);
    if (ret != 0) {
        g_log_buffer_state.snapshot_active = false;
        xSemaphoreGive(g_log_buffer_ctrl_mutex);
        return -EIO;
    }

    log_buffer_fill_snapshot(snapshot);
    xSemaphoreGive(g_log_buffer_ctrl_mutex);

    return 0;
}

void log_buffer_snapshot_release(void)
{
    if (xPortIsInsideInterrupt() || g_log_buffer_ctrl_mutex == NULL) {
        return;
    }

    xSemaphoreTake(g_log_buffer_ctrl_mutex, portMAX_DELAY);
    if (g_log_buffer_state.snapshot_active) {
        lisa_log_backend_resume(LOG_BUFFER_BACKEND_NAME);
        g_log_buffer_state.snapshot_active = false;
    }
    xSemaphoreGive(g_log_buffer_ctrl_mutex);
}

void log_buffer_reset(void)
{
    bool paused_here = false;

    if (xPortIsInsideInterrupt() || g_log_buffer_ctrl_mutex == NULL) {
        return;
    }

    xSemaphoreTake(g_log_buffer_ctrl_mutex, portMAX_DELAY);

    if (!g_log_buffer_state.snapshot_active) {
        if (lisa_log_backend_pause(LOG_BUFFER_BACKEND_NAME) == 0) {
            paused_here = true;
        } else {
            xSemaphoreGive(g_log_buffer_ctrl_mutex);
            return;
        }
    }

    g_log_buffer_state.next_write_offset = 0;
    g_log_buffer_state.valid_bytes = 0;

    if (paused_here) {
        lisa_log_backend_resume(LOG_BUFFER_BACKEND_NAME);
    }

    xSemaphoreGive(g_log_buffer_ctrl_mutex);
}

SYS_INIT(log_buffer_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
