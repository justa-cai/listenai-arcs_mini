#include "common.h"

#include <string.h>

#include "mbedtls/gcm.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/constant_time.h"

#if defined(MBEDTLS_AESNI_C)
#include "mbedtls/aesni.h"
#endif

#include "csk_common.h"

#define AES_GCM_STANDARD_IV_LEN 12

/* Parameter validation macros */
#define GCM_VALIDATE_RET(cond) \
    CSK_CRYPTO_CHECK_RET(cond, MBEDTLS_ERR_GCM_BAD_INPUT)
#define GCM_VALIDATE(cond) \
    CSK_CRYPTO_ASSERT(cond)

/*
 * Initialize a context
 */
void mbedtls_gcm_init(mbedtls_gcm_context *ctx)
{
    GCM_VALIDATE(ctx != NULL);
    memset(ctx, 0, sizeof(mbedtls_gcm_context));
}

/*
 * Precompute small multiples of H, that is set
 *      HH[i] || HL[i] = H times i,
 * where i is seen as a field element as in [MGV], ie high-order bits
 * correspond to low powers of P. The result is stored in the same way, that
 * is the high-order bit of HH corresponds to P^0 and the low-order bit of HL
 * corresponds to P^127.
 */
static int gcm_gen_table(mbedtls_gcm_context *ctx)
{
    int ret, i, j;
    uint64_t hi, lo;
    uint64_t vl, vh;
    unsigned char h[16];
    size_t olen = 0;

    memset(h, 0, 16);
    if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, h, 16, h, &olen)) != 0) {
        return ret;
    }

    /* pack h as two 64-bits ints, big-endian */
    hi = MBEDTLS_GET_UINT32_BE(h,  0);
    lo = MBEDTLS_GET_UINT32_BE(h,  4);
    vh = (uint64_t) hi << 32 | lo;

    hi = MBEDTLS_GET_UINT32_BE(h,  8);
    lo = MBEDTLS_GET_UINT32_BE(h,  12);
    vl = (uint64_t) hi << 32 | lo;

    /* 8 = 1000 corresponds to 1 in GF(2^128) */
    ctx->HL[8] = vl;
    ctx->HH[8] = vh;

#if defined(MBEDTLS_AESNI_HAVE_CODE)
    /* With CLMUL support, we need only h, not the rest of the table */
    if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
        return 0;
    }
#endif

    /* 0 corresponds to 0 in GF(2^128) */
    ctx->HH[0] = 0;
    ctx->HL[0] = 0;

    for (i = 4; i > 0; i >>= 1) {
        uint32_t T = (vl & 1) * 0xe1000000U;
        vl  = (vh << 63) | (vl >> 1);
        vh  = (vh >> 1) ^ ((uint64_t) T << 32);

        ctx->HL[i] = vl;
        ctx->HH[i] = vh;
    }

    for (i = 2; i <= 8; i *= 2) {
        uint64_t *HiL = ctx->HL + i, *HiH = ctx->HH + i;
        vh = *HiH;
        vl = *HiL;
        for (j = 1; j < i; j++) {
            HiH[j] = vh ^ ctx->HH[j];
            HiL[j] = vl ^ ctx->HL[j];
        }
    }

    return 0;
}

int mbedtls_gcm_setkey(mbedtls_gcm_context *ctx,
                       mbedtls_cipher_id_t cipher,
                       const unsigned char *key,
                       unsigned int keybits)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const mbedtls_cipher_info_t *cipher_info;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(key != NULL);
    GCM_VALIDATE_RET(keybits == 128 || keybits == 192 || keybits == 256);

    cipher_info = mbedtls_cipher_info_from_values(cipher, keybits,
                                                  MBEDTLS_MODE_ECB);
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    if (cipher_info->block_size != 16) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    mbedtls_cipher_free(&ctx->cipher_ctx);

    if ((ret = mbedtls_cipher_setup(&ctx->cipher_ctx, cipher_info)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_cipher_setkey(&ctx->cipher_ctx, key, keybits,
                                    MBEDTLS_ENCRYPT)) != 0) {
        return ret;
    }

    if ((ret = gcm_gen_table(ctx)) != 0) {
        return ret;
    }

    if ( cipher == MBEDTLS_CIPHER_ID_AES ) {
        memset(&ctx->aes_ctx, 0, sizeof(csk_gcm_aes_context_t));

        ctx->aes_ctx.key_bytes = keybits / 8;
        memcpy(ctx->aes_ctx.key, key, ctx->aes_ctx.key_bytes);

        ctx->aes_ctx.is_aes = 1;
    }

    return 0;
}

