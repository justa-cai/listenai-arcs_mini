/*
 *  NIST SP800-38C compliant CCM implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * Definition of CCM:
 * http://csrc.nist.gov/publications/nistpubs/800-38C/SP800-38C_updated-July20_2007.pdf
 * RFC 3610 "Counter with CBC-MAC (CCM)"
 *
 * Related:
 * RFC 5116 "An Interface and Algorithms for Authenticated Encryption"
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/ccm.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/constant_time.h"
#include "mbedtls/platform.h"

#include "csk_common.h"

#define CCM_TAG_MIN_LEN 4

#define CCM_VALIDATE_RET(cond) \
    CSK_CRYPTO_CHECK_RET(cond, MBEDTLS_ERR_CCM_BAD_INPUT)
#define CCM_VALIDATE(cond) \
    CSK_CRYPTO_ASSERT(cond)

#define CSK_MBEDTLS_BYTE_0(x) ((uint8_t) ((x)         & 0xff))
#define CSK_MBEDTLS_BYTE_1(x) ((uint8_t) (((x) >> 8) & 0xff))

#define CSK_MBEDTLS_PUT_UINT16_BE(n, data, offset)                \
    {                                                               \
        (data)[(offset)] = CSK_MBEDTLS_BYTE_1(n);             \
        (data)[(offset) + 1] = CSK_MBEDTLS_BYTE_0(n);             \
    }

#define CCM_ENCRYPT 0
#define CCM_DECRYPT 1

/*
 * Initialize context
 */
void mbedtls_ccm_init(mbedtls_ccm_context *ctx)
{
    CCM_VALIDATE(ctx != NULL);
    memset(ctx, 0, sizeof(mbedtls_ccm_context));
}

int mbedtls_ccm_setkey(mbedtls_ccm_context *ctx,
                       mbedtls_cipher_id_t cipher,
                       const unsigned char *key,
                       unsigned int keybits)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const mbedtls_cipher_info_t *cipher_info;

    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(key != NULL);

    cipher_info = mbedtls_cipher_info_from_values(cipher, keybits,
                                                  MBEDTLS_MODE_ECB);
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (cipher_info->block_size != 16) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    memset(&ctx->aes_ctx, 0, sizeof(csk_ccm_aes_context_t));
    mbedtls_cipher_free(&ctx->cipher_ctx);

    if ((ret = mbedtls_cipher_setup(&ctx->cipher_ctx, cipher_info)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_cipher_setkey(&ctx->cipher_ctx, key, keybits,
                                    MBEDTLS_ENCRYPT)) != 0) {
        return ret;
    }

    /* csk aes-ccm */
    if ( cipher == MBEDTLS_CIPHER_ID_AES ) {
        if (keybits != 128 && keybits != 192 && keybits != 256) {
            return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
        }

        ctx->aes_ctx.key_bytes = keybits / 8;

        memcpy(ctx->aes_ctx.key, key, ctx->aes_ctx.key_bytes);

        ctx->aes_ctx.is_aes = 1;
    }

    return 0;
}

/*
 * Free context
 */
void mbedtls_ccm_free(mbedtls_ccm_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
    mbedtls_cipher_free(&ctx->cipher_ctx);
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_ccm_context));
}

/*
 * Macros for common operations.
 * Results in smaller compiled code than static inline functions.
 */

/*
 * Update the CBC-MAC state in y using a block in b
 * (Always using b as the source helps the compiler optimise a bit better.)
 */
#define UPDATE_CBC_MAC                                                      \
    for (i = 0; i < 16; i++)                                               \
    y[i] ^= b[i];                                                       \
                                                                            \
    if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, y, 16, y, &olen)) != 0) \
    return ret;

/*
 * Encrypt or decrypt a partial block with CTR
 * Warning: using b for temporary storage! src and dst must not be b!
 * This avoids allocating one more 16 bytes buffer while allowing src == dst.
 */
#define CTR_CRYPT(dst, src, len)                                            \
    do                                                                  \
    {                                                                   \
        if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctr,       \
                                         16, b, &olen)) != 0)      \
        {                                                               \
            return ret;                                              \
        }                                                               \
                                                                      \
        for (i = 0; i < (len); i++)                                    \
        (dst)[i] = (src)[i] ^ b[i];                                 \
    } while (0)

