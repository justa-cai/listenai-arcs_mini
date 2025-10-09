#ifndef CMAC_ALT_H
#define CMAC_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#undef MBEDTLS_CMAC_MAX_BLOCK_SIZE

/* We don't support Camellia or ARIA in this module */
#if defined(MBEDTLS_AES_C)
#define MBEDTLS_CMAC_MAX_BLOCK_SIZE      16  /**< The longest block used by CMAC is that of AES. */
#else
#define MBEDTLS_CMAC_MAX_BLOCK_SIZE      8   /**< The longest block used by CMAC is that of 3DES. */
#endif

#include <stdint.h>

typedef struct {
    uint32_t key_bytes;
    uint8_t key[32];
    void *crypto_handler;       /*!< The crypto handler. */
} csk_cmac_aes_context_t;

/**
 * The CMAC context structure.
 */
struct mbedtls_cmac_context_t {
    /** The internal state of the CMAC algorithm.  */
    unsigned char       state[MBEDTLS_CMAC_MAX_BLOCK_SIZE];

    /** Unprocessed data - either data that was not block aligned and is still
     *  pending processing, or the final block. */
    unsigned char       unprocessed_block[MBEDTLS_CMAC_MAX_BLOCK_SIZE];

    /** The length of data pending processing. */
    size_t              unprocessed_len;
};

#ifdef __cplusplus
}
#endif

#endif /* CMAC_ALT_H */