/*
 * Shoup's method for multiplication use this table with
 *      last4[x] = x times P^128
 * where x and last4[x] are seen as elements of GF(2^128) as in [MGV]
 */
static const uint64_t last4[16] =
{
    0x0000, 0x1c20, 0x3840, 0x2460,
    0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560,
    0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

/*
 * Sets output to x times H using the precomputed tables.
 * x and output are seen as elements of GF(2^128) as in [MGV].
 */
static void gcm_mult(mbedtls_gcm_context *ctx, const unsigned char x[16],
                     unsigned char output[16])
{
    int i = 0;
    unsigned char lo, hi, rem;
    uint64_t zh, zl;

#if defined(MBEDTLS_AESNI_HAVE_CODE)
    if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
        unsigned char h[16];

        MBEDTLS_PUT_UINT32_BE(ctx->HH[8] >> 32, h,  0);
        MBEDTLS_PUT_UINT32_BE(ctx->HH[8],       h,  4);
        MBEDTLS_PUT_UINT32_BE(ctx->HL[8] >> 32, h,  8);
        MBEDTLS_PUT_UINT32_BE(ctx->HL[8],       h, 12);

        mbedtls_aesni_gcm_mult(output, x, h);
        return;
    }
#endif /* MBEDTLS_AESNI_HAVE_CODE */

    lo = x[15] & 0xf;

    zh = ctx->HH[lo];
    zl = ctx->HL[lo];

    for (i = 15; i >= 0; i--) {
        lo = x[i] & 0xf;
        hi = (x[i] >> 4) & 0xf;

        if (i != 15) {
            rem = (unsigned char) zl & 0xf;
            zl = (zh << 60) | (zl >> 4);
            zh = (zh >> 4);
            zh ^= (uint64_t) last4[rem] << 48;
            zh ^= ctx->HH[lo];
            zl ^= ctx->HL[lo];

        }

        rem = (unsigned char) zl & 0xf;
        zl = (zh << 60) | (zl >> 4);
        zh = (zh >> 4);
        zh ^= (uint64_t) last4[rem] << 48;
        zh ^= ctx->HH[hi];
        zl ^= ctx->HL[hi];
    }

    MBEDTLS_PUT_UINT32_BE(zh >> 32, output, 0);
    MBEDTLS_PUT_UINT32_BE(zh, output, 4);
    MBEDTLS_PUT_UINT32_BE(zl >> 32, output, 8);
    MBEDTLS_PUT_UINT32_BE(zl, output, 12);
}

int mbedtls_gcm_starts(mbedtls_gcm_context *ctx,
                       int mode,
                       const unsigned char *iv,
                       size_t iv_len,
                       const unsigned char *add,
                       size_t add_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char work_buf[16];
    size_t i;
    const unsigned char *p;
    size_t use_len, olen = 0;
    uint64_t iv_bits;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(iv_len == 0 || iv != NULL);
    GCM_VALIDATE_RET(add_len == 0 || add != NULL);

    /* IV and AD are limited to 2^64 bits, so 2^61 bytes */
    /* IV is not allowed to be zero length */
    if (iv_len == 0 ||
        ((uint64_t) iv_len) >> 61 != 0 ||
        ((uint64_t) add_len) >> 61 != 0) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    memset(ctx->y, 0x00, sizeof(ctx->y));
    memset(ctx->buf, 0x00, sizeof(ctx->buf));

    ctx->mode = mode;
    ctx->len = 0;
    ctx->add_len = 0;

    if (iv_len == 12) {
        memcpy(ctx->y, iv, iv_len);
        ctx->y[15] = 1;
    } else {
        memset(work_buf, 0x00, 16);
        iv_bits = (uint64_t) iv_len * 8;
        MBEDTLS_PUT_UINT64_BE(iv_bits, work_buf, 8);

        p = iv;
        while (iv_len > 0) {
            use_len = (iv_len < 16) ? iv_len : 16;

            for (i = 0; i < use_len; i++) {
                ctx->y[i] ^= p[i];
            }

            gcm_mult(ctx, ctx->y, ctx->y);

            iv_len -= use_len;
            p += use_len;
        }

        for (i = 0; i < 16; i++) {
            ctx->y[i] ^= work_buf[i];
        }

        gcm_mult(ctx, ctx->y, ctx->y);
    }

    if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16,
                                     ctx->base_ectr, &olen)) != 0) {
        return ret;
    }

    ctx->add_len = add_len;
    p = add;
    while (add_len > 0) {
        use_len = (add_len < 16) ? add_len : 16;

        for (i = 0; i < use_len; i++) {
            ctx->buf[i] ^= p[i];
        }

        gcm_mult(ctx, ctx->buf, ctx->buf);

        add_len -= use_len;
        p += use_len;
    }

    return 0;
}

