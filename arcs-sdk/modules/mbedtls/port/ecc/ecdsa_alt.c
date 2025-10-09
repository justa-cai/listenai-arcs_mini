#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECDSA_VERIFY_ALT)

#include <stdint.h>
#include <string.h>
#include "mbedtls/platform.h"
#include "mbedtls/error.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecp_internal.h"
#include "mbedtls/ecdsa.h"

#include "csk_common.h"

#if defined(MBEDTLS_ECP_RESTARTABLE)

/*
 * Sub-context for ecdsa_verify()
 */
struct mbedtls_ecdsa_restart_ver {
    mbedtls_mpi u1, u2;     /* intermediate values  */
    enum {                  /* what to do next?     */
        ecdsa_ver_init = 0, /* getting started      */
        ecdsa_ver_muladd,   /* muladd step          */
    } state;
};

/*
 * Init verify restart sub-context
 */
static void ecdsa_restart_ver_init(mbedtls_ecdsa_restart_ver_ctx *ctx)
{
    mbedtls_mpi_init(&ctx->u1);
    mbedtls_mpi_init(&ctx->u2);
    ctx->state = ecdsa_ver_init;
}

/*
 * Free the components of a verify restart sub-context
 */
static void ecdsa_restart_ver_free(mbedtls_ecdsa_restart_ver_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_mpi_free(&ctx->u1);
    mbedtls_mpi_free(&ctx->u2);

    ecdsa_restart_ver_init(ctx);
}

#define CSK_ECDSA_RS_ECP    (rs_ctx == NULL ? NULL : &rs_ctx->ecp)

/* Utility macro for checking and updating ops budget */
#define CSK_ECDSA_BUDGET(ops)   \
    MBEDTLS_MPI_CHK(mbedtls_ecp_check_budget(grp, CSK_ECDSA_RS_ECP, ops));

/* Call this when entering a function that needs its own sub-context */
#define CSK_ECDSA_RS_ENTER(SUB)   do {                                 \
        /* reset ops count for this call if top-level */                 \
        if (rs_ctx != NULL && rs_ctx->ecp.depth++ == 0)                 \
        rs_ctx->ecp.ops_done = 0;                                    \
                                                                     \
        /* set up our own sub-context if needed */                       \
        if (mbedtls_ecp_restart_is_enabled() &&                          \
            rs_ctx != NULL && rs_ctx->SUB == NULL)                      \
        {                                                                \
            rs_ctx->SUB = mbedtls_calloc(1, sizeof(*rs_ctx->SUB));   \
            if (rs_ctx->SUB == NULL)                                    \
            return MBEDTLS_ERR_ECP_ALLOC_FAILED;                  \
                                                                   \
            ecdsa_restart_## SUB ##_init(rs_ctx->SUB);                 \
        }                                                                \
} while (0)

/* Call this when leaving a function that needs its own sub-context */
#define CSK_ECDSA_RS_LEAVE(SUB)   do {                                 \
        /* clear our sub-context when not in progress (done or error) */ \
        if (rs_ctx != NULL && rs_ctx->SUB != NULL &&                     \
            ret != MBEDTLS_ERR_ECP_IN_PROGRESS)                         \
        {                                                                \
            ecdsa_restart_## SUB ##_free(rs_ctx->SUB);                 \
            mbedtls_free(rs_ctx->SUB);                                 \
            rs_ctx->SUB = NULL;                                          \
        }                                                                \
                                                                     \
        if (rs_ctx != NULL)                                             \
        rs_ctx->ecp.depth--;                                         \
} while (0)

#else /* MBEDTLS_ECP_RESTARTABLE */

#define CSK_ECDSA_RS_ECP    NULL

#define CSK_ECDSA_BUDGET(ops)     /* no-op; for compatibility */

#define CSK_ECDSA_RS_ENTER(SUB)   (void) rs_ctx
#define CSK_ECDSA_RS_LEAVE(SUB)   (void) rs_ctx

#endif /* MBEDTLS_ECP_RESTARTABLE */

/*
 * Derive a suitable integer for group grp from a buffer of length len
 * SEC1 4.1.3 step 5 aka SEC1 4.1.4 step 3
 */
static int derive_mpi(const mbedtls_ecp_group *grp, mbedtls_mpi *x,
                      const unsigned char *buf, size_t blen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n_size = (grp->nbits + 7) / 8;
    size_t use_size = blen > n_size ? n_size : blen;

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(x, buf, use_size));
    if (use_size * 8 > grp->nbits) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(x, use_size * 8 - grp->nbits));
    }

    /* While at it, reduce modulo N */
    if (mbedtls_mpi_cmp_mpi(x, &grp->N) >= 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(x, x, &grp->N));
    }

