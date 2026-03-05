/**
 ****************************************************************************************
 *
 * @file mbedtls_compat.c
 *
 * @brief Definition of compatible function to support mbedtls 2.xx and mbedtls 3.xx
 *
 * Copyright (C) ListenAI  2025 ~ 2099
 *
 ****************************************************************************************
 */

#include "mbedtls_compat.h"
/*
 * compatible function for mbedtls_pk_parse_key
 */
int _mbedtls_pk_parse_key(mbedtls_pk_context *ctx,
                         const unsigned char *key, size_t keylen,
                         const unsigned char *pwd, size_t pwdlen,
                         int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret;
#if MBEDTLS_MAJOR_VER == 3
    ret = mbedtls_pk_parse_key(ctx, key, keylen, pwd, pwdlen, f_rng, p_rng);
#else
    ret = mbedtls_pk_parse_key(ctx, key, keylen, pwd, pwdlen);
#endif
    return ret;
}

/*
 * compatible function for mbedtls_ecdsa_write_signature
 */
int _mbedtls_ecdsa_write_signature(mbedtls_ecdsa_context *ctx,
                                  mbedtls_md_type_t md_alg,
                                  const unsigned char *hash, size_t hlen,
                                  unsigned char *sig, size_t sig_size, size_t *slen,
                                  int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret;
#if MBEDTLS_MAJOR_VER == 3
    ret = mbedtls_ecdsa_write_signature(ctx, md_alg, hash, hlen, sig, sig_size, slen, f_rng, p_rng);
#else
    ret = mbedtls_ecdsa_write_signature(ctx, md_alg, hash, hlen, sig, slen, f_rng, p_rng);
#endif
    return ret;
}
