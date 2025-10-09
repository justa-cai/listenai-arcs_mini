#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mbedtls/error.h"
#include "mbedtls/platform.h"
#include "mbedtls/aes.h"

#include "csk_aes.h"
#include "csk_common.h"

#define AES_CHECK_RET(cond, ret)    CSK_CRYPTO_CHECK_RET(cond, ret)
#define AES_ASSERT(cond)            CSK_CRYPTO_ASSERT(cond)

static int csk_aes_validate_input( csk_aes_context *ctx, const unsigned char *input,
                                  const unsigned char *output )
{
    if (!ctx) {
        mbedtls_printf("No AES context supplied\n");
        return -1;
    }
    if (!input) {
        mbedtls_printf("No input supplied\n");
        return -1;
    }
    if (!output) {
        mbedtls_printf("No output supplied\n");
        return -1;
    }

    return 0;
}

void csk_aes_crypto_setkey( csk_aes_context *ctx )
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

    CRYPTO_Control(ctx->crypto_handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)key);
}

void csk_aes_init( csk_aes_context *ctx )
{
    memset( ctx, 0, sizeof( csk_aes_context ) );
}

void csk_aes_free( csk_aes_context *ctx )
{
    if( ctx == NULL )
        return;

    memset( ctx, 0, sizeof( csk_aes_context ) );
}

int csk_aes_setkey( csk_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits )
{
	if (keybits != 128 && keybits != 192 && keybits != 256) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}
	ctx->key_bytes = keybits / 8;
	memcpy(ctx->key, key, ctx->key_bytes);

	return 0;
}

int csk_internal_aes_encrypt( csk_aes_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] )
{
    int ret;

    if (csk_aes_validate_input(ctx, input, output)) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    ret = CRYPTO_AES_Encrypt(ctx->crypto_handler, (uint32_t *)input, 16, (uint32_t *)output);
    AES_ASSERT(ret == 0);

    return 0;
}

int csk_internal_aes_decrypt( csk_aes_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] )
{
    int ret;

    if (csk_aes_validate_input(ctx, input, output)) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    ret = CRYPTO_AES_Decrypt(ctx->crypto_handler, (uint32_t *)input, 16, (uint32_t *)output);
    AES_ASSERT(ret == 0);

    return 0;
}

// 前提这里是nocache区域，并且是4字节对齐的
static unsigned char in[16] __attribute__((aligned(4))) = {0};
static unsigned char out[16] __attribute__((aligned(4))) = {0};

int csk_aes_crypt_ecb( csk_aes_context *ctx,
                           int mode,
                           const unsigned char input[16],
                           unsigned char output[16] )
{
	int ret = 0;

    if (csk_aes_validate_input(ctx, input, output)) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( mode != MBEDTLS_AES_ENCRYPT && mode != MBEDTLS_AES_DECRYPT ) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    ctx->crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_AES);
    AES_CHECK_RET(ctx->crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

    CRYPTO_Control(ctx->crypto_handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_ECB);

    csk_aes_crypto_setkey(ctx);

    memcpy(in, input, 16);

    if (mode == MBEDTLS_AES_ENCRYPT) {
        ret = csk_internal_aes_encrypt(ctx, in, out);
    } else {
        ret = csk_internal_aes_decrypt(ctx, in, out);
    }
    AES_ASSERT(ret == 0);

    memcpy(output, out, 16);

    ret = csk_crypto_release_hardware(CSK_CRYPTO_TYPE_AES);
    AES_ASSERT(ret == 0);

    return 0;
}

int csk_aes_crypt_cbc( csk_aes_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
	int i;
	int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
	unsigned char temp[16];

    if (csk_aes_validate_input(ctx, input, output)) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if ( mode != MBEDTLS_AES_ENCRYPT && mode != MBEDTLS_AES_DECRYPT ) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

	if (length % 16) {
		return (MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH);
	}

    if (!iv) {
        return (MBEDTLS_ERR_AES_BAD_INPUT_DATA);
    }

	if (mode == MBEDTLS_AES_DECRYPT) {
		while (length > 0) {
			memcpy(temp, input, 16);
			ret = csk_aes_crypt_ecb(ctx, mode, input, output);
			if (ret != 0)
				goto exit;

			for (i = 0; i < 16; i++)
				output[i] = (unsigned char)(output[i] ^ iv[i]);

			memcpy(iv, temp, 16);

			input += 16;
			output += 16;
			length -= 16;
		}
	} else {
		while (length > 0) {
			for (i = 0; i < 16; i++)
				output[i] = (unsigned char)(input[i] ^ iv[i]);

			ret = csk_aes_crypt_ecb(ctx, mode, output, output);
			if (ret != 0)
				goto exit;
			memcpy(iv, output, 16);

			input += 16;
			output += 16;
			length -= 16;
		}
	}

	ret = 0;
exit:
	return (ret);
}