cleanup:
    return ret;
}

/*
 * Verify ECDSA signature of hashed message (SEC1 4.1.4)
 * Obviously, compared to SEC1 4.1.3, we skip step 2 (hash message)
 */
static int ecdsa_verify_restartable(mbedtls_ecp_group *grp,
                                    const unsigned char *buf, size_t blen,
                                    const mbedtls_ecp_point *Q,
                                    const mbedtls_mpi *r, const mbedtls_mpi *s,
                                    mbedtls_ecdsa_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi e, s_inv, u1, u2;
    mbedtls_ecp_point R;
    mbedtls_mpi *pu1 = &u1, *pu2 = &u2;

    mbedtls_ecp_point_init(&R);
    mbedtls_mpi_init(&e); mbedtls_mpi_init(&s_inv);
    mbedtls_mpi_init(&u1); mbedtls_mpi_init(&u2);

    /* Fail cleanly on curves such as Curve25519 that can't be used for ECDSA */
    if ((grp->id == MBEDTLS_ECP_DP_CURVE25519) 
        || (grp->id == MBEDTLS_ECP_DP_CURVE448) 
        || grp->N.p == NULL) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    CSK_ECDSA_RS_ENTER(ver);

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ver != NULL) {
        /* redirect to our context */
        pu1 = &rs_ctx->ver->u1;
        pu2 = &rs_ctx->ver->u2;

        /* jump to current step */
        if (rs_ctx->ver->state == ecdsa_ver_muladd) {
            goto muladd;
        }
    }
#endif /* MBEDTLS_ECP_RESTARTABLE */

    /*
     * Step 1: make sure r and s are in range 1..n-1
     */
    if (mbedtls_mpi_cmp_int(r, 1) < 0 || mbedtls_mpi_cmp_mpi(r, &grp->N) >= 0 ||
        mbedtls_mpi_cmp_int(s, 1) < 0 || mbedtls_mpi_cmp_mpi(s, &grp->N) >= 0) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

    /*
     * Step 3: derive MPI from hashed message
     */
    MBEDTLS_MPI_CHK(derive_mpi(grp, &e, buf, blen));

    /*
     * Step 4: u1 = e / s mod n, u2 = r / s mod n
     */
    CSK_ECDSA_BUDGET(MBEDTLS_ECP_OPS_CHK + MBEDTLS_ECP_OPS_INV + 2);

    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&s_inv, s, &grp->N));

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(pu1, &e, &s_inv));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(pu1, pu1, &grp->N));

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(pu2, r, &s_inv));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(pu2, pu2, &grp->N));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ver != NULL) {
        rs_ctx->ver->state = ecdsa_ver_muladd;
    }

muladd:
#endif
    /*
     * Step 5: R = u1 G + u2 Q
     */
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd_restartable(grp,
                                                   &R, pu1, &grp->G, pu2, Q, CSK_ECDSA_RS_ECP));

    if (mbedtls_ecp_is_zero(&R)) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

    /*
     * Step 6: convert xR to an integer (no-op)
     * Step 7: reduce xR mod n (gives v)
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&R.X, &R.X, &grp->N));

    /*
     * Step 8: check if v (that is, R.X) is equal to r
     */
    if (mbedtls_mpi_cmp_mpi(&R.X, r) != 0) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free(&R);
    mbedtls_mpi_free(&e); mbedtls_mpi_free(&s_inv);
    mbedtls_mpi_free(&u1); mbedtls_mpi_free(&u2);

    CSK_ECDSA_RS_LEAVE(ver);

    return ret;
}

static int csk_ecp_load_curve_parameter(mbedtls_ecp_group *grp, CRYPTO_ECC_CURVE *curve)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    mbedtls_mpi A_neg3;
    mbedtls_mpi_init(&A_neg3);

    curve->param_len = grp->pbits / 8;
    curve->cofactor = 1;
    curve->param_flag = 0;

    mbedtls_mpi_write_binary_le(&grp->P, curve->param, curve->param_len);
    if (grp->A.p == NULL) { 
        // 设置 A = p - 3
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&A_neg3, &grp->P));  // A = p
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&A_neg3, &A_neg3, 3));  // A = p - 3
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&A_neg3, curve->param + curve->param_len, curve->param_len));
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&grp->A, curve->param + curve->param_len, curve->param_len));
    }
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&grp->B, curve->param + 2 * curve->param_len, curve->param_len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&grp->G.X, curve->param + 3 * curve->param_len, curve->param_len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&grp->G.Y, curve->param + 4 * curve->param_len, curve->param_len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&grp->N, curve->param + 5 * curve->param_len, curve->param_len));

    ret = 0;