int mbedtls_gcm_update(mbedtls_gcm_context *ctx,
                       size_t length,
                       const unsigned char *input,
                       unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char ectr[16];
    size_t i;
    const unsigned char *p;
    unsigned char *out_p = output;
    size_t use_len, olen = 0;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(length == 0 || input != NULL);
    GCM_VALIDATE_RET(length == 0 || output != NULL);

    if (output > input && (size_t) (output - input) < length) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    /* Total length is restricted to 2^39 - 256 bits, ie 2^36 - 2^5 bytes
     * Also check for possible overflow */
    if (ctx->len + length < ctx->len ||
        (uint64_t) ctx->len + length > 0xFFFFFFFE0ull) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    ctx->len += length;

    p = input;
    while (length > 0) {
        use_len = (length < 16) ? length : 16;

        for (i = 16; i > 12; i--) {
            if (++ctx->y[i - 1] != 0) {
                break;
            }
        }

        if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ectr,
                                         &olen)) != 0) {
            return ret;
        }

        for (i = 0; i < use_len; i++) {
            if (ctx->mode == MBEDTLS_GCM_DECRYPT) {
                ctx->buf[i] ^= p[i];
            }
            out_p[i] = ectr[i] ^ p[i];
            if (ctx->mode == MBEDTLS_GCM_ENCRYPT) {
                ctx->buf[i] ^= out_p[i];
            }
        }

        gcm_mult(ctx, ctx->buf, ctx->buf);

        length -= use_len;
        p += use_len;
        out_p += use_len;
    }

    return 0;
}

int mbedtls_gcm_finish(mbedtls_gcm_context *ctx,
                       unsigned char *tag,
                       size_t tag_len)
{
    unsigned char work_buf[16];
    size_t i;
    uint64_t orig_len;
    uint64_t orig_add_len;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(tag != NULL);

    orig_len = ctx->len * 8;
    orig_add_len = ctx->add_len * 8;

    if (tag_len > 16 || tag_len < 4) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    memcpy(tag, ctx->base_ectr, tag_len);

    if (orig_len || orig_add_len) {
        memset(work_buf, 0x00, 16);

        MBEDTLS_PUT_UINT32_BE((orig_add_len >> 32), work_buf, 0);
        MBEDTLS_PUT_UINT32_BE((orig_add_len), work_buf, 4);
        MBEDTLS_PUT_UINT32_BE((orig_len     >> 32), work_buf, 8);
        MBEDTLS_PUT_UINT32_BE((orig_len), work_buf, 12);

        for (i = 0; i < 16; i++) {
            ctx->buf[i] ^= work_buf[i];
        }

        gcm_mult(ctx, ctx->buf, ctx->buf);

        for (i = 0; i < tag_len; i++) {
            tag[i] ^= ctx->buf[i];
        }
    }

    return 0;
}

static void csk_aes_crypto_setkey( csk_gcm_aes_context_t *ctx )
{
    const unsigned char *key = ctx->key;
    unsigned int keybits = ctx->key_bytes * 8;
    if (keybits == 128) {
        CRYPTO_Control(ctx->crypto_handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    } else if (keybits == 192) { 
        CRYPTO_Control(ctx->crypto_handler,CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    } else if (keybits == 256) {
        CRYPTO_Control(ctx->crypto_handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    } else {
        mbedtls_printf("Invalid key size: %d\n", keybits);
        return;
    }
    // csk_dump_buf("gcm,key\n", key, ctx->key_bytes);

    CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_GCM);

    CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)key);
}

