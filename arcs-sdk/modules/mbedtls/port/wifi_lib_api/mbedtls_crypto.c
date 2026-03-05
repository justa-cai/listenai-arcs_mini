#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// #include "library/common.h"
#include "mbedtls_crypto.h"
#include "rtos_al.h"


void _mbedtls_mpi_init(struct crypto_bignum *n)
{
    mbedtls_mpi_init(&n->mpi);
}

void _mbedtls_mpi_free(struct crypto_bignum *n)
{
    mbedtls_mpi_free(&n->mpi);
}

int _mbedtls_mpi_read_binary(struct crypto_bignum *n, const unsigned char *buf, size_t buflen)
{
    return mbedtls_mpi_read_binary(&n->mpi, buf, buflen);
}

int _mbedtls_mpi_lset(struct crypto_bignum *n, int val)
{
    return mbedtls_mpi_lset(&n->mpi, val);
}

int _mbedtls_mpi_mul_int(struct crypto_bignum *n, unsigned int val)
{
    return mbedtls_mpi_mul_int(&n->mpi, &n->mpi, val);
}

size_t _mbedtls_mpi_size(const struct crypto_bignum *n)
{
    size_t len = mbedtls_mpi_size(&n->mpi);
    return len;
}


int _mbedtls_mpi_write_binary(struct crypto_bignum *n,
                             unsigned char *buf, size_t buflen)
{
    int ret = 0;
    ret = mbedtls_mpi_write_binary(&n->mpi, buf, buflen);
    return ret;
}


int _mbedtls_mpi_mod_mpi(struct crypto_bignum *r, const struct crypto_bignum *a, const struct crypto_bignum *b)
{
    int ret = 0;
    ret = mbedtls_mpi_mod_mpi(&r->mpi, &a->mpi, &b->mpi);
    return ret;
}


int _mbedtls_mpi_add_mpi(struct crypto_bignum *X,
              const struct crypto_bignum *A,
              const struct crypto_bignum *B)
{
    int ret = 0;
    ret = mbedtls_mpi_add_mpi(&X->mpi, &A->mpi, &B->mpi);
    return ret;
}


int _mbedtls_mpi_exp_mod(struct crypto_bignum *X, const struct crypto_bignum *A,
                        const struct crypto_bignum *E, const struct crypto_bignum *N)
{
    int ret = 0;
    ret = mbedtls_mpi_exp_mod(&X->mpi, &A->mpi, &E->mpi, &N->mpi, NULL);
    return ret;
}


int _mbedtls_mpi_inv_mod(struct crypto_bignum *X, const struct crypto_bignum *A, const struct crypto_bignum *N)
{
    int ret = 0;
    ret = mbedtls_mpi_inv_mod(&X->mpi, &A->mpi, &N->mpi);
    return ret;

}

int _mbedtls_mpi_sub_mpi(struct crypto_bignum *X, const struct crypto_bignum *A, const struct crypto_bignum *N)
{
    int ret = 0;
    ret = mbedtls_mpi_sub_mpi(&X->mpi, &A->mpi, &N->mpi);
    return ret;
}


int _mbedtls_mpi_div_mpi(struct crypto_bignum *Q, struct crypto_bignum *R, const struct crypto_bignum *A,
                        const struct crypto_bignum *B)
{
    int ret = 0;
    if (R!=NULL)
        ret = mbedtls_mpi_div_mpi(&Q->mpi, &R->mpi, &A->mpi, &B->mpi);
    else
        ret = mbedtls_mpi_div_mpi(&Q->mpi, NULL, &A->mpi, &B->mpi);
    return ret;
}

int _mbedtls_mpi_mul_mpi(struct crypto_bignum *X, const struct crypto_bignum *A, const struct crypto_bignum *B)
{
    int ret = 0;
    ret = mbedtls_mpi_mul_mpi(&X->mpi, &A->mpi, &B->mpi);
    return ret;
}


int _mbedtls_mpi_copy(struct crypto_bignum *X, const struct crypto_bignum *Y)
{
    int ret = 0;
    ret = mbedtls_mpi_copy(&X->mpi, &Y->mpi);
    return ret;
}

int _mbedtls_mpi_shift_r(struct crypto_bignum *X, size_t count)
{
    int ret = 0;
    ret = mbedtls_mpi_shift_r(&X->mpi, count);
    return ret;
}

int _mbedtls_mpi_cmp_mpi(const struct crypto_bignum *X, const struct crypto_bignum *Y)
{
    int ret = 0;
    ret = mbedtls_mpi_cmp_mpi(&X->mpi, &Y->mpi);
    return ret;
}

size_t _mbedtls_mpi_bitlen(const struct crypto_bignum *X)
{
    return mbedtls_mpi_bitlen(&X->mpi);
}

int _mbedtls_mpi_cmp_int(const struct crypto_bignum *X, int z)
{
    int ret = 0;
    ret = mbedtls_mpi_cmp_int(&X->mpi, z);
    return ret;
}

int _mbedtls_crypto_bignum_is_odd(const struct crypto_bignum *a)
{
    if (a->mpi.p)
        return (a->mpi.p[0] & 0x1);
    return 0;
}

int _mbedtls_crypto_bignum_legendre(const struct crypto_bignum *a,
                                                    const struct crypto_bignum *p)
{
    mbedtls_mpi exp, tmp;
    int res = -2;

    mbedtls_mpi_init(&exp);
    mbedtls_mpi_init(&tmp);

    // exp = (p-1) / 2
    if (mbedtls_mpi_sub_int(&exp, &p->mpi, 1) ||
        mbedtls_mpi_shift_r(&exp, 1))
        goto end;

    if (mbedtls_mpi_exp_mod(&tmp, &a->mpi, &exp, &p->mpi, NULL))
        goto end;

    if (mbedtls_mpi_cmp_int(&tmp, 1) == 0)
        res = 1;
    else if (mbedtls_mpi_cmp_int(&tmp, 0) == 0)
        res = 0;
    else
        res = -1;

end:
    mbedtls_mpi_free(&exp);
    mbedtls_mpi_free(&tmp);
    return res;
}