int csk_aes_crypt_cfb128( csk_aes_context *ctx,
                       int mode,
                       size_t length,
                       size_t *iv_off,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    int c;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;

    if( csk_aes_validate_input( ctx, input, output ) ) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( mode != MBEDTLS_AES_ENCRYPT && mode != MBEDTLS_AES_DECRYPT ) {
        mbedtls_printf("mode error: %d\n", mode);
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( !iv) {
        mbedtls_printf("No IV supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( !iv_off) {
        mbedtls_printf("No IV offset supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    n = *iv_off;

    if( n > 15 )
        return( MBEDTLS_ERR_AES_BAD_INPUT_DATA );

    if( mode == MBEDTLS_AES_DECRYPT )
    {
        while( length-- )
        {
            if( n == 0 )
            {
                ret = csk_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );
                if( ret != 0 )
                    goto exit;
            }

            c = *input++;
            *output++ = (unsigned char)( c ^ iv[n] );
            iv[n] = (unsigned char) c;

            n = ( n + 1 ) & 0x0F;
        }
    }
    else
    {
        while( length-- )
        {
            if( n == 0 )
            {
                ret = csk_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );
                if( ret != 0 )
                    goto exit;
            }

            iv[n] = *output++ = (unsigned char)( iv[n] ^ *input++ );

            n = ( n + 1 ) & 0x0F;
        }
    }

    *iv_off = n;
    ret = 0;

exit:
    return( ret );
}

int csk_aes_crypt_cfb8( csk_aes_context *ctx,
                            int mode,
                            size_t length,
                            unsigned char iv[16],
                            const unsigned char *input,
                            unsigned char *output )
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char c;
    unsigned char ov[17];

    if( csk_aes_validate_input( ctx, input, output ) ) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( mode != MBEDTLS_AES_ENCRYPT && mode != MBEDTLS_AES_DECRYPT ) {
        mbedtls_printf("mode error: %d\n", mode);
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( !iv) {
        mbedtls_printf("No IV supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    while( length-- )
    {
        memcpy( ov, iv, 16 );
        ret = csk_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );
        if( ret != 0 )
            goto exit;

        if( mode == MBEDTLS_AES_DECRYPT )
            ov[16] = *input;

        c = *output++ = (unsigned char)( iv[0] ^ *input++ );

        if( mode == MBEDTLS_AES_ENCRYPT )
            ov[16] = c;

        memcpy( iv, ov + 1, 16 );
    }
    ret = 0;

exit:
    return( ret );
}

int csk_aes_crypt_ofb( csk_aes_context *ctx,
                           size_t length,
                           size_t *iv_off,
                           unsigned char iv[16],
                           const unsigned char *input,
                           unsigned char *output )
{
    int ret = 0;
    size_t n;

    if( csk_aes_validate_input( ctx, input, output ) ) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( !iv) {
        mbedtls_printf("No IV supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if( !iv_off) {
        mbedtls_printf("No IV offset supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    n = *iv_off;

    if( n > 15 )
        return( MBEDTLS_ERR_AES_BAD_INPUT_DATA );

    while( length-- )
    {
        if( n == 0 )
        {
            ret = csk_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );
            if( ret != 0 )
                goto exit;
        }
        *output++ =  *input++ ^ iv[n];

        n = ( n + 1 ) & 0x0F;
    }

    *iv_off = n;

exit:
    return( ret );
}

int csk_aes_crypt_ctr( csk_aes_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output )
{
	int c, i;

    if (csk_aes_validate_input(ctx, input, output)) {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if (!stream_block) {
        mbedtls_printf("No stream supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if (!nonce_counter) {
        mbedtls_printf("No nonce supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if (!nc_off) {
        mbedtls_printf("No nonce offset supplied\n");
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

	size_t n = *nc_off;

	while (length--) {
		if (n == 0) {
			csk_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, nonce_counter, stream_block);

			for (i = 16; i > 0; i--) {
				if (++nonce_counter[i - 1] != 0) {
					break;
				}
			}
		}
		c = *input++;
		*output++ = (unsigned char)(c ^ stream_block[n]);

		n = (n + 1) & 0x0F;
	}

	*nc_off = n;

	return 0;
}
