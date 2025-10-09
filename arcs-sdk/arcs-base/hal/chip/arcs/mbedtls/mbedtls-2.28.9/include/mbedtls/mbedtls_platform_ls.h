/**
 * \file mbedtls_platform.h
 *
 * \brief Contains header files needed to compile for ListenAI platform
 */
#ifndef MBEDTLS_PLATFORM_LS_H
#define MBEDTLS_PLATFORM_LS_H

#include "rtos_al.h"

/// Elliptic Curves supported by HSU
enum hsu_elliptic_curve
{
    /// 256-bit random ECP group (aka IANA IKE group 19)
    HSU_EC_256 = 0,
    /// 384-bit random ECP group (aka IANA IKE group 20)
    HSU_EC_384 = 1,
    /// 521-bit random ECP group (aka IANA IKE group 21)
    HSU_EC_521 = 2,
    /// Number of Elliptic Curve supported (keep last)
    HSU_EC_MAX,
};

bool hsu_ecp_mul(enum hsu_elliptic_curve curve, const uint32_t *k, size_t k_len,
                 const uint32_t *P[2], size_t P_len[2], uint32_t *R[2]);

bool hsu_mod_exp(const uint8_t *val, int val_len, const uint8_t *exponent, int exponent_len,
                 const uint8_t *modulus, int modulus_len,bool little_endian, uint8_t *res, int *res_len);

#endif // MBEDTLS_PLATFORM_LS_H