cleanup:
    mbedtls_mpi_free(&A_neg3);
    return ret;
}

static int csk_ecp_ecdsa_verify(CRYPTO_ECC_CURVE *curve,
                            uint32_t *digest,
                            uint32_t *public,
                            uint32_t *sign)
{
    int ret = CSK_CRYPTO_OK;

    void *crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_ECC);
    CSK_CRYPTO_CHECK_RET(crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

    CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    CSK_CRYPTO_CHK_EXIT(CRYPTO_ECC_Build_Curve(crypto_handler, curve));

    CSK_CRYPTO_CHK_EXIT(CRYPTO_ECSDA_Verify_Signature(crypto_handler, digest, public, sign));

cleanup:
    csk_crypto_release_hardware(CSK_CRYPTO_TYPE_ECC);

    return ret;
}

static int csk_ecdsa_verify_restartable(mbedtls_ecp_group *grp,
                                    const unsigned char *buf, size_t blen,
                                    const mbedtls_ecp_point *Q,
                                    const mbedtls_mpi *r, const mbedtls_mpi *s,
                                    mbedtls_ecdsa_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    CRYPTO_ECC_CURVE *curve = NULL;
    uint8_t *public = NULL;
    uint8_t *sign = NULL;
    uint8_t *digest = NULL;
    uint32_t mod_bits = grp->pbits / 8;

    mbedtls_mpi e;
    mbedtls_mpi_init(&e);

    curve = mbedtls_calloc(1, sizeof(CRYPTO_ECC_CURVE) + mod_bits * 8);
    CSK_CRYPTO_ASSERT(curve != NULL);

    public = mbedtls_calloc(1, mod_bits * 2);
    CSK_CRYPTO_ASSERT(public != NULL);

    sign = mbedtls_calloc(1, mod_bits * 2);
    CSK_CRYPTO_ASSERT(sign != NULL);

    digest = mbedtls_calloc(1, mod_bits);
    CSK_CRYPTO_ASSERT(digest != NULL);

    CSK_CRYPTO_CHK_EXIT(csk_ecp_load_curve_parameter(grp, curve));

    MBEDTLS_MPI_CHK(derive_mpi(grp, &e, buf, blen));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&e, digest, mod_bits));

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&Q->X, public, mod_bits));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&Q->Y, public + mod_bits, mod_bits));

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(r, sign, mod_bits));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(s, sign + mod_bits, mod_bits));

    CSK_CRYPTO_CHK_EXIT(csk_ecp_ecdsa_verify(curve, (uint32_t *)digest, (uint32_t *)public, (uint32_t *)sign));

cleanup:
    mbedtls_free(digest);
    mbedtls_free(sign);
    mbedtls_free(public);
    mbedtls_free(curve);
    mbedtls_mpi_free(&e);

    return ret;
}

/*
 * Verify ECDSA signature of hashed message
 */
int mbedtls_ecdsa_verify(mbedtls_ecp_group *grp,
                         const unsigned char *buf, size_t blen,
                         const mbedtls_ecp_point *Q, const mbedtls_mpi *r,
                         const mbedtls_mpi *s)
{
    CSK_CRYPTO_CHECK_RET(grp != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(Q   != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(r   != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(s   != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(buf != NULL || blen == 0, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((grp->id == MBEDTLS_ECP_DP_SECP192R1) 
            || (grp->id == MBEDTLS_ECP_DP_SECP224R1)
            || (grp->id == MBEDTLS_ECP_DP_SECP256R1)
            || (grp->id == MBEDTLS_ECP_DP_SECP384R1)
            || (grp->id == MBEDTLS_ECP_DP_BP256R1)
            || (grp->id == MBEDTLS_ECP_DP_BP384R1)
            || (grp->id == MBEDTLS_ECP_DP_BP512R1)
            || (grp->id == MBEDTLS_ECP_DP_SECP192K1)
            || (grp->id == MBEDTLS_ECP_DP_SECP256K1)) {
        return csk_ecdsa_verify_restartable(grp, buf, blen, Q, r, s, NULL);
    } else {
        return ecdsa_verify_restartable(grp, buf, blen, Q, r, s, NULL);
    }

}

#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECDSA_VERIFY_ALT */