/*
 * Authenticated encryption or decryption
 */
static int ccm_auth_crypt(mbedtls_ccm_context *ctx, int mode, size_t length,
                          const unsigned char *iv, size_t iv_len,
                          const unsigned char *add, size_t add_len,
                          const unsigned char *input, unsigned char *output,
                          unsigned char *tag, size_t tag_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;
    unsigned char q;
    size_t len_left, olen;
    unsigned char b[16];
    unsigned char y[16];
    unsigned char ctr[16];
    const unsigned char *src;
    unsigned char *dst;

    /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     *
     * Also, loosen the requirements to enable support for CCM* (IEEE 802.15.4).
     */
    if (tag_len == 2 || tag_len > 16 || tag_len % 2 != 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    /* Also implies q is within bounds */
    if (iv_len < 7 || iv_len > 13) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (add_len >= 0xFF00) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    q = 16 - 1 - (unsigned char) iv_len;

    /*
     * First block B_0:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       length
     *
     * With flags as (bits):
     * 7        0
     * 6        add present?
     * 5 .. 3   (t - 2) / 2
     * 2 .. 0   q - 1
     */
    b[0] = 0;
    b[0] |= (add_len > 0) << 6;
    b[0] |= ((tag_len - 2) / 2) << 3;
    b[0] |= q - 1;

    memcpy(b + 1, iv, iv_len);

    for (i = 0, len_left = length; i < q; i++, len_left >>= 8) {
        b[15-i] = CSK_MBEDTLS_BYTE_0(len_left);
    }

    if (len_left > 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }


    /* Start CBC-MAC with first block */
    memset(y, 0, 16);
    UPDATE_CBC_MAC;

    /*
     * If there is additional data, update CBC-MAC with
     * add_len, add, 0 (padding to a block boundary)
     */
    if (add_len > 0) {
        size_t use_len;
        len_left = add_len;
        src = add;

        memset(b, 0, 16);
        CSK_MBEDTLS_PUT_UINT16_BE(add_len, b, 0);

        use_len = len_left < 16 - 2 ? len_left : 16 - 2;
        memcpy(b + 2, src, use_len);
        len_left -= use_len;
        src += use_len;

        UPDATE_CBC_MAC;

        while (len_left > 0) {
            use_len = len_left > 16 ? 16 : len_left;

            memset(b, 0, 16);
            memcpy(b, src, use_len);
            UPDATE_CBC_MAC;

            len_left -= use_len;
            src += use_len;
        }
    }

    /*
     * Prepare counter block for encryption:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       counter (initially 1)
     *
     * With flags as (bits):
     * 7 .. 3   0
     * 2 .. 0   q - 1
     */
    ctr[0] = q - 1;
    memcpy(ctr + 1, iv, iv_len);
    memset(ctr + 1 + iv_len, 0, q);
    ctr[15] = 1;

    /*
     * Authenticate and {en,de}crypt the message.
     *
     * The only difference between encryption and decryption is
     * the respective order of authentication and {en,de}cryption.
     */
    len_left = length;
    src = input;
    dst = output;

    while (len_left > 0) {
        size_t use_len = len_left > 16 ? 16 : len_left;

        if (mode == CCM_ENCRYPT) {
            memset(b, 0, 16);
            memcpy(b, src, use_len);
            UPDATE_CBC_MAC;
        }

        CTR_CRYPT(dst, src, use_len);

        if (mode == CCM_DECRYPT) {
            memset(b, 0, 16);
            memcpy(b, dst, use_len);
            UPDATE_CBC_MAC;
        }

        dst += use_len;
        src += use_len;
        len_left -= use_len;

        /*
         * Increment counter.
         * No need to check for overflow thanks to the length check above.
         */
        for (i = 0; i < q; i++) {
            if (++ctr[15-i] != 0) {
                break;
            }
        }
    }

    /*
     * Authentication: reset counter and crypt/mask internal tag
     */
    for (i = 0; i < q; i++) {
        ctr[15-i] = 0;
    }

    CTR_CRYPT(y, y, 16);
    memcpy(tag, y, tag_len);

    return 0;
}

static void csk_aes_crypto_setkey( csk_ccm_aes_context_t *ctx )
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
        mbedtls_printf("Error: Invalid key size: %d\n", keybits);
        return;
    }

    CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CCM);

    CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)key);
}