struct crypto_bignum *_crypto_bignum_init(void)
{
    struct crypto_bignum *n;

    n = rtos_malloc(sizeof(*n));
    if (!n)
        return NULL;

    mbedtls_mpi_init(&n->mpi);

    return n;
}

struct crypto_bignum *_crypto_bignum_init_set(const uint8_t *buf, size_t len)
{
    struct crypto_bignum *n = _crypto_bignum_init();
    if (!n)
        return NULL;

    if (mbedtls_mpi_read_binary(&n->mpi, buf, len)) {
        _crypto_bignum_deinit(n, 0);
        return NULL;
    }

    return n;
}

struct crypto_bignum * _crypto_bignum_init_uint(unsigned int val)
{
    struct crypto_bignum *n = _crypto_bignum_init();
    if (!n)
        return NULL;

    // use mpi_mul_int as mpi_lset only take singed int as parameter
    if (mbedtls_mpi_lset(&n->mpi, 1) ||
        mbedtls_mpi_mul_int(&n->mpi, &n->mpi, val)) {
        _crypto_bignum_deinit(n, 0);
        return NULL;
    }

    return n;
}

void _crypto_bignum_deinit(struct crypto_bignum *n, int clear)
{
    // mbedtls always clear the memory
    mbedtls_mpi_free(&n->mpi);
    rtos_free(n);
}

int _crypto_bignum_to_bin(const struct crypto_bignum *a,
                        uint8_t *buf, size_t buflen, size_t padlen)
{
    int res = buflen;
    size_t len = mbedtls_mpi_size(&a->mpi);

    if (len > buflen)
        return -1;

    // mbedtls will always do padding to the size of the buffer.
    if (padlen <= len)
        res = buflen = len;
    else
        res = buflen = padlen;

    if (mbedtls_mpi_write_binary(&a->mpi, buf, buflen))
        return -1;

    return res;
}


/* Map from IANA registry for IKE D-H groups to Mbed TLS group ID */
static mbedtls_ecp_group_id mbedtls_get_group_id(int group)
{
    switch (group) {
    case 19:
        return MBEDTLS_ECP_DP_SECP256R1;
    case 20:
        return MBEDTLS_ECP_DP_SECP384R1;
    case 21:
        return MBEDTLS_ECP_DP_SECP521R1;
    case 25:
        return MBEDTLS_ECP_DP_SECP192R1;
    case 26:
        // mbedtls support this curve (MBEDTLS_ECP_DP_SECP224R1) but since the prime
        // of this curve is not 3 congruent module 4 the square root algo used in
        // crypto_ec_point_solve_y_coord is not correct.
        return -1;
    case 28:
        return MBEDTLS_ECP_DP_BP256R1;
    case 29:
        return MBEDTLS_ECP_DP_BP384R1;
    case 30:
        return MBEDTLS_ECP_DP_BP512R1;
    default:
        return -1;
    }
}

void _crypto_ec_deinit(struct crypto_ec *e)
{
    mbedtls_ecp_group_free(&e->group);
    rtos_free(e);
}

struct crypto_ec *_crypto_ec_init(int group)
{
    struct crypto_ec *ec;
    mbedtls_ecp_group_id grp_id;
    grp_id = mbedtls_get_group_id(group);
    if (grp_id < 0)
        return NULL;

    ec = rtos_malloc(sizeof(*ec));
    if (!ec)
        return NULL;

    mbedtls_ecp_group_init(&ec->group);
    if (mbedtls_ecp_group_load(&ec->group, grp_id)) {
        _crypto_ec_deinit(ec);
        return NULL;
    }

    return ec;
}

/**
 * _crypto_ec_prime_len - Get length of the prime in octets
 * @e: EC context from crypto_ec_init()
 * Returns: Length of the prime defining the group
 */
size_t _crypto_ec_prime_len(struct crypto_ec *e)
{
    return mbedtls_mpi_size(&e->group.P);
}

/**
 * _crypto_ec_prime_len_bits - Get length of the prime in bits
 * @e: EC context from crypto_ec_init()
 * Returns: Length of the prime defining the group in bits
 */
size_t _crypto_ec_prime_len_bits(struct crypto_ec *e)
{
    return mbedtls_mpi_bitlen(&e->group.P);
}

/**
 * _crypto_ec_order_len - Get length of the order in octets
 * @e: EC context from crypto_ec_init()
 * Returns: Length of the order defining the group
 */
size_t _crypto_ec_order_len(struct crypto_ec *e)
{
    return mbedtls_mpi_size(&e->group.N);
}


/**
 * _crypto_ec_get_prime - Get prime defining an EC group
 * @e: EC context from crypto_ec_init()
 * Returns: Prime (bignum) defining the group
 */
const struct crypto_bignum * _crypto_ec_get_prime(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.P;
}

/**
 * _crypto_ec_get_order - Get order of an EC group
 * @e: EC context from crypto_ec_init()
 * Returns: Order (bignum) of the group
 */
const struct crypto_bignum * _crypto_ec_get_order(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.N;
}



/**
 * -3 in a bignum, to be used for NIST curve 'a' coefficient
 */
static mbedtls_mpi_uint minus_3_data[1] = {3};
static struct crypto_bignum minus_3 = {
    .mpi = {
        .s = -1,
        .n = 1,
        .p = minus_3_data,
    },
};

/**
 * _crypto_ec_get_a - Get 'a' value of an EC curve
 * @e: EC context from crypto_ec_init()
 * Returns: a (bignum) of the group
 */
const struct crypto_bignum * _crypto_ec_get_a(struct crypto_ec *e)
{
    if (e->group.A.p)
        return (const struct crypto_bignum *)&e->group.A;
    else
        // For NIST curves mbedtls doesn't store the value
        // of 'a' in the group as it is always -3
        return (const struct crypto_bignum *)&minus_3;
}


/**
 * _crypto_ec_get_b - Get 'b' value of an EC curve
 * @e: EC context from crypto_ec_init()
 * Returns: b (bignum) of the group
 */