static int cks_aes_gcm_crypt_and_tag(mbedtls_gcm_context *ctx,
                              int mode,
                              size_t length,
                              const unsigned char *iv,
                              size_t iv_len,
                              const unsigned char *add,
                              size_t add_len,
                              const unsigned char *input,
                              unsigned char *output,
                              size_t tag_len,
                              unsigned char *tag)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    csk_gcm_aes_context_t *aes_ctx = &ctx->aes_ctx;
    uint32_t aes_length[4] = {0};
    uint8_t tag_out[16] = {0};
    uint8_t *in = NULL;
    uint8_t *out = NULL;
    uint8_t *add_1 = NULL;

    unsigned char work_buf[16];
    size_t i;
    const unsigned char *p;
    size_t use_len = 0;
    uint64_t iv_bits;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(iv_len == 0 || iv != NULL);
    GCM_VALIDATE_RET(add_len == 0 || add != NULL);

    /* IV and AD are limited to 2^64 bits, so 2^61 bytes */
    /* IV is not allowed to be zero length */
    if (iv_len == 0 ||
        ((uint64_t) iv_len) >> 61 != 0 ||
        ((uint64_t) add_len) >> 61 != 0) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(length == 0 || input != NULL);
    GCM_VALIDATE_RET(length == 0 || output != NULL);

    if (output > input && (size_t) (output - input) < length) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    /* Total length is restricted to 2^39 - 256 bits, ie 2^36 - 2^5 bytes
     * Also check for possible overflow */
    if (ctx->len + length < ctx->len ||
        (uint64_t) ctx->len + length > 0xFFFFFFFE0ull) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(tag != NULL);

    if (tag_len > 16 || tag_len < 4) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    aes_ctx->crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_AES);
    CSK_CRYPTO_CHECK_RET(aes_ctx->crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

    uint32_t in_malloc_len = 0;
    uint32_t add_malloc_len = 0;

    aes_length[0] = tag_len;
    aes_length[1] = AES_GCM_STANDARD_IV_LEN;
    aes_length[2] = add_len;
    aes_length[3] = length;

    // mbedtls_printf("\naes-gcm length: tag:%d iv:%d add:%d in:%d\n", aes_length[0], aes_length[1], aes_length[2], aes_length[3]);
    // mbedtls_printf("\naes-gcm mode: %d, input: %p, iv: %p, add: %p, output: %p\n", mode, input, iv, add, output);

    in_malloc_len = (aes_length[3] / 32 + 1) * 32;          // 长度是32字节对齐，用来避免cache操作出错; 超过input的长度,方便后续操作
    in = mbedtls_calloc(1, (in_malloc_len));    // mbedtls_calloc分配的地址时会32字节对齐
    GCM_VALIDATE(in != NULL);
    memcpy(in, input, aes_length[3]);


    out = mbedtls_calloc(1, (in_malloc_len));   // mbedtls_calloc分配的地址时会32字节对齐
    GCM_VALIDATE(out != NULL);

    add_malloc_len = (add_len / 4 + 1) * 4;                 // 长度是4字节对齐，避免写到硬件寄存器的数据长度超过add的长度
    add_1 = mbedtls_calloc(1, add_malloc_len);
    memcpy(add_1, add, add_len);
    memset(add_1 + add_len, 0, add_malloc_len - add_len);

    // mbedtls_printf("\nmode:%d, in:%p, out:%p, add_1:%p, iv:%p, aes_len0:%d, aes_len1:%d, aes_len2:%d, aes_len3:%d\n", mode, in, out, add_1, iv, aes_length[0],aes_length[1],aes_length[2],aes_length[3]);
    
    // if (aes_length[3] <= 32) {
    //     csk_dump_buf("input\n", in, aes_length[3]);
    //     csk_dump_buf("iv\n", iv, aes_length[1]);
    //     csk_dump_buf("add\n", add_1, aes_length[2]);
    //     csk_dump_buf("tag\n", tag, aes_length[0]);
    // }

#if CONFIG_DCACHE_ENABLE
    dcache_flush_range((uint32_t)in, (uint32_t)in + in_malloc_len);
    // dcache_flush_range((uint32_t)add_1, (uint32_t)add_1 + aes_length[2]);
    // dcache_flush_range((uint32_t)out, (uint32_t)out + aes_length[3]);
    dcache_invalidate_range((uint32_t)out, (uint32_t)out + in_malloc_len);
#endif

    csk_aes_crypto_setkey(aes_ctx);

    ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    GCM_VALIDATE(ret == 0);

    ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)iv);
    GCM_VALIDATE(ret == 0);

    if ( mode == MBEDTLS_GCM_ENCRYPT ) {
        // aad
        ret = CRYPTO_AES_Encrypt(aes_ctx->crypto_handler, (uint32_t *)add_1, aes_length[2], (uint32_t *)out);
        GCM_VALIDATE(ret == 0);

        // data
        ret = CRYPTO_AES_Encrypt(aes_ctx->crypto_handler, (uint32_t *)in, aes_length[3], (uint32_t *)out);
        GCM_VALIDATE(ret == 0);

        // get mac
        ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_GET_AES_MAC, (uint32_t)tag_out);
        GCM_VALIDATE(ret == 0);
    } else {
        // aad
        ret = CRYPTO_AES_Decrypt(aes_ctx->crypto_handler, (uint32_t *)add_1, aes_length[2], (uint32_t *)out);
        GCM_VALIDATE(ret == 0);

        ret = CRYPTO_AES_Decrypt(aes_ctx->crypto_handler, (uint32_t *)in, aes_length[3], (uint32_t *)out);
        GCM_VALIDATE(ret == 0);

        // get mac
        ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_GET_AES_MAC, (uint32_t)tag_out);
        GCM_VALIDATE(ret == 0);
    }

    memcpy(tag, tag_out, tag_len);
    memcpy(output, out, aes_length[3]);

    mbedtls_free(in);
    mbedtls_free(out);
    mbedtls_free(add_1);

    // if (aes_length[3] <= 32) {
    //     csk_dump_buf("tag\n", tag, aes_length[0]);
    //     csk_dump_buf("output\n", output, aes_length[3]);
    // }

    ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_AES);
    CSK_CRYPTO_CHECK_RET(ret == 0, CSK_CRYPTO_ERR_HARDWARE_RELEASED);

    return 0;

}

