#ifndef CCM_ALT_H
#define CCM_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint32_t key_bytes;
    uint8_t key[32];
    void *crypto_handler;       /*!< The crypto handler. */
    uint32_t is_aes;
} csk_ccm_aes_context_t;

typedef struct mbedtls_ccm_context {
    mbedtls_cipher_context_t cipher_ctx;    /*!< The cipher context used. */
    csk_ccm_aes_context_t aes_ctx;              /*!< The AES context used. */
}
mbedtls_ccm_context;

#ifdef __cplusplus
}
#endif

#endif /* CCM_ALT_H */