const struct crypto_bignum * _crypto_ec_get_b(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.B;
}

/**
 * _crypto_ec_get_generator - Get generator point of the EC group's curve
 * @e: EC context from crypto_ec_init()
 * Returns: Pointer to Generator point
 */
const struct crypto_ec_point * _crypto_ec_get_generator(struct crypto_ec *e)
{
    return (const struct crypto_ec_point *)&e->group.G;
}


/**
 * _crypto_ec_point_init - Initialize data for an EC point
 * @e: EC context from crypto_ec_init()
 * Returns: Pointer to EC point data or %NULL on failure
 */
struct crypto_ec_point *_crypto_ec_point_init(struct crypto_ec *e)
{
    struct crypto_ec_point *ecp;
    ecp = rtos_malloc(sizeof(*ecp));
    if (!ecp)
        return NULL;

    mbedtls_ecp_point_init(&ecp->point);
    return ecp;
}

/**
 * _crypto_ec_point_deinit - Deinitialize EC point data
 * @p: EC point data from crypto_ec_point_init()
 * @clear: Whether to clear the EC point value from memory
 */
void _crypto_ec_point_deinit(struct crypto_ec_point *p, int clear)
{
    // always clear memory
    mbedtls_ecp_point_free(&p->point);
    rtos_free(p);
}

/**
 * _crypto_ec_point_x - Copies the x-ordinate point into big number
 * @e: EC context from crypto_ec_init()
 * @p: EC point data
 * @x: Big number to set to the copy of x-ordinate
 * Returns: 0 on success, -1 on failure
 */
int _crypto_ec_point_x(struct crypto_ec *e, const struct crypto_ec_point *p,
              struct crypto_bignum *x)
{
    if (mbedtls_mpi_copy(&x->mpi, &p->point.X))
        return -1;
    return 0;
}

/**
 * _crypto_ec_point_to_bin - Write EC point value as binary data
 * @e: EC context from crypto_ec_init()
 * @p: EC point data from crypto_ec_point_init()
 * @x: Buffer for writing the binary data for x coordinate or %NULL if not used
 * @y: Buffer for writing the binary data for y coordinate or %NULL if not used
 * Returns: 0 on success, -1 on failure
 *
 * This function can be used to write an EC point as binary data in a format
 * that has the x and y coordinates in big endian byte order fields padded to
 * the length of the prime defining the group.
 */
int _crypto_ec_point_to_bin(struct crypto_ec *e,
               const struct crypto_ec_point *p, uint8_t *x, uint8_t *y)
{
    size_t p_len = mbedtls_mpi_size(&e->group.P);
    if (x && mbedtls_mpi_write_binary(&p->point.X, x, p_len))
        return -1;

    if (y && mbedtls_mpi_write_binary(&p->point.Y, y, p_len))
        return -1;

    return 0;
}

/**
 * _crypto_ec_point_from_bin - Create EC point from binary data
 * @e: EC context from crypto_ec_init()
 * @val: Binary data to read the EC point from
 * Returns: Pointer to EC point data or %NULL on failure
 *
 * This function readers x and y coordinates of the EC point from the provided
 * buffer assuming the values are in big endian byte order with fields padded to
 * the length of the prime defining the group.
 */
struct crypto_ec_point *_crypto_ec_point_from_bin(struct crypto_ec *e,
                          const uint8_t *val)
{
    size_t p_len = _crypto_ec_prime_len(e);
    size_t tmp_len = 2 * p_len + 1;
    struct crypto_ec_point *ecp;
    uint8_t *tmp;
    ecp = _crypto_ec_point_init(e);
    if (!ecp)
        return NULL;

    tmp = rtos_malloc(tmp_len);
    if (!tmp) {
        _crypto_ec_point_deinit(ecp, 1);
        return NULL;
    }

    tmp[0] = 0x4; // UNCOMPRESSED
    memcpy(&tmp[1], val, 2 * p_len);

    if (mbedtls_ecp_point_read_binary(&e->group, &ecp->point, tmp, tmp_len)) {
        _crypto_ec_point_deinit(ecp, 1);
        ecp = NULL;
    }

    rtos_free(tmp);
    return ecp;
}

/**
 * _crypto_bignum_add - c = a + b
 * @e: EC context from crypto_ec_init()
 * @a: Bignum
 * @b: Bignum
 * @c: Bignum; used to store the result of a + b
 * Returns: 0 on success, -1 on failure
 */
int _crypto_ec_point_add(struct crypto_ec *e, const struct crypto_ec_point *a,
            const struct crypto_ec_point *b,
            struct crypto_ec_point *c)
{
    mbedtls_mpi one;
    int ret = -1;
    mbedtls_mpi_init(&one);
    if (mbedtls_mpi_lset(&one, 1))
        goto end;

    if (mbedtls_ecp_muladd(&e->group, &c->point, &one, &a->point,
                   &one, &b->point))
        goto end;

    ret = 0;

end:
    mbedtls_mpi_free(&one);
    return ret;
}

/**
 * _crypto_bignum_mul - res = b * p
 * @e: EC context from crypto_ec_init()
 * @p: EC point
 * @b: Bignum
 * @res: EC point; used to store the result of b * p
 * Returns: 0 on success, -1 on failure
 */
int _crypto_ec_point_mul(struct crypto_ec *e, const struct crypto_ec_point *p,
            const struct crypto_bignum *b,
            struct crypto_ec_point *res,
            int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    if (mbedtls_ecp_mul(&e->group, &res->point, &b->mpi, &p->point,
                f_rng, p_rng))
        return -1;

    return 0;
}

/**
 * _crypto_ec_point_invert - Compute inverse of an EC point
 * @e: EC context from crypto_ec_init()
 * @p: EC point to invert (and result of the operation)
 * Returns: 0 on success, -1 on failure
 */
