/*
 * pet_save.c - pet state persistence (lisa_kv)
 *
 * One versioned blob ("pet_state") guarded by magic + version + CRC. KV is
 * ini-rewrite based, so writes are throttled by the caller (pet_core saves at
 * most once a minute plus on every action).
 */
#include <string.h>

#include "lisa_kv.h"
#include "lisa_log.h"
#include "lisa_mem.h"

#include "pet_save.h"

#define TAG "pet"

#define PET_SAVE_KEY "pet_state"

_Static_assert(sizeof(pet_persist_t) < 512, "pet save blob should stay small");

void pet_save_write(const pet_persist_t *state)
{
    pet_persist_t copy = *state;
    copy.magic = PET_PERSIST_MAGIC;
    copy.version = PET_PERSIST_VERSION;

    /* CRC over everything except the crc field itself */
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)&copy;
    for (size_t i = 0; i < offsetof(pet_persist_t, crc); i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    copy.crc = crc ^ 0xFFFFFFFFu;

    if (lisa_kv_set_blob(PET_SAVE_KEY, (uint8_t *)&copy, sizeof(copy)) != 0) {
        LISA_LOGE(TAG, "save: kv write failed");
    }
}

bool pet_save_read(pet_persist_t *state)
{
    uint8_t *blob = NULL;
    int len = 0;

    if (lisa_kv_get_blob(PET_SAVE_KEY, &blob, &len) != 0 || !blob ||
        len != sizeof(pet_persist_t)) {
        goto fail;
    }

    pet_persist_t *in = (pet_persist_t *)blob;
    if (in->magic != PET_PERSIST_MAGIC || in->version != PET_PERSIST_VERSION) {
        LISA_LOGW(TAG, "save: magic/version mismatch, starting fresh");
        goto fail;
    }

    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = blob;
    for (size_t i = 0; i < offsetof(pet_persist_t, crc); i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    if ((crc ^ 0xFFFFFFFFu) != in->crc) {
        LISA_LOGW(TAG, "save: crc mismatch, starting fresh");
        goto fail;
    }

    *state = *in;
    lisa_kv_free(blob);
    return true;

fail:
    if (blob) {
        lisa_kv_free(blob);
    }
    return false;
}
