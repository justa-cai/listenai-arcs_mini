#include "miniapp.h"
#include "miniapp_storage.h"
#include "lisa_kv.h"
#include "easyflash.h"
#include "lisa_mem.h"
#include "listen_system.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* Fixed keys bound total usage, and never expose system KV keys to Lua. */
typedef struct {
    uint32_t magic;
    uint32_t size;
    int64_t expires;
    char id[MINIAPP_ID_MAX + 1];
    char data[MINIAPP_STORAGE_MAX_BYTES + 1];
} miniapp_record_t;
#define RECORD_MAGIC 0x4d415031u
static TickType_t s_written_at[MINIAPP_STORAGE_MAX_APPS];
static bool s_written[MINIAPP_STORAGE_MAX_APPS];

static void slot_key(int slot, char *key, size_t size)
{
    snprintf(key, size, "miniapp.save.%d", slot);
}

static int64_t storage_now(void)
{
    struct timeval tv;
    return ls_sys_time_is_valid() && ls_sys_get_time(&tv) == 0 ? (int64_t)tv.tv_sec : 0;
}

static miniapp_record_t *read_slot(int slot, bool *failed)
{
    char key[24];
    size_t len = 0;
    slot_key(slot, key, sizeof(key));
    ef_get_env_blob(key, NULL, 0, &len);
    if (!len) return NULL;
    if (len != sizeof(miniapp_record_t)) {
        if (lisa_kv_del(key) != 0) *failed = true;
        return NULL;
    }
    miniapp_record_t *record = lisa_mem_alloc(sizeof(*record));
    if (!record) { *failed = true; return NULL; }
    if (ef_get_env_blob(key, record, sizeof(*record), NULL) != sizeof(*record)) {
        lisa_mem_free(record);
        *failed = true;
        return NULL;
    }
    int64_t now = storage_now();
    if (len != sizeof(*record) || record->magic != RECORD_MAGIC ||
        record->size > MINIAPP_STORAGE_MAX_BYTES ||
        !memchr(record->id, 0, sizeof(record->id)) ||
        record->data[record->size] != 0 || record->expires <= 0 ||
        (now && record->expires <= now)) {
        lisa_mem_free(record);
        if (lisa_kv_del(key) != 0) *failed = true;
        return NULL;
    }
    return record;
}

int miniapp_storage_load(const char *id, char *data, size_t *size)
{
    if (!storage_now()) return 0;
    for (int slot = 0; slot < MINIAPP_STORAGE_MAX_APPS; ++slot) {
        bool failed = false;
        miniapp_record_t *record = read_slot(slot, &failed);
        if (failed) return 0;
        if (!record) continue;
        bool match = strcmp(record->id, id) == 0;
        if (match) {
            *size = record->size;
            memcpy(data, record->data, record->size + 1);
        }
        lisa_mem_free(record);
        if (match) return 1;
    }
    return 0;
}

const char *miniapp_storage_save(const char *id, const char *data, size_t size, uint32_t ttl)
{
    int64_t now = storage_now();
    if (!now) return "clock_unavailable";
    if (size > MINIAPP_STORAGE_MAX_BYTES || !ttl || ttl > MINIAPP_STORAGE_MAX_TTL) return "invalid_data";
    int target = -1;
    for (int slot = 0; slot < MINIAPP_STORAGE_MAX_APPS; ++slot) {
        bool failed = false;
        miniapp_record_t *record = read_slot(slot, &failed);
        if (failed) return "io_error";
        if (!record) {
            if (target < 0) target = slot;
            continue;
        }
        bool match = strcmp(record->id, id) == 0;
        lisa_mem_free(record);
        if (match) { target = slot; break; }
    }
    if (target < 0) return "storage_full";
    TickType_t ticks = xTaskGetTickCount();
    if (s_written[target] && ticks - s_written_at[target] < pdMS_TO_TICKS(MINIAPP_STORAGE_WRITE_INTERVAL_MS)) {
        return "rate_limited";
    }
    miniapp_record_t *record = lisa_mem_alloc(sizeof(*record));
    if (!record) return "no_memory";
    memset(record, 0, sizeof(*record));
    record->magic = RECORD_MAGIC;
    record->size = size;
    record->expires = now + ttl;
    snprintf(record->id, sizeof(record->id), "%s", id);
    memcpy(record->data, data, size);
    char key[24];
    slot_key(target, key, sizeof(key));
    int result = lisa_kv_set_blob(key, (uint8_t *)record, sizeof(*record));
    lisa_mem_free(record);
    if (result != 0) return "io_error";
    s_written[target] = true;
    s_written_at[target] = ticks;
    return NULL;
}

const char *miniapp_storage_clear(const char *id)
{
    for (int slot = 0; slot < MINIAPP_STORAGE_MAX_APPS; ++slot) {
        bool failed = false;
        miniapp_record_t *record = read_slot(slot, &failed);
        if (failed) return "io_error";
        if (!record) continue;
        bool match = strcmp(record->id, id) == 0;
        lisa_mem_free(record);
        if (match) {
            char key[24];
            slot_key(slot, key, sizeof(key));
            return lisa_kv_del(key) == 0 ? NULL : "io_error";
        }
    }
    return NULL;
}
