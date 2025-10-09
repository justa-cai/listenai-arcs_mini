/*
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t key_bytes;
    uint8_t key[32];
    void *crypto_handler;       /*!< The crypto handler. */
} csk_aes_context;

typedef struct
{
    csk_aes_context crypt; /*!< The AES context to use for AES block
                                        encryption or decryption. */
    csk_aes_context tweak; /*!< The AES context used for tweak
                                        computation. */
} csk_aes_xts_context;

void csk_aes_init( csk_aes_context *ctx );
void csk_aes_free( csk_aes_context *ctx );

/**
 * \brief          This function initializes the specified AES XTS context.
 *
 *                 It must be the first API called before using
 *                 the context.
 *
 * \param ctx      The AES XTS context to initialize.
 */
void csk_aes_xts_init( csk_aes_xts_context *ctx );

/**
 * \brief          This function releases and clears the specified AES XTS context.
 *
 * \param ctx      The AES XTS context to clear.
 */
void csk_aes_xts_free( csk_aes_xts_context *ctx );

int csk_aes_setkey( csk_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits );

int csk_internal_aes_encrypt( csk_aes_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] );

int csk_internal_aes_decrypt( csk_aes_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] );

int csk_aes_crypt_ecb( csk_aes_context *ctx,
                           int mode,
                           const unsigned char input[16],
                           unsigned char output[16] );


int csk_aes_crypt_cbc( csk_aes_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output );

int csk_aes_crypt_cfb128( csk_aes_context *ctx,
                       int mode,
                       size_t length,
                       size_t *iv_off,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output );

int csk_aes_crypt_cfb8( csk_aes_context *ctx,
                            int mode,
                            size_t length,
                            unsigned char iv[16],
                            const unsigned char *input,
                            unsigned char *output );

int csk_aes_crypt_ofb( csk_aes_context *ctx,
                           size_t length,
                           size_t *iv_off,
                           unsigned char iv[16],
                           const unsigned char *input,
                           unsigned char *output );

int csk_aes_crypt_ctr( csk_aes_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output );

/**
 * \brief          This function prepares an XTS context for encryption and
 *                 sets the encryption key.
 *
 * \param ctx      The AES XTS context to which the key should be bound.
 * \param key      The encryption key. This is comprised of the XTS key1
 *                 concatenated with the XTS key2.
 * \param keybits  The size of \p key passed in bits. Valid options are:
 *                 <ul><li>256 bits (each of key1 and key2 is a 128-bit key)</li>
 *                 <li>512 bits (each of key1 and key2 is a 256-bit key)</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
int csk_aes_xts_setkey_enc( csk_aes_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits );

/**
 * \brief          This function prepares an XTS context for decryption and
 *                 sets the decryption key.
 *
 * \param ctx      The AES XTS context to which the key should be bound.
 * \param key      The decryption key. This is comprised of the XTS key1
 *                 concatenated with the XTS key2.
 * \param keybits  The size of \p key passed in bits. Valid options are:
 *                 <ul><li>256 bits (each of key1 and key2 is a 128-bit key)</li>
 *                 <li>512 bits (each of key1 and key2 is a 256-bit key)</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
int csk_aes_xts_setkey_dec( csk_aes_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits );


/**
 * \brief           Internal AES block encryption function
 *                  (Only exposed to allow overriding it,
 *                  see AES_ENCRYPT_ALT)
 *
 * \param ctx       AES context
 * \param input     Plaintext block
 * \param output    Output (ciphertext) block
 */
int csk_internal_aes_encrypt( csk_aes_context *ctx, const unsigned char input[16], unsigned char output[16] );

/**
 * \brief           Internal AES block decryption function
 *                  (Only exposed to allow overriding it,
 *                  see AES_DECRYPT_ALT)
 *
 * \param ctx       AES context
 * \param input     Ciphertext block
 * \param output    Output (plaintext) block
 */
int csk_internal_aes_decrypt( csk_aes_context *ctx, const unsigned char input[16], unsigned char output[16] );

/** AES-XTS buffer encryption/decryption */
int csk_aes_crypt_xts( csk_aes_xts_context *ctx, int mode, size_t length, const unsigned char data_unit[16], const unsigned char *input, unsigned char *output );

#ifdef __cplusplus
}
#endif
