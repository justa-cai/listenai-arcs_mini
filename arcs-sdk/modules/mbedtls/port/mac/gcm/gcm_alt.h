#ifndef GCM_ALT_H
#define GCM_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint32_t key_bytes;
    uint8_t key[32];
    void *crypto_handler;       /*!< The crypto handler. */
    uint32_t is_aes;
} csk_gcm_aes_context_t;

typedef struct mbedtls_gcm_context {
    mbedtls_cipher_context_t cipher_ctx;  /*!< The cipher context used. */
    uint64_t HL[16];                      /*!< Precalculated HTable low. */
    uint64_t HH[16];                      /*!< Precalculated HTable high. */
    uint64_t len;                         /*!< The total length of the encrypted data. */
    uint64_t add_len;                     /*!< The total length of the additional data. */
    unsigned char base_ectr[16];          /*!< The first ECTR for tag. */
    unsigned char y[16];                  /*!< The Y working value. */
    unsigned char buf[16];                /*!< The buf working value. */
    int mode;                             /*!< The operation to perform:
                                           #MBEDTLS_GCM_ENCRYPT or
                                           #MBEDTLS_GCM_DECRYPT. */
    csk_gcm_aes_context_t aes_ctx;
}
mbedtls_gcm_context;

#ifdef __cplusplus
}
#endif

#endif /* GCM_ALT_H */