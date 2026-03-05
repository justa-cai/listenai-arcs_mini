#define TAG "ota_flash"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "lisa_log.h"
#include "mbedtls/md5.h"
#include "lisa_flash.h"
#include "arcs_ap_base.h"

#include "ota_flash.h"
#include "ota_api.h"

#define SIZE_K(x) ((x) * 1024)
#define SIZE_M(x) (SIZE_K(x) * 1024)

typedef struct {
    uint32_t addr;
    uint32_t size;
} ota_partition_t;

static const ota_partition_t partition_map[] = {
    [OTA_PART_WAKE_WORD_BIN] =
        {
            .addr = 0x00200000,
            .size = SIZE_M(2),
        },
    [OTA_PART_PROMPT_TONE_BIN] =
        {
            .addr = 0x00100000,
            .size = SIZE_M(1),
        },
};

int ota_flash_verify(ota_partition_id_e part, const char *md5, uint32_t size)
{
    char calc_md5[OTA_RES_MD5_LEN];
    const ota_partition_t *partition = &partition_map[part];

    if (size > partition->size) {
        LISA_LOGE(TAG, "Partition %d size too large: %u > %u", part, size, partition->size);
        return -1;
    }

    mbedtls_md5((const uint8_t *)(CMN_FLASH_REGION + partition->addr), size, calc_md5);
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
        *data = (const void *)(CMN_FLASH_REGION + partition->addr);
    }

    if (size) {
        *size = partition->size;
    }

    return 0;
}

static struct {
    bool in_progress;
    SemaphoreHandle_t mutex;
    lisa_device_t *flash;
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

    lisa_device_t *flash = lisa_device_get("flash0");
    if (!flash) {
        LISA_LOGE(TAG, "Flash device not found");
        return -1;
    }

    flash_update.in_progress = true;
    flash_update.mutex = xSemaphoreCreateMutex();
    flash_update.flash = flash;
    flash_update.part = part;
    flash_update.offset = 0;
    flash_update.total_size = total_size;
    flash_update.buffer_len = 0;

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

    xSemaphoreTake(flash_update.mutex, portMAX_DELAY);

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
            int ret;

            ret = lisa_flash_erase(flash_update.flash, partition->addr + write_offset, sizeof(flash_update.buffer));
            if (ret != 0) {
                LISA_LOGE(TAG, "Erase partition %d failed at offset %u (%d)", part, write_offset, ret);
                xSemaphoreGive(flash_update.mutex);
                return -1;
            }

            ret = lisa_flash_write(flash_update.flash, partition->addr + write_offset, flash_update.buffer,
                                   sizeof(flash_update.buffer));
            if (ret != 0) {
                LISA_LOGE(TAG, "Write partition %d failed at offset %u (%d)", part, write_offset, ret);
                xSemaphoreGive(flash_update.mutex);
                return -1;
            }

            flash_update.buffer_len = 0;
            LISA_LOGI(TAG, "Partition %d wrote to offset %u, buffer remaining %u", part,
                      flash_update.offset - flash_update.buffer_len, flash_update.buffer_len);
        }
    }

    xSemaphoreGive(flash_update.mutex);

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
        int ret;

        xSemaphoreTake(flash_update.mutex, portMAX_DELAY);

        ret = lisa_flash_erase(flash_update.flash, partition->addr + write_offset, sizeof(flash_update.buffer));
        if (ret != 0) {
            LISA_LOGE(TAG, "Erase partition %d failed at offset %u (%d)", part, write_offset, ret);
            xSemaphoreGive(flash_update.mutex);
            return -1;
        }

        ret = lisa_flash_write(flash_update.flash, partition->addr + write_offset, flash_update.buffer,
                               flash_update.buffer_len);
        if (ret != 0) {
            LISA_LOGE(TAG, "Write partition %d failed at offset %u (%d)", part, write_offset, ret);
            xSemaphoreGive(flash_update.mutex);
            return -1;
        }

        xSemaphoreGive(flash_update.mutex);

        flash_update.buffer_len = 0;
    }

    vSemaphoreDelete(flash_update.mutex);
    flash_update.in_progress = false;

    return 0;
}
