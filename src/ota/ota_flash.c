#define TAG "ota_flash"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lisa_log.h"
#include "mbedtls/md5.h"
#include "arcs_flash.h"
#include "ef_def.h"

#include "ota_flash.h"
#include "ota_api.h"

#define SIZE_K(x) ((x) * 1024)

typedef struct {
    uint32_t addr;
    uint32_t size;
} ota_partition_t;

static const ota_partition_t partition_map[] = {
    [OTA_PART_WAKE_WORD_BIN] =
        {
            .addr = 0x00200000,
            .size = SIZE_K(2048),
        },
    [OTA_PART_GREETING_MP3] =
        {
            .addr = 0x00F80000,
            .size = SIZE_K(32),
        },
    [OTA_PART_TONE_BIN] =
        {
            .addr = 0x00F88000,
            .size = SIZE_K(448),
        },
};

int ota_flash_init(void)
{
    return 0;
}

int ota_flash_verify(ota_partition_id_e part, const char *md5, uint32_t size)
{
    char calc_md5[OTA_RES_MD5_LEN];
    const ota_partition_t *partition = &partition_map[part];

    if (size > partition->size) {
        LISA_LOGE(TAG, "Partition %d size too large: %u > %u", part, size, partition->size);
        return -1;
    }

    mbedtls_md5((const uint8_t *)(FLASH_ADDR_BASE + partition->addr), size, calc_md5);
    LISA_LOGI(TAG, "Partition %d MD5 actual: " MD5_PRI, part, MD5_ARG(calc_md5));
    LISA_LOGI(TAG, "Partition %d MD5 expect: " MD5_PRI, part, MD5_ARG(md5));

    int mismatch = memcmp(md5, calc_md5, OTA_RES_MD5_LEN) != 0;
    LISA_LOGI(TAG, "Partition %d %s", part, mismatch ? "mismatch" : "match");

    return mismatch;
}

int ota_flash_get(ota_partition_id_e part, const void **data, uint32_t *size)
{
    const ota_partition_t *partition = &partition_map[part];

    if (data) {
        *data = (const void *)(FLASH_ADDR_BASE + partition->addr);
    }

    if (size) {
        *size = partition->size;
    }

    return 0;
}

extern EfErrCode ef_port_erase(uint32_t addr, size_t size);
extern EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size);
extern void ef_port_env_lock(void);
extern void ef_port_env_unlock(void);

static struct {
    bool in_progress;
    ota_partition_id_e part;
    uint32_t offset;
    uint32_t total_size;
    uint32_t buffer_len;
    uint8_t buffer[4096];
} flash_update = {.in_progress = false};

int ota_flash_update_begin(ota_partition_id_e part, uint32_t total_size)
{
    const ota_partition_t *partition = &partition_map[part];

    if (total_size > partition->size) {
        LISA_LOGE(TAG, "Partition %d size too small: %u < %u", part, partition->size, total_size);
        return -1;
    }

    ef_port_env_lock();

    EfErrCode err = ef_port_erase(FLASH_ADDR_BASE + partition->addr, partition->size);
    if (err != EF_NO_ERR) {
        LISA_LOGE(TAG, "Erase partition %d failed (%d)", part, err);
        ef_port_env_unlock();
        return -1;
    }

    flash_update.in_progress = true;
    flash_update.part = part;
    flash_update.offset = 0;
    flash_update.total_size = total_size;
    flash_update.buffer_len = 0;

    ef_port_env_unlock();

    return 0;
}

int ota_flash_update_step(ota_partition_id_e part, uint32_t offset, const uint8_t *data, uint32_t size)
{
    const ota_partition_t *partition = &partition_map[part];

    if (!flash_update.in_progress || flash_update.part != part) {
        LISA_LOGE(TAG, "Update for partition %d not in progress", part);
        return -1;
    }

    if (offset != flash_update.offset) {
        LISA_LOGE(TAG, "Unexpected offset %u, expected %u", offset, flash_update.offset);
        return -1;
    }

    if (offset + size > partition->size) {
        LISA_LOGE(TAG, "Write exceeds partition %d size: %u + %u > %u", part, offset, size, partition->size);
        return -1;
    }

    if (size > sizeof(flash_update.buffer)) {
        LISA_LOGE(TAG, "Write size too large: %u > %zu", size, sizeof(flash_update.buffer));
        return -1;
    }

    if (offset + size > flash_update.total_size) {
        LISA_LOGE(TAG, "Write exceeds update size: %u + %u > %u", offset, size, flash_update.total_size);
        return -1;
    }

    LISA_LOGI(TAG, "Updating partition %d at offset %u, size %u, current buffered %u", part, offset, size,
              flash_update.buffer_len);

    const uint8_t *src = data;
    uint32_t remaining = size;

    ef_port_env_lock();

    while (remaining > 0) {
        uint32_t capacity = sizeof(flash_update.buffer) - flash_update.buffer_len;
        uint32_t chunk = remaining < capacity ? remaining : capacity;

        memcpy(flash_update.buffer + flash_update.buffer_len, src, chunk);
        flash_update.buffer_len += chunk;
        flash_update.offset += chunk;
        src += chunk;
        remaining -= chunk;

        if (flash_update.buffer_len == sizeof(flash_update.buffer)) {
            uint32_t write_offset = flash_update.offset - flash_update.buffer_len;

            EfErrCode err = ef_port_write(FLASH_ADDR_BASE + partition->addr + write_offset,
                                          (const uint32_t *)flash_update.buffer, sizeof(flash_update.buffer));
            if (err != EF_NO_ERR) {
                LISA_LOGE(TAG, "Write partition %d failed at offset %u (%d)", part, write_offset, err);
                ef_port_env_unlock();
                return -1;
            }

            flash_update.buffer_len = 0;
            LISA_LOGI(TAG, "Partition %d wrote to offset %u, buffer remaining %u", part,
                      flash_update.offset - flash_update.buffer_len, flash_update.buffer_len);
        }
    }

    ef_port_env_unlock();

    return 0;
}

int ota_flash_update_finish(ota_partition_id_e part)
{
    const ota_partition_t *partition = &partition_map[part];

    if (!flash_update.in_progress || flash_update.part != part) {
        LISA_LOGE(TAG, "Update for partition %d not in progress", part);
        return -1;
    }

    if (flash_update.offset != flash_update.total_size) {
        LISA_LOGE(TAG, "Update incomplete: %u / %u", flash_update.offset, flash_update.total_size);
        return -1;
    }

    if (flash_update.buffer_len > 0) {
        uint32_t write_offset = flash_update.offset - flash_update.buffer_len;

        ef_port_env_lock();

        EfErrCode err = ef_port_write(FLASH_ADDR_BASE + partition->addr + write_offset,
                                      (const uint32_t *)flash_update.buffer, flash_update.buffer_len);

        ef_port_env_unlock();

        if (err != EF_NO_ERR) {
            LISA_LOGE(TAG, "Write partition %d failed at offset %u (%d)", part, write_offset, err);
            return -1;
        }

        flash_update.buffer_len = 0;
    }

    flash_update.in_progress = false;

    return 0;
}
