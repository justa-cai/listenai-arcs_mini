#ifndef _SHA512_ALT_H_
#define _SHA512_ALT_H_

#if defined(MBEDTLS_SHA512_ALT)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CSK_SHA512_STATE_INIT,
    CSK_SHA512_STATE_PROCESS,
}csk_sha512_state_t;

/**
 * \brief          The SHA-512 context structure.
 *
 *                 The structure is used both for SHA-384 and for SHA-512
 *                 checksum calculations. The choice between these two is
 *                 made in the call to mbedtls_sha512_starts_ret().
 */
typedef struct mbedtls_sha512_context {
    uint64_t total[2];          /*!< The number of Bytes processed. */
    uint64_t state[8];          /*!< The intermediate digest state. */
    unsigned char buffer[128];  /*!< The data block being processed. */
    int is384;                  /*!< Determines which function to use:
                                     0: Use SHA-512, or 1: Use SHA-384. */
    csk_sha512_state_t sha_state;
    void *crypto_handler;       /*!< The crypto handler. */
}
mbedtls_sha512_context;

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_SHA512_ALT */
#endif /* _SHA512_ALT_H_ */