#ifndef __BOOT_MD5_H__
#define __BOOT_MD5_H__

#include <stddef.h>
#include <stdint.h>

#define BOOT_MD5_DIGEST_SIZE 16U
#define BOOT_MD5_BLOCK_SIZE 64U

typedef struct {
    uint32_t state[4];
    uint64_t total_size;
    uint32_t buffer_size;
    uint8_t buffer[BOOT_MD5_BLOCK_SIZE];
} boot_md5_context_t;

void boot_md5_init(boot_md5_context_t *ctx);
void boot_md5_update(boot_md5_context_t *ctx, const uint8_t *data, size_t size);
void boot_md5_finish(boot_md5_context_t *ctx, uint8_t digest[BOOT_MD5_DIGEST_SIZE]);

#endif /* __BOOT_MD5_H__ */
