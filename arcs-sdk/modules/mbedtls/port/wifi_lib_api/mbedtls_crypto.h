#ifndef _MBEDTLS_CRYPTO_H
#define _MBEDTLS_CRYPTO_H
/**
 ****************************************************************************************
 *
 * @file mbedtls_crypto.h
 *
 * @brief crypto function for wpa lib
 *
 * Copyright (C) ListenAI  2025 ~ 2099
 *
 ****************************************************************************************
 */

#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"


//struct crypto_bignum;
struct crypto_bignum {
    mbedtls_mpi mpi;
};

struct crypto_ec {
    mbedtls_ecp_group group;
};
struct crypto_ec_point {
    mbedtls_ecp_point point;
};
struct crypto_ec_key
{
    mbedtls_pk_context pk;
};
struct crypto_ecdh
{
    struct crypto_ec_key *key;
    bool ephemeral_key;
};
void _mbedtls_mpi_init(struct crypto_bignum *X);
void _mbedtls_mpi_free(struct crypto_bignum *n);
int _mbedtls_mpi_read_binary(struct crypto_bignum *n, const unsigned char *buf, size_t buflen);
int _mbedtls_mpi_lset(struct crypto_bignum *n, int val);
int _mbedtls_mpi_mul_int(struct crypto_bignum *n, unsigned int val);
size_t _mbedtls_mpi_size(const struct crypto_bignum *n);
int _mbedtls_mpi_write_binary(struct crypto_bignum *n,
                             unsigned char *buf, size_t buflen);
int _mbedtls_mpi_mod_mpi(struct crypto_bignum *r, const struct crypto_bignum *a, const struct crypto_bignum *b);
int _mbedtls_mpi_add_mpi(struct crypto_bignum *X,
                                      const struct crypto_bignum *A,
                                      const struct crypto_bignum *B);
int _mbedtls_mpi_exp_mod(struct crypto_bignum *X, const struct crypto_bignum *A,
                                        const struct crypto_bignum *E, const struct crypto_bignum *N);
int _mbedtls_mpi_inv_mod(struct crypto_bignum *X, const struct crypto_bignum *A, const struct crypto_bignum *N);
int _mbedtls_mpi_sub_mpi(struct crypto_bignum *X, const struct crypto_bignum *A, const struct crypto_bignum *N);
int _mbedtls_mpi_div_mpi(struct crypto_bignum *Q, struct crypto_bignum *R, const struct crypto_bignum *A,
                                        const struct crypto_bignum *B);
int _mbedtls_mpi_mul_mpi(struct crypto_bignum *X, const struct crypto_bignum *A, const struct crypto_bignum *B);
int _mbedtls_mpi_copy(struct crypto_bignum *X, const struct crypto_bignum *Y);
int _mbedtls_mpi_shift_r(struct crypto_bignum *X, size_t count);
int _mbedtls_mpi_cmp_mpi(const struct crypto_bignum *X, const struct crypto_bignum *Y);
size_t _mbedtls_mpi_bitlen(const struct crypto_bignum *X);
int _mbedtls_mpi_cmp_int(const struct crypto_bignum *X, int z);
int _mbedtls_crypto_bignum_is_odd(const struct crypto_bignum *a);
int _mbedtls_crypto_bignum_legendre(const struct crypto_bignum *a, const struct crypto_bignum *p);
struct crypto_bignum *_crypto_bignum_init(void);
struct crypto_bignum *_crypto_bignum_init_set(const uint8_t *buf, size_t len);
struct crypto_bignum * _crypto_bignum_init_uint(unsigned int val);
void _crypto_bignum_deinit(struct crypto_bignum *n, int clear);
int _crypto_bignum_to_bin(const struct crypto_bignum *a, uint8_t *buf, size_t buflen, size_t padlen);

/*** crypto ec related func **/
//struct crypto_ec;
void _crypto_ec_deinit(struct crypto_ec *e);
struct crypto_ec *_crypto_ec_init(int group);
size_t _crypto_ec_prime_len(struct crypto_ec *e);
size_t _crypto_ec_prime_len_bits(struct crypto_ec *e);
size_t _crypto_ec_order_len(struct crypto_ec *e);
const struct crypto_bignum * _crypto_ec_get_prime(struct crypto_ec *e);
const struct crypto_bignum * _crypto_ec_get_order(struct crypto_ec *e);
const struct crypto_bignum * _crypto_ec_get_a(struct crypto_ec *e);
const struct crypto_bignum * _crypto_ec_get_b(struct crypto_ec *e);
const struct crypto_ec_point * _crypto_ec_get_generator(struct crypto_ec *e);

/** crypto_ec_point related func */
//struct crypto_ec_point;

struct crypto_ec_point *_crypto_ec_point_init(struct crypto_ec *e);
void _crypto_ec_point_deinit(struct crypto_ec_point *p, int clear);
int _crypto_ec_point_x(struct crypto_ec *e, const struct crypto_ec_point *p,
                                                            struct crypto_bignum *x);
int _crypto_ec_point_to_bin(struct crypto_ec *e,
                        const struct crypto_ec_point *p, uint8_t *x, uint8_t *y);
struct crypto_ec_point *_crypto_ec_point_from_bin(struct crypto_ec *e,
                                                                const uint8_t *val);
