#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA256_C) && defined(MBEDTLS_SHA256_ALT)

#include <string.h>

#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"

#include "csk_common.h"

#define SHA256_CHECK_RET(cond, ret)    CSK_CRYPTO_CHECK_RET(cond, ret)
#define SHA256_ASSERT(cond)            CSK_CRYPTO_ASSERT(cond)

static const uint8_t sha256_zero_length_input_hash[32] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
    0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
    0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
    0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

static const uint8_t sha224_zero_length_input_hash[28] = {
    0xd1, 0x4a, 0x02, 0x8c, 0x2a, 0x3a, 0x2b, 0xc9,
    0x47, 0x61, 0x02, 0xbb, 0x28, 0x82, 0x34, 0xc4,
    0x15, 0xa2, 0xb0, 0x1f, 0x82, 0x8e, 0xa6, 0x2a,
    0xc5, 0xb3, 0xe4, 0x2f,
};

void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    SHA256_ASSERT(ctx != NULL);

    memset(ctx, 0, sizeof(mbedtls_sha256_context));
}

void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha256_context));
}

void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
                          const mbedtls_sha256_context *src)
{
    SHA256_ASSERT(dst != NULL);
    SHA256_ASSERT(src != NULL);

    *dst = *src;
}

/*
 * SHA-256 context setup
 */
int mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int is224)
{
    SHA256_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);
    SHA256_CHECK_RET(is224 == 0 || is224 == 1, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);

    ctx->total[0] = 0;
    ctx->total[1] = 0;

    memset(ctx, 0, sizeof(mbedtls_sha256_context));

    ctx->is224 = is224;

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_starts(mbedtls_sha256_context *ctx,
                           int is224)
{
    mbedtls_sha256_starts_ret(ctx, is224);
}
#endif


int mbedtls_internal_sha256_process(mbedtls_sha256_context *ctx,
                                    const unsigned char data[64])
{
    SHA256_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);
    SHA256_CHECK_RET((const unsigned char *) data != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);

    if (ctx->sha_state == CSK_SHA256_STATE_INIT) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)data, 64, (uint32_t*)NULL, 0);
        // mbedtls_printf("first sha block===========\n");
        ctx->sha_state = CSK_SHA256_STATE_PROCESS;
    }else if (ctx->sha_state == CSK_SHA256_STATE_PROCESS) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)data, 64, (uint32_t*)NULL, 1);
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_process(mbedtls_sha256_context *ctx,
                            const unsigned char data[64])
{
    mbedtls_internal_sha256_process(ctx, data);
}
#endif

/*
 * SHA-256 process buffer
 */
int mbedtls_sha256_update_ret(mbedtls_sha256_context *ctx,
                              const unsigned char *input,
                              size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t fill;
    uint32_t left;

    SHA256_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);
    SHA256_CHECK_RET(ilen == 0 || input != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);

    if (ilen == 0) {
        return 0;
    }

    if (ctx->state[0] == 0) {
        ctx->crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_SHA);
        // SHA256_CHECK_RET(ctx->crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

        if (ctx->is224 == 0) {
            CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);
        } else {
            CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA224);
        }

        ctx->state[0] = 1;
    }

    /* 处理之前总长度正好是64整数倍的情况， 先处理上一次剩余的64字节 */
    if ( ((ctx->total[0] % 64) == 0) && ((ctx->total[0] > 0) || (ctx->total[1] > 0))) {
        if ((ret = mbedtls_internal_sha256_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }
    }

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += (uint32_t) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (uint32_t) ilen) {
        ctx->total[1]++;
    }

    if (left && ilen > fill) {
        memcpy((void *) (ctx->buffer + left), input, fill);

        if ((ret = mbedtls_internal_sha256_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }

        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while (ilen > 64) {
        if ((ret = mbedtls_internal_sha256_process(ctx, input)) != 0) {
            return ret;
        }

        input += 64;
        ilen  -= 64;
    }

    if (ilen > 0) {
        memcpy((void *) (ctx->buffer + left), input, ilen);
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_update(mbedtls_sha256_context *ctx,
                           const unsigned char *input,
                           size_t ilen)
{
    mbedtls_sha256_update_ret(ctx, input, ilen);
}
#endif

/*
 * SHA-256 final digest
 */
int mbedtls_sha256_finish_ret(mbedtls_sha256_context *ctx,
                              unsigned char output[32])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    SHA256_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);
    SHA256_CHECK_RET((unsigned char *) output != NULL, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA);

    bool update = false;
    int left = 0;
    if ( ctx->total[1] > 0 || ctx->total[0] > 64) { // 必须是大于64，不能等于64
        update = true;
    }else {
        update = false;
    }

    left = ctx->total[0] & 0x3f;
    if (left == 0) {
        if ( ctx->total[1] > 0 || ctx->total[0] >0) {
            left = 64;
        }else{
            left = 0;
            // mbedtls_printf("left:%d, update: %d\n", left, update);
        }
    }

    if (left != 0) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)ctx->buffer, left, (uint32_t*)output, update);
    } else {
        /* 软件来处理输入长度为0的情况 */
        if (ctx->is224 == 0) {
            memcpy(output, sha256_zero_length_input_hash, 32);
        } else {
            memcpy(output, sha224_zero_length_input_hash, 28);
        }
    }

    if (ctx->state[0] == 1) {
        ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_SHA);
        SHA256_CHECK_RET(ret == 0, CSK_CRYPTO_ERR_HARDWARE_RELEASED);
        ctx->state[0] = 0;
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
                           unsigned char output[32])
{
    mbedtls_sha256_finish_ret(ctx, output);
}
#endif

#endif /* defined(MBEDTLS_SHA256_C) && defined(MBEDTLS_SHA256_ALT) */