int _crypto_ec_point_invert(struct crypto_ec *e, struct crypto_ec_point *p)
{
    mbedtls_mpi one;
    mbedtls_mpi minus_one;
    mbedtls_ecp_point zero;
    int ret = -1;

    mbedtls_mpi_init(&one);
    mbedtls_mpi_init(&minus_one);
    mbedtls_ecp_point_init(&zero);

    if (mbedtls_mpi_lset(&one, 1) ||
        mbedtls_mpi_lset(&minus_one, -1) ||
        mbedtls_ecp_set_zero(&zero))
        goto end;

    if (mbedtls_ecp_muladd(&e->group, &p->point, &one, &zero,
                   &minus_one, &p->point))
        goto end;

    ret = 0;

end:
    mbedtls_mpi_free(&one);
    mbedtls_mpi_free(&minus_one);
    mbedtls_ecp_point_free(&zero);
    return ret;
}


/**
 * _crypto_ec_point_solve_y_coord - Solve y coordinate for an x coordinate
 * @e: EC context from crypto_ec_init()
 * @p: EC point to use for the returning the result
 * @x: x coordinate
 * @y_bit: y-bit (0 or 1) for selecting the y value to use
 * Returns: 0 on success, -1 on failure
 */
int _crypto_ec_point_solve_y_coord(struct crypto_ec *e,
                  struct crypto_ec_point *p,
                  const struct crypto_bignum *x, int y_bit)
{
    struct crypto_bignum *y_sqr;
    mbedtls_mpi exp;
    int ret = -1;

    mbedtls_mpi_init(&exp);

    y_sqr = _crypto_ec_point_compute_y_sqr(e, x);
    if (!y_sqr)
        return -1;

    // If p = 3 % 4 (which is the case for all curves except the one for group 26)
    // then y = (y_sqr)^((p + 1) / 4)

    // exp = (p + 1) / 4
    if (mbedtls_mpi_add_int(&exp, &e->group.P, 1) ||
        mbedtls_mpi_shift_r(&exp, 2))
        goto end;

    if (mbedtls_mpi_exp_mod(&p->point.Y, &y_sqr->mpi, &exp, &e->group.P, NULL))
        goto end;

    if (((p->point.Y.p[0] & 0x1) != y_bit) &&
        mbedtls_mpi_sub_mpi(&p->point.Y, &e->group.P, &p->point.Y))
        goto end;

    if (mbedtls_mpi_copy(&p->point.X, &x->mpi) ||
        mbedtls_mpi_lset(&p->point.Z, 1))
        goto end;

    ret = 0;
end:
    mbedtls_mpi_free(&exp);
    _crypto_bignum_deinit(y_sqr, 1);
    return ret;
}

/**
 * _crypto_ec_point_compute_y_sqr - Compute y^2 = x^3 + ax + b
 * @e: EC context from crypto_ec_init()
 * @x: x coordinate
 * Returns: y^2 on success, %NULL failure
 */
struct crypto_bignum *
_crypto_ec_point_compute_y_sqr(struct crypto_ec *e,
                  const struct crypto_bignum *x)
{
    struct crypto_bignum *y_sqr = _crypto_bignum_init();

    if (!y_sqr)
        return NULL;

    // x^2 (% p)
    if (mbedtls_mpi_mul_mpi(&y_sqr->mpi, &x->mpi, &x->mpi) ||
        mbedtls_mpi_mod_mpi(&y_sqr->mpi, &y_sqr->mpi, &e->group.P))
        goto error;

    // (X^2) + a (%p)
    if (e->group.A.p == NULL) {
        // For optimizations mbedtls doesn't store 'a' when it is -3 ...
        if (mbedtls_mpi_sub_int(&y_sqr->mpi, &y_sqr->mpi, 3) ||
            ((mbedtls_mpi_cmp_int(&y_sqr->mpi, 0) < 0) &&
             mbedtls_mpi_add_mpi(&y_sqr->mpi, &y_sqr->mpi, &e->group.P)))
            goto error;
    } else if (mbedtls_mpi_add_mpi(&y_sqr->mpi, &y_sqr->mpi, &e->group.A) ||
           ((mbedtls_mpi_cmp_mpi(&y_sqr->mpi, &e->group.P) >= 0) &&
            mbedtls_mpi_sub_abs(&y_sqr->mpi, &y_sqr->mpi, &e->group.P)))
        goto error;

    // (x^2 + a) * x (% p)
    if (mbedtls_mpi_mul_mpi(&y_sqr->mpi, &y_sqr->mpi, &x->mpi) ||
        mbedtls_mpi_mod_mpi(&y_sqr->mpi, &y_sqr->mpi, &e->group.P))
        goto error;

    // ((x^2 + a) * x) + b (%p)
    if (mbedtls_mpi_add_mpi(&y_sqr->mpi, &y_sqr->mpi, &e->group.B) ||
        ((mbedtls_mpi_cmp_mpi(&y_sqr->mpi, &e->group.P) >= 0) &&
         mbedtls_mpi_sub_abs(&y_sqr->mpi, &y_sqr->mpi, &e->group.P)))
        goto error;

    return y_sqr;

error:
    _crypto_bignum_deinit(y_sqr, 1);
    return NULL;
}


/**
 * _crypto_ec_point_is_at_infinity - Check whether EC point is neutral element
 * @e: EC context from crypto_ec_init()
 * @p: EC point
 * Returns: 1 if the specified EC point is the neutral element of the group or
 *  0 if not
 */
int _crypto_ec_point_is_at_infinity(struct crypto_ec *e,
                   const struct crypto_ec_point *p)
{
    return mbedtls_ecp_is_zero((mbedtls_ecp_point *)&p->point);
}


/**
 * _crypto_ec_point_is_on_curve - Check whether EC point is on curve
 * @e: EC context from crypto_ec_init()
 * @p: EC point
 * Returns: 1 if the specified EC point is on the curve or 0 if not
 */
int _crypto_ec_point_is_on_curve(struct crypto_ec *e,
                const struct crypto_ec_point *p)
{
    if (mbedtls_ecp_check_pubkey(&e->group, &p->point))
        return 0;
    return 1;
}


