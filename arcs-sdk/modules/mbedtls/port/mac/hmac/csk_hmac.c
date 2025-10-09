#include <string.h>

#include "mbedtls/md.h"
#include "mbedtls/md_internal.h"
#include "mbedtls/error.h"

#include "mbedtls/platform.h"

#include "csk_common.h"

static int csk_md_hmac(const mbedtls_md_info_t *md_info,
                    const unsigned char *key, size_t keylen,
                    const unsigned char *input, size_t ilen,
                    unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    CSK_CRYPTO_CHECK_RET(ilen == 0 || input != NULL, MBEDTLS_ERR_MD_BAD_INPUT_DATA);

    if (ilen == 0) {
        return 0;
    }

    void *crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_SHA);
    CSK_CRYPTO_CHECK_RET(crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

    if ( md_info->type == MBEDTLS_MD_SHA1 ) {
        CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA1);
    } else if ( md_info->type == MBEDTLS_MD_SHA224 ) {
        CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA224);
    } else if ( md_info->type == MBEDTLS_MD_SHA256 ) {
        CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);
    } else if ( md_info->type == MBEDTLS_MD_SHA384 ) {
        CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA384);
    } else if ( md_info->type == MBEDTLS_MD_SHA512 ) {
        CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA512);
    } else {
        mbedtls_printf("Error: Invalid hash type: %d\n", md_info->type);
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

#if CONFIG_DCACHE_ENABLE
    dcache_clean_range((uint32_t)input, (uint32_t)input + ilen);
    dcache_clean_range((uint32_t)key, (uint32_t)key + keylen);
    // dcache_invalidate_range((uint32_t)output, (uint32_t)output + ilen);
#endif

    CRYPTO_HMAC(crypto_handler, (uint32_t*)input, ilen, (uint32_t*)key, keylen, (uint32_t*)output);

    ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_SHA);
    CSK_CRYPTO_CHECK_RET(ret == 0, CSK_CRYPTO_ERR_HARDWARE_RELEASED);

    return 0;
}

int __real_mbedtls_md_hmac(const mbedtls_md_info_t *md_info,
                    const unsigned char *key, size_t keylen,
                    const unsigned char *input, size_t ilen,
                    unsigned char *output);

int __wrap_mbedtls_md_hmac(const mbedtls_md_info_t *md_info,
                    const unsigned char *key, size_t keylen,
                    const unsigned char *input, size_t ilen,
                    unsigned char *output)
{

    if (md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if ( md_info->type == MBEDTLS_MD_SHA1 ||
         md_info->type == MBEDTLS_MD_SHA224 ||
         md_info->type == MBEDTLS_MD_SHA256 ||
         md_info->type == MBEDTLS_MD_SHA384 ||
         md_info->type == MBEDTLS_MD_SHA512) {
        // mbedtls_printf("hardware hmac\n");
        return csk_md_hmac(md_info, key, keylen, input, ilen, output);
    } else {
        return __real_mbedtls_md_hmac(md_info, key, keylen, input, ilen, output);
    }
}