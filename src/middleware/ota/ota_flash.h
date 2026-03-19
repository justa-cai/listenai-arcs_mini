#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    OTA_PART_WAKE_WORD_BIN,
    OTA_PART_PROMPT_TONE_BIN,
    OTA_PART_EMOJI_BIN,
} ota_partition_id_e;

int ota_flash_verify(ota_partition_id_e part, const char *md5, uint32_t size);

int ota_flash_get(ota_partition_id_e part, const void **data, uint32_t *size);

int ota_flash_update_begin(ota_partition_id_e part, uint32_t total_size);

int ota_flash_update_step(ota_partition_id_e part, uint32_t offset, const uint8_t *data, uint32_t size);

int ota_flash_update_finish(ota_partition_id_e part);
