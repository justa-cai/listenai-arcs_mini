#ifndef SHA1_ALT_H
#define SHA1_ALT_H

#if defined(MBEDTLS_SHA1_ALT)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CSK_SHA1_STATE_INIT,
    CSK_SHA1_STATE_PROCESS,
}csk_sha1_state_t;

/**
 * \brief          The SHA-1 context structure.
 *
 * \warning        SHA-1 is considered a weak message digest and its use
 *                 constitutes a security risk. We recommend considering
 *                 stronger message digests instead.
 *
 */
typedef struct mbedtls_sha1_context {
    uint32_t total[2];          /*!< The number of Bytes processed.  */
    uint32_t state[5];          /*!< The intermediate digest state.  */
    unsigned char buffer[64];   /*!< The data block being processed. */
    csk_sha1_state_t sha_state;
    void *crypto_handler;       /*!< The crypto handler. */
    // unsigned char output[20];   /*!< The digest. */
}
mbedtls_sha1_context;

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_SHA1_ALT */

#endif /* SHA1_ALT_H */