/**
 * _crypto_ec_point_cmp - Compare two EC points
 * @e: EC context from crypto_ec_init()
 * @a: EC point
 * @b: EC point
 * Returns: 0 on equal, non-zero otherwise
 */
int _crypto_ec_point_cmp(const struct crypto_ec *e,
            const struct crypto_ec_point *a,
            const struct crypto_ec_point *b)
{
    return mbedtls_ecp_point_cmp(&a->point, &b->point);
}


/**
 * _crypto_ec_point_debug_print - Dump EC point
 * @e: EC context from crypto_ec_init()
 * @p: EC point
 * @title: Name of the EC point in the trace
 */
static char* write_hex(char *str, uint8_t val)
{
    uint8_t x[2];
    x[0] = (val >> 4) & 0x0f;
    x[1] = val & 0xf;

    for (int i = 0; i <2 ; i++)
    {
        if (x[i] < 10)
            *str++ = '0' + x[i];
        else
            *str++ = 'a' - 10 + x[i];
    }
    return str;
}
int _crypto_ec_point_debug(const struct crypto_ec *e,
                 const struct crypto_ec_point *p,
                 const char *title, size_t prime_len, char *str, uint8_t *bin)
{
    char *pos;
        int ret = 0;

    pos = str;
    *pos++ = '(';

    if (mbedtls_mpi_write_binary(&p->point.X, bin, prime_len)) {
        ret = -1;
        return ret;
    }

    for (int i=0; i <prime_len; i++) {
        pos = write_hex(pos, bin[i]);
    }

    *pos++ = ',';
    if (mbedtls_mpi_write_binary(&p->point.Y, bin, prime_len)) {
        ret = -1;
        return ret;
    }

    for (int i=0; i <prime_len; i++) {
        pos = write_hex(pos, bin[i]);
    }
    *pos++ = ')';
    *pos = 0;
        return ret;
}

/**
 * _crypto_ecdh_init - Initialize elliptic curve diffie–hellman context
 * @group: Identifying number for the ECC group (IANA "Group Description"
 *  attribute registrty for RFC 2409)
 * This function generates ephemeral key pair.
 * Returns: Pointer to ECDH context or %NULL on failure
 */
struct crypto_ecdh * _crypto_ecdh_init(int group, int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    struct crypto_ecdh *ecdh = NULL;
    mbedtls_ecp_group_id grp_id;
    mbedtls_ecp_keypair *key;

    grp_id = mbedtls_get_group_id(group);
    if (grp_id < 0)
        return NULL;

    ecdh = rtos_calloc(sizeof(struct crypto_ecdh) + sizeof(struct crypto_ec_key), sizeof(uint8_t));
    if (!ecdh)
        return NULL;
    ecdh->key = (struct crypto_ec_key *)((uint8_t *)ecdh + sizeof(struct crypto_ecdh));

    ecdh->ephemeral_key = true;
    mbedtls_pk_init(&ecdh->key->pk);
    if (mbedtls_pk_setup(&ecdh->key->pk,
                 mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY_DH)))
        goto fail;

    key = mbedtls_pk_ec(ecdh->key->pk);

    if (mbedtls_ecp_group_load(&key->grp, grp_id) ||
        mbedtls_ecdh_gen_public(&key->grp, &key->d, &key->Q, f_rng, p_rng))
        goto fail;

    return ecdh;

fail:
    _crypto_ecdh_deinit(ecdh);
    return NULL;
}


/**
 * _crypto_ecdh_init2 - Initialize elliptic curve diffie–hellman context with
 * given EC key
 * @group: Identifying number for the ECC group (IANA "Group Description"
 *  attribute registrty for RFC 2409)
 * @own_key: Our own EC Key.
 * Returns: Pointer to ECDH context or %NULL on failure
 */
struct crypto_ecdh * _crypto_ecdh_init2(int group, struct crypto_ec_key *own_key)
{
    struct crypto_ecdh *ecdh = NULL;

    ecdh = rtos_calloc(sizeof(struct crypto_ecdh), sizeof(uint8_t));
    if (!ecdh)
        return NULL;

    ecdh->key = own_key;
    return ecdh;
}


/**
 * _crypto_ecdh_get_pubkey - Retrieve Public from ECDH context
 * @ecdh: ECDH context from crypto_ecdh_init() or crypto_ecdh_init2()
 * @inc_y: Whether public key should include y coordinate (explicit form)
 * or not (compressed form)
 * Returns: Binary data f the public key or %NULL on failure
 */
 int _crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, uint8_t *x, uint8_t *y)
{
    mbedtls_ecp_keypair *key;
    int ret = 0;
    
    key = mbedtls_pk_ec(ecdh->key->pk);

    if (_crypto_ec_point_to_bin((struct crypto_ec *)&key->grp,
                   (struct crypto_ec_point *)&key->Q, x, y)) {
        ret = -1;
    }

    return ret;
}

/**
 * _crypto_ecdh_set_peerkey - Compute ECDH secret
 * @ecdh: ECDH context from crypto_ecdh_init() or crypto_ecdh_init2()
 * @inc_y: Whether Peer's public key includes y coordinate (explicit form)
 * or not (compressed form)
 * @key: Binary data of the Peer's public key
 * @len: Length of the @key buffer
 * Returns: Binary data with the EDCH secret or %NULL on failure
 */
