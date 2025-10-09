#ifndef SHA256_ALT_H
#define SHA256_ALT_H

#if defined(MBEDTLS_SHA256_ALT)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CSK_SHA256_STATE_INIT,
    CSK_SHA256_STATE_PROCESS,
}csk_sha256_state_t;

/**
 * \brief          The SHA-256 context structure.
 *
 *                 The structure is used both for SHA-256 and for SHA-224
 *                 checksum calculations. The choice between these two is
 *                 made in the call to mbedtls_sha256_starts_ret().
 */
typedef struct mbedtls_sha256_context {
    uint32_t total[2];          /*!< The number of Bytes processed.  */
    uint32_t state[8];          /*!< The intermediate digest state.  */
    unsigned char buffer[64];   /*!< The data block being processed. */
    int is224;                  /*!< Determines which function to use:
                                     0: Use SHA-256, or 1: Use SHA-224. */
    csk_sha256_state_t sha_state;
    void *crypto_handler;       /*!< The crypto handler. */
}
mbedtls_sha256_context;

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_SHA256_ALT */
#endif /* SHA256_ALT_H */