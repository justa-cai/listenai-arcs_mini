#ifndef __ADB_SYNC_PROTOCOL_H__
#define __ADB_SYNC_PROTOCOL_H__

#include <stdint.h>

#define ADB_SYNC_MAX_DATA_CHUNK_SIZE (64U * 1024U)

static inline uint32_t adb_sync_data_chunk_limit(uint32_t max_payload)
{
    if (max_payload > ADB_SYNC_MAX_DATA_CHUNK_SIZE) {
        return ADB_SYNC_MAX_DATA_CHUNK_SIZE;
    }

    return max_payload;
}

#endif