int _crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, struct crypto_bignum *z, int inc_y,
                    const uint8_t *key, size_t len, size_t *out_prime_len,
                    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    mbedtls_ecp_point peer_pub;
    mbedtls_ecp_keypair *own_key;
    size_t prime_len, secret_len;
    //mbedtls_mpi z;
    struct wpabuf *secret = NULL;
    int ret = -1;

    own_key = mbedtls_pk_ec(ecdh->key->pk);
    prime_len = mbedtls_mpi_size(&own_key->grp.P);
    mbedtls_ecp_point_init(&peer_pub);
    mbedtls_mpi_init(&z->mpi);

    if (inc_y) {
        if (len != (2 * prime_len))
            return ret;

        if (mbedtls_mpi_read_binary(&peer_pub.X, key, prime_len) ||
            mbedtls_mpi_read_binary(&peer_pub.Y, key + prime_len, prime_len) ||
            mbedtls_mpi_lset(&peer_pub.Z, 1))
            goto fail;
    } else {
        if (mbedtls_mpi_read_binary(&z->mpi, key, len) ||
            _crypto_ec_point_solve_y_coord((struct crypto_ec *)&own_key->grp,
                          (struct crypto_ec_point *)&peer_pub,
                          (struct crypto_bignum *)&z->mpi, 0))
            goto fail;
        mbedtls_mpi_free(&z->mpi);
    }


    if (mbedtls_ecdh_compute_shared(&own_key->grp, &z->mpi, &peer_pub, &own_key->d,
                    f_rng, p_rng))
        goto fail;
    *out_prime_len = prime_len;
    ret = 0;


fail:
    mbedtls_ecp_point_free(&peer_pub);
    return ret;
}

/**
 * _crypto_ecdh_deinit - Free ECDH context
 * @ecdh: ECDH context from crypto_ecdh_init() or crypto_ecdh_init2()
 */
void _crypto_ecdh_deinit(struct crypto_ecdh *ecdh)
{
    if (!ecdh)
        return;

    if (ecdh->ephemeral_key)
        mbedtls_pk_free(&ecdh->key->pk);
    rtos_free(ecdh);
}


/**
 * _crypto_ecdh_prime_len - Get length of the prime in octets
 * @e: ECDH context from crypto_ecdh_init()
 * Returns: Length of the prime defining the group
 */
size_t _crypto_ecdh_prime_len(struct crypto_ecdh *ecdh)
{
    mbedtls_ecp_keypair *key;
    key = mbedtls_pk_ec(ecdh->key->pk);
    return mbedtls_mpi_size(&key->grp.P);
}


/*
 * compatible function for mbedtls_pk_parse_key
 */
static int _mbedtls_pk_parse_key(mbedtls_pk_context *ctx,
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

/**
 * _crypto_ec_key_parse_priv - Initialize EC Key pair from ECPrivateKey ASN.1
 * @der: DER encoding of ASN.1 ECPrivateKey
 * @der_len: Length of @der buffer
 * Returns: EC key or %NULL on failure
 */
struct crypto_ec_key * _crypto_ec_key_parse_priv(const uint8_t *der, size_t der_len,
                      int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    struct crypto_ec_key *key;

    key = rtos_calloc(sizeof(struct crypto_ec_key), sizeof(uint8_t));
    if (!key)
        return NULL;
    mbedtls_pk_init(&key->pk);

    if (_mbedtls_pk_parse_key(&key->pk, der, der_len, NULL, 0, f_rng, p_rng)) {
        _crypto_ec_key_deinit(key);
        return NULL;
    }

    return key;
}


/**
 * _crypto_ec_key_parse_pub - Initialize EC Key pair from SubjectPublicKeyInfo ASN.1
 * @der: DER encoding of ASN.1 SubjectPublicKeyInfo
 * @der_len: Length of @der buffer
 * Returns: EC key or %NULL on failure
 */
struct crypto_ec_key * _crypto_ec_key_parse_pub(const uint8_t *pubkey, size_t pubkey_len, int grp_id)
{
    struct crypto_ec_key *key;
    mbedtls_ecp_keypair *eckey;

    key = rtos_calloc(sizeof(struct crypto_ec_key), sizeof(uint8_t));
    if (!key)
        return NULL;

    mbedtls_pk_init(&key->pk);
    if (mbedtls_pk_setup(&key->pk,
                 mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)))
        goto fail;
    eckey = mbedtls_pk_ec(key->pk);
    if (mbedtls_ecp_group_load(&eckey->grp, (mbedtls_ecp_group_id)grp_id))
        goto fail;

    if (pubkey[0] == 0x4) {
        // uncompressed form
        if (mbedtls_ecp_point_read_binary(&eckey->grp, &eckey->Q,
                          pubkey, pubkey_len))
            goto fail;

    } else {
        //compressed form
        mbedtls_mpi x;
        int y_bit = pubkey[0] & 0x1;

        mbedtls_mpi_init(&x);
        if (mbedtls_mpi_read_binary(&x, &pubkey[1], pubkey_len - 1) ||
            _crypto_ec_point_solve_y_coord((struct crypto_ec *)&eckey->grp,
                          (struct crypto_ec_point *)&eckey->Q,
                          (struct crypto_bignum *)&x, y_bit))
            goto fail;
    }

    if (mbedtls_ecp_check_pubkey(&eckey->grp, &eckey->Q))
        goto fail;

    return key;
fail:
    _crypto_ec_key_deinit(key);
    return NULL;
}

/**
 * _crypto_ec_key_set_pub - Initialize an EC Public Key from EC point coordinates
 * @group: Identifying number for the ECC group
 * @x: X coordinate of the Public key
 * @y: Y coordinate of the Public key
 * @len: Length of @x and @y buffer
 * Returns: EC key or %NULL on failure
 *
 * This function initialize an EC Key from public key coordinates, in big endian
 * byte order padded to the length of the prime defining the group.
 */
struct crypto_ec_key * _crypto_ec_key_set_pub(int group, const uint8_t *x, const uint8_t *y, size_t len)
{
    struct crypto_ec_key *key;
    mbedtls_ecp_keypair *eckey;
    mbedtls_ecp_group_id grp_id;

    grp_id = mbedtls_get_group_id(group);
    if (grp_id < 0)
        return NULL;

    key = rtos_calloc(sizeof(struct crypto_ec_key), sizeof(uint8_t));
    if (!key)
        return NULL;
    mbedtls_pk_init(&key->pk);
    if (mbedtls_pk_setup(&key->pk,
                 mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)))
        goto fail;

    eckey = mbedtls_pk_ec(key->pk);

    if (mbedtls_ecp_group_load(&eckey->grp, grp_id) ||
        mbedtls_mpi_read_binary(&eckey->Q.X, x, len) ||
        mbedtls_mpi_read_binary(&eckey->Q.Y, y, len) ||
        mbedtls_mpi_lset(&eckey->Q.Z, 1) ||
        mbedtls_ecp_check_pubkey(&eckey->grp, &eckey->Q))
        goto fail;

    return key;
