#ifndef __ADB_SYNC_STAGE_H__
#define __ADB_SYNC_STAGE_H__

#include <stdint.h>

struct adb_sync_stage {
    uint8_t *buf;
    uint32_t capacity;
    uint32_t size;
    void (*free_fn)(void *ptr);
};

typedef int (*adb_sync_stage_flush_fn)(void *user_data, const uint8_t *data, uint32_t len);

int adb_sync_stage_init(struct adb_sync_stage *stage, uint32_t capacity, uint32_t alignment);
int adb_sync_stage_attach(struct adb_sync_stage *stage, uint8_t *buf, uint32_t capacity);
void adb_sync_stage_deinit(struct adb_sync_stage *stage);
int adb_sync_stage_write(struct adb_sync_stage *stage, const uint8_t *data, uint32_t len,
                         adb_sync_stage_flush_fn flush, void *user_data);
int adb_sync_stage_finish(struct adb_sync_stage *stage, adb_sync_stage_flush_fn flush, void *user_data);

#endif
