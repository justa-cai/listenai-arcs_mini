#include "adb_sync_stage.h"

#include "adb_utils.h"

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
#include "esp_heap_caps.h"
#endif

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

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
static int adb_sync_stage_try_alloc_boot(struct adb_sync_stage *stage, uint32_t capacity, uint32_t alignment)
{
    size_t required_internal_block = (size_t)capacity + (size_t)alignment;
    size_t largest_internal_block = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);

    printf("boot adb: stage alloc req=%u align=%u largest_internal=%u\n",
           (unsigned int)capacity,
           (unsigned int)alignment,
           (unsigned int)largest_internal_block);
    if (largest_internal_block >= required_internal_block) {
        stage->buf = adb_boot_try_inram_malloc(alignment, capacity);
        if (stage->buf != NULL) {
            printf("boot adb: stage alloc ok req=%u buf=%p\n",
                   (unsigned int)capacity,
                   stage->buf);
            stage->free_fn = inram_free;
            return 0;
        }
    }

    stage->buf = psram_malloc_align(alignment, capacity);
    if (stage->buf == NULL) {
        return -1;
    }

    printf("boot adb: stage alloc fallback psram req=%u buf=%p\n",
           (unsigned int)capacity,
           stage->buf);
    stage->free_fn = psram_free;
    return 0;
}
#endif

int adb_sync_stage_init(struct adb_sync_stage *stage, uint32_t capacity, uint32_t alignment)
{
    if (stage == NULL || capacity == 0U || alignment == 0U) {
        return -1;
    }

    memset(stage, 0, sizeof(*stage));

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
    if (adb_sync_stage_try_alloc_boot(stage, capacity, alignment) != 0) {
        return -1;
    }
#elif defined(CONFIG_BOOT_ADB)
    stage->buf = adb_boot_try_inram_malloc(alignment, capacity);
    stage->free_fn = inram_free;
#else
    stage->buf = exram_malloc(alignment, capacity);
    stage->free_fn = exram_free;
#endif
    if (stage->buf == NULL) {
        stage->free_fn = NULL;
        return -1;
    }

    if (stage->capacity == 0U) {
        stage->capacity = capacity;
    }
    return 0;
}

int adb_sync_stage_attach(struct adb_sync_stage *stage, uint8_t *buf, uint32_t capacity)
{
    if (stage == NULL || buf == NULL || capacity == 0U) {
        return -1;
    }

    memset(stage, 0, sizeof(*stage));
    stage->buf = buf;
    stage->capacity = capacity;
    return 0;
}

void adb_sync_stage_deinit(struct adb_sync_stage *stage)
{
    if (stage == NULL) {
        return;
    }

    if (stage->buf != NULL && stage->free_fn != NULL) {
        stage->free_fn(stage->buf);
    }
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