fail:
    _crypto_ec_key_deinit(key);
    return NULL;
}


/**
 * _crypto_ec_key_set_pub_point - Initialize an EC Public Key from EC point
 * @e: EC context from crypto_ec_init()
 * @pub: Public key point
 * Returns: EC key or %NULL on failure
 */
struct crypto_ec_key * _crypto_ec_key_set_pub_point(struct crypto_ec *e,
                           const struct crypto_ec_point *pub)
{
    struct crypto_ec_key *key;
    mbedtls_ecp_keypair *eckey;

    key = rtos_calloc(sizeof(struct crypto_ec_key), sizeof(uint8_t));
    if (!key)
        return NULL;
    mbedtls_pk_init(&key->pk);
    if (mbedtls_pk_setup(&key->pk,
                 mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)))
        goto fail;

    eckey = mbedtls_pk_ec(key->pk);

    if (mbedtls_ecp_group_load(&eckey->grp, e->group.id) ||
        mbedtls_ecp_copy(&eckey->Q, &pub->point) ||
        mbedtls_ecp_check_pubkey(&eckey->grp, &eckey->Q))
        goto fail;

    return key;
fail:
    _crypto_ec_key_deinit(key);
    return NULL;
}

/**
 * _crypto_ec_key_gen - Generate EC Key pair
 * @group: Identifying number for the ECC group
 * Returns: EC key or %NULL on failure
 */
struct crypto_ec_key * _crypto_ec_key_gen(int group,
                                        int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    struct crypto_ec_key *key;
    mbedtls_ecp_keypair *eckey;
    mbedtls_ecp_group_id grp_id;

    grp_id = mbedtls_get_group_id(group);
    if (grp_id < 0)
        return NULL;

    key = rtos_calloc(sizeof(struct crypto_ec_key), sizeof(uint8_t));
    if (!key)
        return NULL;
    mbedtls_pk_init(&key->pk);
    if (mbedtls_pk_setup(&key->pk,
                 mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)))
        goto fail;

    eckey = mbedtls_pk_ec(key->pk);
    if (mbedtls_ecp_gen_key(grp_id, eckey, f_rng, p_rng))
        goto fail;

    return key;
fail:
    _crypto_ec_key_deinit(key);
    return NULL;
}


/**
 * _crypto_ec_key_deinit - Free EC Key
 * @key: EC key from crypto_ec_key_parse_pub/priv() or crypto_ec_key_gen()
 */
void _crypto_ec_key_deinit(struct crypto_ec_key *key)
{
    if (key) {
        mbedtls_pk_free(&key->pk);
        rtos_free(key);
    }
}


/**
 * _crypto_ec_key_get_subject_public_key - Get SubjectPublicKeyInfo ASN.1 for a EC key
 * @key: EC key from crypto_ec_key_parse/set_pub/priv() or crypto_ec_key_gen()
 * Returns: Buffer with DER encoding of ASN.1 SubjectPublicKeyInfo or %NULL on failure
 */

size_t _crypto_ec_key_mbedtls_mpi_size(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);

    size_t len = mbedtls_mpi_size(&eckey->grp.P);
    return len;
}

int _crypto_ec_key_mbedtls_mpi_get_bit(struct crypto_ec_key *key, size_t pos)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return mbedtls_mpi_get_bit(&eckey->Q.Y, pos);

}

int _crypto_ec_key_get_grp_id(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return eckey->grp.id;
}

struct crypto_ec * _crypto_ec_key_get_grp(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return (struct crypto_ec *)&eckey->grp;
}

struct crypto_ec_point * _crypto_ec_key_get_q(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return (struct crypto_ec_point *)&eckey->Q;
}

int _crypto_ec_key_mbedtls_mpi_x_write_binary(struct crypto_ec_key *key,
                             unsigned char *buf, size_t buflen)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return mbedtls_mpi_write_binary(&eckey->Q.X, buf, buflen);
}

int _crypto_ec_key_mbedtls_mpi_y_write_binary(struct crypto_ec_key *key,
                             unsigned char *buf, size_t buflen)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return mbedtls_mpi_write_binary(&eckey->Q.Y, buf, buflen);
}

int _crypto_ec_key_mbedtls_mpi_d_write_binary(struct crypto_ec_key *key,
                             unsigned char *buf, size_t buflen)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return mbedtls_mpi_write_binary(&eckey->d, buf, buflen);
}


size_t _crypto_ec_key_get_mpi_num(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return eckey->d.n;
}

int _crypto_ec_key_point_to_bin(struct crypto_ec_key *key,
                                uint8_t *x, uint8_t *y)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return _crypto_ec_point_to_bin((struct crypto_ec *)&eckey->grp,
                       (struct crypto_ec_point *)&eckey->Q, x, y);
}


/**
 * _crypto_ec_key_get_public_key - Get EC Public Key as an EC point
 * @key: EC key from crypto_ec_key_parse/set_pub() or crypto_ec_key_parse_priv()
 * Returns: Public key a an EC point and %NULL on failure
 */
const struct crypto_ec_point *_crypto_ec_key_get_public_key(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return (const struct crypto_ec_point *)&eckey->Q;
}


/**
 * _crypto_ec_key_get_private_key - Get EC Private Key as a bignum
 * @key: EC key from crypto_ec_key_parse/set_pub() or crypto_ec_key_parse_priv()
 * Returns: private key as a bignum and %NULL on failure
 */
const struct crypto_bignum *_crypto_ec_key_get_private_key(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    return (const struct crypto_bignum *)&eckey->d;
}


