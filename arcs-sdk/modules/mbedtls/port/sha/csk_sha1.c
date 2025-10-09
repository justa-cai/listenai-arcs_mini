#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA1_C) && defined(MBEDTLS_SHA1_ALT)

#include <stdint.h>
#include <string.h>

#include "mbedtls/sha1.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"

#include "csk_common.h"

#define SHA1_CHECK_RET(cond, ret)    CSK_CRYPTO_CHECK_RET(cond, ret)
#define SHA1_ASSERT(cond)            CSK_CRYPTO_ASSERT(cond)

static const uint8_t sha1_zero_length_input_hash[20] = {
    0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 
    0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 
    0xaf, 0xd8, 0x07, 0x09,
};

void mbedtls_sha1_init(mbedtls_sha1_context *ctx)
{
    SHA1_ASSERT(ctx != NULL);

    memset(ctx, 0, sizeof(mbedtls_sha1_context));
}

void mbedtls_sha1_free(mbedtls_sha1_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha1_context));
}

void mbedtls_sha1_clone(mbedtls_sha1_context *dst,
                        const mbedtls_sha1_context *src)
{
    SHA1_ASSERT(dst != NULL);
    SHA1_ASSERT(src != NULL);

    *dst = *src;
}

/*
 * SHA-1 context setup
 */
int mbedtls_sha1_starts_ret(mbedtls_sha1_context *ctx)
{
    SHA1_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);

    ctx->total[0] = 0;
    ctx->total[1] = 0;

    memset(ctx, 0, sizeof(mbedtls_sha1_context));

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_starts(mbedtls_sha1_context *ctx)
{
    mbedtls_sha1_starts_ret(ctx);
}
#endif

int mbedtls_internal_sha1_process(mbedtls_sha1_context *ctx,
                                  const unsigned char data[64])
{

    SHA1_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);
    SHA1_CHECK_RET((const unsigned char *) data != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);

    if (ctx->sha_state == CSK_SHA1_STATE_INIT) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)data, 64, (uint32_t*)NULL, 0);
        ctx->sha_state = CSK_SHA1_STATE_PROCESS;
    }else if (ctx->sha_state == CSK_SHA1_STATE_PROCESS) {
        CRYPTO_Hash(ctx->crypto_handler, (uint32_t*)data, 64, (uint32_t*)NULL, 1);
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_process(mbedtls_sha1_context *ctx,
                          const unsigned char data[64])
{
    mbedtls_internal_sha1_process(ctx, data);
}
#endif

/*
 * SHA-1 process buffer
 */
int mbedtls_sha1_update_ret(mbedtls_sha1_context *ctx,
                            const unsigned char *input,
                            size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t fill;
    uint32_t left;

    SHA1_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);
    SHA1_CHECK_RET(ilen == 0 || input != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);

    if (ilen == 0) {
        return 0;
    }

    if (ctx->state[0] == 0) {
        ctx->crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_SHA);
        // SHA1_CHECK_RET(ctx->crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

        CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA1);
        ctx->state[0] = 1;
    }

    /* 处理之前总长度正好是64整数倍的情况， 先处理上一次剩余的64字节 */
    if ( ((ctx->total[0] % 64) == 0) && ((ctx->total[0] > 0) || (ctx->total[1] > 0))) {
        if ((ret = mbedtls_internal_sha1_process(ctx, ctx->buffer)) != 0) {
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

    if (left) {
        if ( ilen > fill ) { // 必须是大于fill，不能等于fill
            memcpy((void *) (ctx->buffer + left), input, fill);

            if ((ret = mbedtls_internal_sha1_process(ctx, ctx->buffer)) != 0) {
                return ret;
            }

            input += fill;
            ilen  -= fill;
            left = 0;
        }
    }

    while (ilen > 64) { // 必须是大于64，不能等于64
        if ((ret = mbedtls_internal_sha1_process(ctx, input)) != 0) {
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
void mbedtls_sha1_update(mbedtls_sha1_context *ctx,
                         const unsigned char *input,
                         size_t ilen)
{
    mbedtls_sha1_update_ret(ctx, input, ilen);
}
#endif

/*
 * SHA-1 final digest
 */
int mbedtls_sha1_finish_ret(mbedtls_sha1_context *ctx,
                            unsigned char output[20])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    SHA1_CHECK_RET(ctx != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);
    SHA1_CHECK_RET((unsigned char *) output != NULL, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA);

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
        memcpy(output, sha1_zero_length_input_hash, 20);
    }

    if (ctx->state[0] == 1) {
        ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_SHA);
        SHA1_CHECK_RET(ret == 0, CSK_CRYPTO_ERR_HARDWARE_RELEASED);
        ctx->state[0] = 0;
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_finish(mbedtls_sha1_context *ctx,
                         unsigned char output[20])
{
    mbedtls_sha1_finish_ret(ctx, output);
}
#endif

#endif /* MBEDTLS_SHA1_C && MBEDTLS_SHA1_ALT */