/*
 * Authenticated encryption or decryption implementation by csk aes
 */
static int csk_ccm_aes_auth_crypt(mbedtls_ccm_context *ctx, int mode, size_t length,
                          const unsigned char *iv, size_t iv_len,
                          const unsigned char *add, size_t add_len,
                          const unsigned char *input, unsigned char *output,
                          unsigned char *tag, size_t tag_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    csk_ccm_aes_context_t *aes_ctx = &ctx->aes_ctx;
    uint32_t aes_length[4] = {0};
    uint8_t tag_out[16] = {0};
    uint8_t *in = NULL;
    uint8_t *out = NULL;

    /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     *
     * Also, loosen the requirements to enable support for CCM* (IEEE 802.15.4).
     */
    if (tag_len == 2 || tag_len > 16 || tag_len % 2 != 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    /* Also implies q is within bounds */
    if (iv_len < 7 || iv_len > 13) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (add_len >= 0xFF00) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    aes_length[0] = tag_len;
    aes_length[1] = iv_len;
    aes_length[2] = add_len;
    aes_length[3] = length;
    
    if (input == output) {
        in = mbedtls_calloc(1, length);
        CCM_VALIDATE(in != NULL);
        memcpy(in, input, length);

        out = mbedtls_calloc(1, length);
        CCM_VALIDATE(out != NULL);

    } else {
        in = (uint8_t *)input;
        out = (uint8_t *)output;
    }

    // mbedtls_printf("aes-ccm length: tag:%d iv:%d add:%d in:%d\n", aes_length[0], aes_length[1], aes_length[2], aes_length[3]);

    aes_ctx->crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_AES);
    CSK_CRYPTO_CHECK_RET(aes_ctx->crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);
    
    csk_aes_crypto_setkey(aes_ctx);

    ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CCM_VALIDATE(ret == 0);

    ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)iv);
    CCM_VALIDATE(ret == 0);

#if CONFIG_DCACHE_ENABLE
    // dcache_clean_range((uint32_t)add, (uint32_t)add + aes_length[2]);
    // dcache_clean_range((uint32_t)input, (uint32_t)input + aes_length[3]);
#endif

    if ( mode == CCM_ENCRYPT ) {
        // aad
        ret = CRYPTO_AES_Encrypt(aes_ctx->crypto_handler, (uint32_t *)add, aes_length[2], (uint32_t *)out);
        CCM_VALIDATE(ret == 0);

        // data
        ret = CRYPTO_AES_Encrypt(aes_ctx->crypto_handler, (uint32_t *)in, aes_length[3], (uint32_t *)out);
        CCM_VALIDATE(ret == 0);

        // get mac
        ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_GET_AES_MAC, (uint32_t)tag_out);
        CCM_VALIDATE(ret == 0);
    } else {
        // aad
        ret = CRYPTO_AES_Decrypt(aes_ctx->crypto_handler, (uint32_t *)add, aes_length[2], (uint32_t *)out);
        CCM_VALIDATE(ret == 0);

        ret = CRYPTO_AES_Decrypt(aes_ctx->crypto_handler, (uint32_t *)in, aes_length[3], (uint32_t *)out);
        CCM_VALIDATE(ret == 0);

        // get mac
        ret = CRYPTO_Control(aes_ctx->crypto_handler, CSK_CRYPTO_GET_AES_MAC, (uint32_t)tag_out);
        CCM_VALIDATE(ret == 0);
    }

    memcpy(tag, tag_out, tag_len);
    
    if (input == output) {
        memcpy(output, out, aes_length[3]);

        mbedtls_free(in);
        mbedtls_free(out);
    }

    ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_AES);
    CSK_CRYPTO_CHECK_RET(ret == 0, CSK_CRYPTO_ERR_HARDWARE_RELEASED);

    return 0;
}
/*
 * Authenticated encryption
 */
