#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA512_C) && defined(MBEDTLS_SHA512_ALT)

#include <string.h>

#include "mbedtls/sha512.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"

#include "csk_common.h"

#define SHA512_CHECK_RET(cond, ret)    CSK_CRYPTO_CHECK_RET(cond, ret)
#define SHA512_ASSERT(cond)            CSK_CRYPTO_ASSERT(cond)

static const uint8_t sha512_zero_length_input_hash[64] = {
    0xcf, 0x83, 0xe1, 0x35, 0x7e, 0xef, 0xb8, 0xbd,
    0xf1, 0x54, 0x28, 0x50, 0xd6, 0x6d, 0x80, 0x07,
    0xd6, 0x20, 0xe4, 0x05, 0x0b, 0x57, 0x15, 0xdc,
    0x83, 0xf4, 0xa9, 0x21, 0xd3, 0x6c, 0xe9, 0xce,
    0x47, 0xd0, 0xd1, 0x3c, 0x5d, 0x85, 0xf2, 0xb0,
    0xff, 0x83, 0x18, 0xd2, 0x87, 0x7e, 0xec, 0x2f,
    0x63, 0xb9, 0x31, 0xbd, 0x47, 0x41, 0x7a, 0x81,
    0xa5, 0x38, 0x32, 0x7a, 0xf9, 0x27, 0xda, 0x3e,
};

static const uint8_t sha384_zero_length_input_hash[48] = {
    0x38, 0xb0, 0x60, 0xa7, 0x51, 0xac, 0x96, 0x38,
    0x4c, 0xd9, 0x32, 0x7e, 0xb1, 0xb1, 0xe3, 0x6a,
    0x21, 0xfd, 0xb7, 0x11, 0x14, 0xbe, 0x07, 0x43,
    0x4c, 0x0c, 0xc7, 0xbf, 0x63, 0xf6, 0xe1, 0xda,
    0x27, 0x4e, 0xde, 0xbf, 0xe7, 0x6f, 0x65, 0xfb,
    0xd5, 0x1a, 0xd2, 0xf1, 0x48, 0x98, 0xb9, 0x5b,
};

void mbedtls_sha512_init(mbedtls_sha512_context *ctx)
{
    SHA512_ASSERT(ctx != NULL);

    memset(ctx, 0, sizeof(mbedtls_sha512_context));
}

void mbedtls_sha512_free(mbedtls_sha512_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha512_context));
}

void mbedtls_sha512_clone(mbedtls_sha512_context *dst,
                          const mbedtls_sha512_context *src)
{
    SHA512_ASSERT(dst != NULL);
    SHA512_ASSERT(src != NULL);

    *dst = *src;
}

/*
 * SHA-512 context setup
 */
int mbedtls_sha512_starts_ret(mbedtls_sha512_context *ctx, int is384)
{
    SHA512_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);
    SHA512_CHECK_RET(is384 == 0 || is384 == 1, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);

    ctx->total[0] = 0;
    ctx->total[1] = 0;

    memset(ctx, 0, sizeof(mbedtls_sha512_context));

    ctx->is384 = is384;

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_starts(mbedtls_sha512_context *ctx,
                           int is384)
{
    mbedtls_sha512_starts_ret(ctx, is384);
}
#endif

#if !defined(MBEDTLS_SHA512_PROCESS_ALT)

int mbedtls_internal_sha512_process(mbedtls_sha512_context *ctx,
                                    const unsigned char data[128])
{
    SHA512_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);
    SHA512_CHECK_RET((const unsigned char *) data != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);

    if (ctx->sha_state == CSK_SHA512_STATE_INIT) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)data, 128, (uint32_t*)NULL, 0);
        // mbedtls_printf("first sha512 block===========\n");
        ctx->sha_state = CSK_SHA512_STATE_PROCESS;
    }else if (ctx->sha_state == CSK_SHA512_STATE_PROCESS) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)data, 128, (uint32_t*)NULL, 1);
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_process(mbedtls_sha512_context *ctx,
                            const unsigned char data[128])
{
    mbedtls_internal_sha512_process(ctx, data);
}
#endif
#endif /* !MBEDTLS_SHA512_PROCESS_ALT */

/*
 * SHA-512 process buffer
 */
int mbedtls_sha512_update_ret(mbedtls_sha512_context *ctx,
                              const unsigned char *input,
                              size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t fill;
    unsigned int left;

    SHA512_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);
    SHA512_CHECK_RET(ilen == 0 || input != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);

    if (ilen == 0) {
        return 0;
    }

    if (ctx->state[0] == 0) {
        ctx->crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_SHA);
        // SHA512_CHECK_RET(ctx->crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

        if (ctx->is384 == 0) {
            CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA512);
        } else {
            CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA384);
        }

        ctx->state[0] = 1;
    }

    /* 处理之前总长度正好是128整数倍的情况， 先处理上一次剩余的128字节 */
    if ( ((ctx->total[0] % 128) == 0) && ((ctx->total[0] > 0) || (ctx->total[1] > 0))) {
        if ((ret = mbedtls_internal_sha512_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }
    }

    left = (unsigned int) (ctx->total[0] & 0x7F);
    fill = 128 - left;

    ctx->total[0] += (uint64_t) ilen;

    if (ctx->total[0] < (uint64_t) ilen) {
        ctx->total[1]++;
    }

    if (left && ilen > fill) {
        memcpy((void *) (ctx->buffer + left), input, fill);

        if ((ret = mbedtls_internal_sha512_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }

        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while (ilen > 128) {
        if ((ret = mbedtls_internal_sha512_process(ctx, input)) != 0) {
            return ret;
        }

        input += 128;
        ilen  -= 128;
    }

    if (ilen > 0) {
        memcpy((void *) (ctx->buffer + left), input, ilen);
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_update(mbedtls_sha512_context *ctx,
                           const unsigned char *input,
                           size_t ilen)
{
    mbedtls_sha512_update_ret(ctx, input, ilen);
}
#endif

/*
 * SHA-512 final digest
 */
int mbedtls_sha512_finish_ret(mbedtls_sha512_context *ctx,
                              unsigned char output[64])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    SHA512_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);
    SHA512_CHECK_RET((unsigned char *) output != NULL, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA);

    bool update = false;
    int left = 0;
    if ( ctx->total[1] > 0 || ctx->total[0] > 128) { // 必须是大于128，不能等于128
        update = true;
    }else {
        update = false;
    }

    left = ctx->total[0] & 0x7f;
    if (left == 0) {
        if ( ctx->total[1] > 0 || ctx->total[0] >0) {
            left = 128;
        }else{
            left = 0;
            // mbedtls_printf("left:%d, update: %d\n", left, update);
        }
    }

    if (left != 0) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)ctx->buffer, left, (uint32_t*)output, update);
    } else {
        /* 软件来处理输入长度为0的情况 */
        if (ctx->is384 == 0) {
            memcpy(output, sha512_zero_length_input_hash, 64);
        } else {
            memcpy(output, sha384_zero_length_input_hash, 48);
        }
    }

    if (ctx->state[0] == 1) {
        ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_SHA);
        SHA512_CHECK_RET(ret == 0, CSK_CRYPTO_ERR_HARDWARE_RELEASED);
        ctx->state[0] = 0;
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_finish(mbedtls_sha512_context *ctx,
                           unsigned char output[64])
{
    mbedtls_sha512_finish_ret(ctx, output);
}
#endif

#endif /* defined(MBEDTLS_SHA512_C) && defined(MBEDTLS_SHA512_ALT) */
