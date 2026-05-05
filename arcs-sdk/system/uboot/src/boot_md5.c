#include "boot_md5.h"

#include <string.h>

static const uint32_t g_boot_md5_shift[64] = {
    7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
    5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U,
    4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
    6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U,
};

static const uint32_t g_boot_md5_table[64] = {
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
    0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
    0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
    0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
    0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
    0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
    0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
    0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
    0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
    0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
    0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
    0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
    0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
    0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
    0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
    0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
};

static uint32_t boot_md5_load_le32(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static void boot_md5_store_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void boot_md5_store_le64(uint8_t *dst, uint64_t value)
{
    size_t i;

    for (i = 0; i < 8U; ++i) {
        dst[i] = (uint8_t)((value >> (i * 8U)) & 0xFFU);
    }
}

static uint32_t boot_md5_rotate_left(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32U - bits));
}

static void boot_md5_process_block(boot_md5_context_t *ctx, const uint8_t block[BOOT_MD5_BLOCK_SIZE])
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t m[16];
    size_t i;

    for (i = 0; i < 16U; ++i) {
        m[i] = boot_md5_load_le32(block + i * 4U);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];

    for (i = 0; i < 64U; ++i) {
        uint32_t f;
        uint32_t g;
        uint32_t next;

        if (i < 16U) {
            f = (b & c) | ((~b) & d);
            g = (uint32_t)i;
        } else if (i < 32U) {
            f = (d & b) | ((~d) & c);
            g = (uint32_t)((i * 5U + 1U) & 0x0FU);
        } else if (i < 48U) {
            f = b ^ c ^ d;
            g = (uint32_t)((i * 3U + 5U) & 0x0FU);
        } else {
            f = c ^ (b | (~d));
            g = (uint32_t)((i * 7U) & 0x0FU);
        }

        next = d;
        d = c;
        c = b;
        b += boot_md5_rotate_left(a + f + g_boot_md5_table[i] + m[g], g_boot_md5_shift[i]);
        a = next;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void boot_md5_init(boot_md5_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xefcdab89U;
    ctx->state[2] = 0x98badcfeU;
    ctx->state[3] = 0x10325476U;
}

void boot_md5_update(boot_md5_context_t *ctx, const uint8_t *data, size_t size)
{
    size_t remaining;
    size_t offset;

    if (ctx == NULL || size == 0U) {
        return;
    }
    if (data == NULL) {
        return;
    }

    ctx->total_size += size;
    offset = 0U;

    if (ctx->buffer_size > 0U) {
        remaining = BOOT_MD5_BLOCK_SIZE - ctx->buffer_size;
        if (remaining > size) {
            remaining = size;
        }

        memcpy(ctx->buffer + ctx->buffer_size, data, remaining);
        ctx->buffer_size += (uint32_t)remaining;
        offset += remaining;

        if (ctx->buffer_size == BOOT_MD5_BLOCK_SIZE) {
            boot_md5_process_block(ctx, ctx->buffer);
            ctx->buffer_size = 0U;
        }
    }

    while (size - offset >= BOOT_MD5_BLOCK_SIZE) {
        boot_md5_process_block(ctx, data + offset);
        offset += BOOT_MD5_BLOCK_SIZE;
    }

    if (offset < size) {
        remaining = size - offset;
        memcpy(ctx->buffer, data + offset, remaining);
        ctx->buffer_size = (uint32_t)remaining;
    }
}

void boot_md5_finish(boot_md5_context_t *ctx, uint8_t digest[BOOT_MD5_DIGEST_SIZE])
{
    uint64_t bit_count;
    size_t used;
    size_t i;

    if (ctx == NULL || digest == NULL) {
        return;
    }

    bit_count = ctx->total_size * 8U;
    used = ctx->buffer_size;
    ctx->buffer[used++] = 0x80U;

    if (used > 56U) {
        memset(ctx->buffer + used, 0, BOOT_MD5_BLOCK_SIZE - used);
        boot_md5_process_block(ctx, ctx->buffer);
        used = 0U;
    }

    memset(ctx->buffer + used, 0, 56U - used);
    boot_md5_store_le64(ctx->buffer + 56U, bit_count);
    boot_md5_process_block(ctx, ctx->buffer);

    for (i = 0; i < 4U; ++i) {
        boot_md5_store_le32(digest + i * 4U, ctx->state[i]);
    }

    memset(ctx, 0, sizeof(*ctx));
}