int mbedtls_ccm_star_encrypt_and_tag(mbedtls_ccm_context *ctx, size_t length,
                                     const unsigned char *iv, size_t iv_len,
                                     const unsigned char *add, size_t add_len,
                                     const unsigned char *input, unsigned char *output,
                                     unsigned char *tag, size_t tag_len)
{
    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(iv != NULL);
    CCM_VALIDATE_RET(add_len == 0 || add != NULL);
    CCM_VALIDATE_RET(length == 0 || input != NULL);
    CCM_VALIDATE_RET(length == 0 || output != NULL);
    CCM_VALIDATE_RET(tag_len == 0 || tag != NULL);
    if ( (ctx->aes_ctx.is_aes) && (tag_len > CCM_TAG_MIN_LEN) ) {
        return csk_ccm_aes_auth_crypt(ctx, CCM_ENCRYPT, length, iv, iv_len,
                          add, add_len, input, output, tag, tag_len);
    } else {
        return ccm_auth_crypt(ctx, CCM_ENCRYPT, length, iv, iv_len,
                            add, add_len, input, output, tag, tag_len);
    }
}

int mbedtls_ccm_encrypt_and_tag(mbedtls_ccm_context *ctx, size_t length,
                                const unsigned char *iv, size_t iv_len,
                                const unsigned char *add, size_t add_len,
                                const unsigned char *input, unsigned char *output,
                                unsigned char *tag, size_t tag_len)
{
    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(iv != NULL);
    CCM_VALIDATE_RET(add_len == 0 || add != NULL);
    CCM_VALIDATE_RET(length == 0 || input != NULL);
    CCM_VALIDATE_RET(length == 0 || output != NULL);
    CCM_VALIDATE_RET(tag_len == 0 || tag != NULL);
    if (tag_len == 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    return mbedtls_ccm_star_encrypt_and_tag(ctx, length, iv, iv_len, add,
                                            add_len, input, output, tag, tag_len);
}

/*
 * Authenticated decryption
 */
int mbedtls_ccm_star_auth_decrypt(mbedtls_ccm_context *ctx, size_t length,
                                  const unsigned char *iv, size_t iv_len,
                                  const unsigned char *add, size_t add_len,
                                  const unsigned char *input, unsigned char *output,
                                  const unsigned char *tag, size_t tag_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char check_tag[16];
    int diff;

    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(iv != NULL);
    CCM_VALIDATE_RET(add_len == 0 || add != NULL);
    CCM_VALIDATE_RET(length == 0 || input != NULL);
    CCM_VALIDATE_RET(length == 0 || output != NULL);
    CCM_VALIDATE_RET(tag_len == 0 || tag != NULL);

    if (ctx->aes_ctx.is_aes) {
        ret = csk_ccm_aes_auth_crypt(ctx, CCM_DECRYPT, length, iv, iv_len,
                          add, add_len, input, output, check_tag, tag_len);
    } else {
        ret = ccm_auth_crypt(ctx, CCM_DECRYPT, length,
                              iv, iv_len, add, add_len,
                              input, output, check_tag, tag_len);
    }

    if (ret != 0) {
        return ret;
    }

    /* Check tag in "constant-time" */
    diff = mbedtls_ct_memcmp(tag, check_tag, tag_len);

    if (diff != 0) {
        mbedtls_platform_zeroize(output, length);
        return MBEDTLS_ERR_CCM_AUTH_FAILED;
    }

    return 0;
}

int mbedtls_ccm_auth_decrypt(mbedtls_ccm_context *ctx, size_t length,
                             const unsigned char *iv, size_t iv_len,
                             const unsigned char *add, size_t add_len,
                             const unsigned char *input, unsigned char *output,
                             const unsigned char *tag, size_t tag_len)
{
    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(iv != NULL);
    CCM_VALIDATE_RET(add_len == 0 || add != NULL);
    CCM_VALIDATE_RET(length == 0 || input != NULL);
    CCM_VALIDATE_RET(length == 0 || output != NULL);
    CCM_VALIDATE_RET(tag_len == 0 || tag != NULL);

    if (tag_len == 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    return mbedtls_ccm_star_auth_decrypt(ctx, length, iv, iv_len, add,
                                         add_len, input, output, tag, tag_len);
}