int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context *ctx,
                              int mode,
                              size_t length,
                              const unsigned char *iv,
                              size_t iv_len,
                              const unsigned char *add,
                              size_t add_len,
                              const unsigned char *input,
                              unsigned char *output,
                              size_t tag_len,
                              unsigned char *tag)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(iv_len == 0 || iv != NULL);
    GCM_VALIDATE_RET(add_len == 0 || add != NULL);
    GCM_VALIDATE_RET(length == 0 || input != NULL);
    GCM_VALIDATE_RET(length == 0 || output != NULL);
    GCM_VALIDATE_RET(tag != NULL);

    static uint32_t total_cnt = 0;
    static uint32_t miss_cnt = 0;

    total_cnt++;
    if ( (ctx->aes_ctx.is_aes) && (iv_len == AES_GCM_STANDARD_IV_LEN) && (length >= 256)) {
        // mbedtls_printf("\naes-gcm length: tag:%d iv:%d add:%d in:%d, in:%p, output:%p\n", 16, iv_len, add_len, length, input, output);
        return cks_aes_gcm_crypt_and_tag(ctx, mode, length, iv, iv_len, add, add_len, input, output, tag_len, tag);
    } else {
        miss_cnt++;
        // mbedtls_printf("\n==miss: %u/%u==== tag:%d iv:%d add:%d in:%d, in:%p, output:%p\n", miss_cnt, total_cnt, 16, iv_len, add_len, length, input, output);
    }

    if ((ret = mbedtls_gcm_starts(ctx, mode, iv, iv_len, add, add_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_gcm_update(ctx, length, input, output)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_gcm_finish(ctx, tag, tag_len)) != 0) {
        return ret;
    }

    return 0;
}

int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context *ctx,
                             size_t length,
                             const unsigned char *iv,
                             size_t iv_len,
                             const unsigned char *add,
                             size_t add_len,
                             const unsigned char *tag,
                             size_t tag_len,
                             const unsigned char *input,
                             unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char check_tag[16];
    int diff;

    GCM_VALIDATE_RET(ctx != NULL);
    GCM_VALIDATE_RET(iv_len == 0 || iv != NULL);
    GCM_VALIDATE_RET(add_len == 0 || add != NULL);
    GCM_VALIDATE_RET(tag != NULL);
    GCM_VALIDATE_RET(length == 0 || input != NULL);
    GCM_VALIDATE_RET(length == 0 || output != NULL);

    if ((ret = mbedtls_gcm_crypt_and_tag(ctx, MBEDTLS_GCM_DECRYPT, length,
                                         iv, iv_len, add, add_len,
                                         input, output, tag_len, check_tag)) != 0) {
        return ret;
    }

    /* Check tag in "constant-time" */
    diff = mbedtls_ct_memcmp(tag, check_tag, tag_len);

    if (diff != 0) {
        mbedtls_platform_zeroize(output, length);
        return MBEDTLS_ERR_GCM_AUTH_FAILED;
    }

    return 0;
}

void mbedtls_gcm_free(mbedtls_gcm_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
    mbedtls_cipher_free(&ctx->cipher_ctx);
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_gcm_context));
}