/**
 * _crypto_ec_key_sign_r_s - Sign a buffer with an EC key
 * @key: EC key from crypto_ec_key_parse_priv() or crypto_ec_key_gen()
 * @data: Data to sign
 * @len: Length of @data buffer
 * Returns: Buffer with r and s value concatenated in a buffer. Each value
 * is in big endian byte order padded to the length of the prime defined the
 * group of the key.
 */
int _crypto_ec_key_sign_r_s(struct crypto_ec_key *key, const uint8_t *data,
                       size_t len, uint8_t *pbuf, size_t prime_len,
                       int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    mbedtls_mpi r, s;
    int ret = 0;

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    if (mbedtls_ecdsa_sign(&eckey->grp, &r, &s, &eckey->d, data, len,
                   f_rng, p_rng)) {
        ret = -1;
        goto fail;
    }

    if (mbedtls_mpi_write_binary(&r, pbuf, prime_len) ||
        mbedtls_mpi_write_binary(&s, pbuf, prime_len)) {
        ret = -1;
    }

fail:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    return ret;
}

/**
 * _crypto_ec_key_verify_signature - Verify signature
 * @key: EC key from crypto_ec_key_parse/set_pub() or crypto_ec_key_gen()
 * @data: Data to signed
 * @len: Length of @data buffer
 * @sig: DER encoding of ASN.1 Ecdsa-Sig-Value
 * @sig_len: Length of @sig buffer
 * Returns: 1 if signature is valid, 0 if signature is invalid and -1 on failure
 */
int _crypto_ec_key_verify_signature(struct crypto_ec_key *key, const uint8_t *data,
                   size_t len, const uint8_t *sig, size_t sig_len)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    int ret;

    ret = mbedtls_ecdsa_read_signature(eckey, data, len, sig, sig_len);
    if (ret == MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
        return 0;
    else if (ret)
        return -1;

    return 1;
}


/**
 * _crypto_ec_key_verify_signature_r_s - Verify signature
 * @key: EC key from crypto_ec_key_parse/set_pub() or crypto_ec_key_gen()
 * @data: Data to signed
 * @len: Length of @data buffer
 * @r: Binary data, in big endian byte order, of the 'r' field of the ECDSA signature.
 * @s: Binary data, in big endian byte order, of the 's' field of the ECDSA signature.
 * @r_len: Length of @r buffer
 * @s_len: Length of @s buffer
 * Returns: 1 if signature is valid, 0 if signature is invalid and -1 on failure
 */
int _crypto_ec_key_verify_signature_r_s(struct crypto_ec_key *key, const uint8_t *data,
                       size_t len,  const uint8_t *r, size_t r_len,
                       const uint8_t *s, size_t s_len)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);
    mbedtls_mpi mpi_r, mpi_s;
    int ret = -1;

    mbedtls_mpi_init(&mpi_r);
    mbedtls_mpi_init(&mpi_s);
    if (mbedtls_mpi_read_binary(&mpi_r, r, r_len) ||
        mbedtls_mpi_read_binary(&mpi_s, s, s_len))
        goto fail;


    ret = mbedtls_ecdsa_verify(&eckey->grp, data, len,
                   &eckey->Q, &mpi_r, &mpi_s);
    if (ret == MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
        ret = 0;
    else if (ret == 0)
        ret = 1;

fail:
    mbedtls_mpi_free(&mpi_r);
    mbedtls_mpi_free(&mpi_s);
    return ret;
}


/**
 * _crypto_ec_key_group - Get IANA group identifier for an EC key
 * @key: EC key from crypto_ec_key_parse/set_pub/priv() or crypto_ec_key_gen()
 * Returns: IANA group identifier and -1 on failure
 */
int _crypto_ec_key_group(struct crypto_ec_key *key)
{
    mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(key->pk);

    switch (eckey->grp.id) {
    case MBEDTLS_ECP_DP_SECP256R1:
        return 19;
    case MBEDTLS_ECP_DP_SECP384R1:
        return 20;
    case MBEDTLS_ECP_DP_SECP521R1:
        return 21;
    case MBEDTLS_ECP_DP_SECP192R1:
        return 26;
    case MBEDTLS_ECP_DP_BP256R1:
        return 28;
    case MBEDTLS_ECP_DP_BP384R1:
        return 29;
    case MBEDTLS_ECP_DP_BP512R1:
        return 30;
    default:
        return -1;
    }
}

/**
 * _crypto_ec_key_cmp - Compare 2 EC Public keys
 * @key1: Key 1
 * @key2: Key 2
 * Retruns: 0 if Public keys are identical, non-zero otherwise
 */
int _crypto_ec_key_cmp(struct crypto_ec_key *key1, struct crypto_ec_key *key2)
{
    mbedtls_ecp_keypair *eckey1 = mbedtls_pk_ec(key1->pk);
    mbedtls_ecp_keypair *eckey2 = mbedtls_pk_ec(key2->pk);

    return mbedtls_ecp_point_cmp(&eckey1->Q, &eckey2->Q);
}


/*
 * compatible function for mbedtls_ecdsa_write_signature
 */
int _mbedtls_ecdsa_write_signature(struct crypto_ec_key *key,
                                  const unsigned char *hash, size_t hlen,
                                  unsigned char *sig, size_t sig_size, size_t *slen,
                                  int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    mbedtls_ecp_keypair *ctx = mbedtls_pk_ec(key->pk);
    mbedtls_md_type_t md_alg;
    size_t prime_len = mbedtls_mpi_size(&ctx->grp.P);
    int ret;

    if (prime_len == 32)
        md_alg = MBEDTLS_MD_SHA256;
    else if (prime_len == 48)
        md_alg = MBEDTLS_MD_SHA384;
    else
        md_alg = MBEDTLS_MD_SHA512;


#if MBEDTLS_MAJOR_VER == 3
    ret = mbedtls_ecdsa_write_signature((mbedtls_ecdsa_context *)ctx, md_alg, hash, hlen, sig, sig_size, slen, f_rng, p_rng);
#else
    ret = mbedtls_ecdsa_write_signature((mbedtls_ecdsa_context *)ctx, md_alg, hash, hlen, sig, slen, f_rng, p_rng);
#endif
    return ret;
}

