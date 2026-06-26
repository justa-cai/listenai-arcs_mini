#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    OTA_PART_WAKE_WORD_BIN,
    OTA_PART_PROMPT_TONE_BIN,
    OTA_PART_EMOJI_BIN,
    OTA_PART_APP_STAGING,
} ota_partition_id_e;

/*
 * 传给 ota_flash_update_begin 的 total_size 特殊值：
 * 表示事先不知道实际写入大小（例如 HTTP 下载没给出 Content-Length），
 * 仅按分区物理大小做越界检查，finish 时也不做"写入长度必须等于声明大小"校验。
 */
#define OTA_FLASH_SIZE_UNKNOWN ((uint32_t)-1)

int ota_flash_verify(ota_partition_id_e part, const char *md5, uint32_t size);

int ota_flash_get(ota_partition_id_e part, const void **data, uint32_t *size);

int ota_flash_update_begin(ota_partition_id_e part, uint32_t total_size);

int ota_flash_update_step(ota_partition_id_e part, uint32_t offset, const uint8_t *data, uint32_t size);

/* 成功时返回本次会话累计写入的字节数，失败返回 < 0 */
int ota_flash_update_finish(ota_partition_id_e part);

/*
 * 中止当前分区的 flash 写入会话，释放互斥锁但不刷缓冲区。
 * 用于下载过程中检测到数据错误（如 MD5 不匹配）后放弃本轮写入，
 * 以便安全重试。
 */
int ota_flash_update_abort(ota_partition_id_e part);
