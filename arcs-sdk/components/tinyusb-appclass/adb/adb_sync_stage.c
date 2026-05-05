#include "adb_sync_stage.h"

#include "sysheap.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int adb_sync_stage_flush(struct adb_sync_stage *stage, adb_sync_stage_flush_fn flush, void *user_data)
{
    int r;

    if (stage == NULL || flush == NULL) {
        return -1;
    }

    if (stage->size == 0U) {
        return 0;
    }

    r = flush(user_data, stage->buf, stage->size);
    if (r == 0) {
        stage->size = 0U;
    }

    return r;
}

int adb_sync_stage_init(struct adb_sync_stage *stage, uint32_t capacity, uint32_t alignment)
{
    if (stage == NULL || capacity == 0U || alignment == 0U) {
        return -1;
    }

    memset(stage, 0, sizeof(*stage));
    stage->buf = exram_malloc(alignment, capacity);
    if (stage->buf == NULL) {
        return -1;
    }

    stage->capacity = capacity;
    return 0;
}

void adb_sync_stage_deinit(struct adb_sync_stage *stage)
{
    if (stage == NULL) {
        return;
    }

    exram_free(stage->buf);
    memset(stage, 0, sizeof(*stage));
}

int adb_sync_stage_write(struct adb_sync_stage *stage, const uint8_t *data, uint32_t len,
                         adb_sync_stage_flush_fn flush, void *user_data)
{
    if (stage == NULL || data == NULL || flush == NULL) {
        return -1;
    }

    while (len != 0U) {
        uint32_t copy_len;
        int r;

        if (stage->size == stage->capacity) {
            r = adb_sync_stage_flush(stage, flush, user_data);
            if (r != 0) {
                return r;
            }
        }

        copy_len = len;
        if ((stage->size + copy_len) > stage->capacity) {
            copy_len = stage->capacity - stage->size;
        }

        memcpy(stage->buf + stage->size, data, copy_len);
        stage->size += copy_len;
        data += copy_len;
        len -= copy_len;

        if (stage->size == stage->capacity) {
            r = adb_sync_stage_flush(stage, flush, user_data);
            if (r != 0) {
                return r;
            }
        }
    }

    return 0;
}

int adb_sync_stage_finish(struct adb_sync_stage *stage, adb_sync_stage_flush_fn flush, void *user_data)
{
    if (stage == NULL || flush == NULL) {
        return -1;
    }

    return adb_sync_stage_flush(stage, flush, user_data);
}