int _crypto_ec_point_add(struct crypto_ec *e, const struct crypto_ec_point *a,
                                        const struct crypto_ec_point *b,
                                                struct crypto_ec_point *c);
int _crypto_ec_point_mul(struct crypto_ec *e, const struct crypto_ec_point *p,
                                                        const struct crypto_bignum *b,
                                                        struct crypto_ec_point *res,
                                    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);
int _crypto_ec_point_invert(struct crypto_ec *e, struct crypto_ec_point *p);
int _crypto_ec_point_solve_y_coord(struct crypto_ec *e,
                                                struct crypto_ec_point *p,
                                                const struct crypto_bignum *x, int y_bit);

struct crypto_bignum *_crypto_ec_point_compute_y_sqr(struct crypto_ec *e,
                                                                    const struct crypto_bignum *x);

int _crypto_ec_point_is_at_infinity(struct crypto_ec *e,
                                                    const struct crypto_ec_point *p);

int _crypto_ec_point_is_on_curve(struct crypto_ec *e,
                                                    const struct crypto_ec_point *p);

int _crypto_ec_point_cmp(const struct crypto_ec *e,
                                 const struct crypto_ec_point *a,
                                 const struct crypto_ec_point *b);

int _crypto_ec_point_debug(const struct crypto_ec *e,
                                 const struct crypto_ec_point *p,
                                 const char *title, size_t prime_len, char *str, uint8_t *bin);

/** crypto_ec_key related func */
//struct crypto_ec_key;
//struct crypto_ecdh;
struct crypto_ecdh * _crypto_ecdh_init(int group, int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng);
struct crypto_ecdh * _crypto_ecdh_init2(int group, struct crypto_ec_key *own_key);
int _crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, uint8_t *x, uint8_t *y);
int _crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, struct crypto_bignum *z, int inc_y,
                                 const uint8_t *key, size_t len, size_t *out_prime_len,
                                 int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

void _crypto_ecdh_deinit(struct crypto_ecdh *ecdh);
size_t _crypto_ecdh_prime_len(struct crypto_ecdh *ecdh);
struct crypto_ec_key * _crypto_ec_key_parse_priv(const uint8_t *der, size_t der_len,
                                 int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

struct crypto_ec_key * _crypto_ec_key_parse_pub(const uint8_t *pubkey, size_t pubkey_len, int grp_id);
struct crypto_ec_key * _crypto_ec_key_set_pub(int group, const uint8_t *x, const uint8_t *y, size_t len);
struct crypto_ec_key * _crypto_ec_key_set_pub_point(struct crypto_ec *e,
                                                                  const struct crypto_ec_point *pub);
struct crypto_ec_key * _crypto_ec_key_gen(int group,
                        int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);
void _crypto_ec_key_deinit(struct crypto_ec_key *key);
size_t _crypto_ec_key_mbedtls_mpi_size(struct crypto_ec_key *key);
int _crypto_ec_key_mbedtls_mpi_get_bit(struct crypto_ec_key *key, size_t pos);
int _crypto_ec_key_get_grp_id(struct crypto_ec_key *key);
struct crypto_ec * _crypto_ec_key_get_grp(struct crypto_ec_key *key);
struct crypto_ec_point * _crypto_ec_key_get_q(struct crypto_ec_key *key);
int _crypto_ec_key_mbedtls_mpi_x_write_binary(struct crypto_ec_key *key,
                             unsigned char *buf, size_t buflen);
int _crypto_ec_key_mbedtls_mpi_y_write_binary(struct crypto_ec_key *key,
                             unsigned char *buf, size_t buflen);
int _crypto_ec_key_mbedtls_mpi_d_write_binary(struct crypto_ec_key *key,
                             unsigned char *buf, size_t buflen);
size_t _crypto_ec_key_get_mpi_num(struct crypto_ec_key *key);
int _crypto_ec_key_point_to_bin(struct crypto_ec_key *key, uint8_t *x, uint8_t *y);
const struct crypto_ec_point *_crypto_ec_key_get_public_key(struct crypto_ec_key *key);
const struct crypto_bignum *_crypto_ec_key_get_private_key(struct crypto_ec_key *key);
int _crypto_ec_key_sign_r_s(struct crypto_ec_key *key, const uint8_t *data,
                                 size_t len, uint8_t *pbuf, size_t prime_len,
                                 int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

int _crypto_ec_key_verify_signature(struct crypto_ec_key *key, const uint8_t *data,
                                 size_t len, const uint8_t *sig, size_t sig_len);

int _crypto_ec_key_verify_signature_r_s(struct crypto_ec_key *key, const uint8_t *data,
                                 size_t len,  const uint8_t *r, size_t r_len,
                                 const uint8_t *s, size_t s_len);
int _crypto_ec_key_group(struct crypto_ec_key *key);
int _crypto_ec_key_cmp(struct crypto_ec_key *key1, struct crypto_ec_key *key2);

int _mbedtls_ecdsa_write_signature(struct crypto_ec_key *key,
                                  const unsigned char *hash, size_t hlen,
                                  unsigned char *sig, size_t sig_size, size_t *slen,
                                  int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

#endif /* _MBEDTLS_CRYPTO_H */

