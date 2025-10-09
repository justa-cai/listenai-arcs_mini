#include <stdint.h>
#include <string.h>

#include "mbedtls/platform.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecp_internal.h"

#include "csk_common.h"

static int csk_ecp_load_curve_parameter(mbedtls_ecp_group *grp, CRYPTO_ECC_CURVE *curve)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    mbedtls_mpi A_neg3;
    mbedtls_mpi_init(&A_neg3);

    curve->param_len = grp->pbits / 8;
    curve->cofactor = 1;
    curve->param_flag = 0;

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&grp->P, curve->param, curve->param_len));

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

static int csk_ecp_mul(CRYPTO_ECC_CURVE *curve, uint32_t *result, uint32_t *point, uint32_t *mult)
{
    int ret = CSK_CRYPTO_OK;

    void *crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_ECC);
    CSK_CRYPTO_CHECK_RET(crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

    CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    CSK_CRYPTO_CHK_EXIT(CRYPTO_ECC_Build_Curve(crypto_handler, curve));

    CSK_CRYPTO_CHK_EXIT(CRYPTO_ECC_Multiply(crypto_handler, result, point, mult));

cleanup:
    csk_crypto_release_hardware(CSK_CRYPTO_TYPE_ECC);

    return ret;
}

/*
 * Restartable multiplication R = m * P
 */
int csk_ecp_mul_restartable(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                                const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                                int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    CRYPTO_ECC_CURVE *curve = NULL;
    uint8_t *point = NULL;
    uint8_t *mult = NULL;
    uint8_t *result = NULL;
    int mod_bits = grp->pbits / 8;

    curve = mbedtls_calloc(1, sizeof(CRYPTO_ECC_CURVE) + mod_bits * 8);
    CSK_CRYPTO_ASSERT(curve != NULL);

    point = mbedtls_calloc(1, mod_bits * 2);
    CSK_CRYPTO_ASSERT(point != NULL);

    mult = mbedtls_calloc(1, mod_bits);
    CSK_CRYPTO_ASSERT(mult != NULL);

    result = mbedtls_calloc(1, mod_bits * 2);
    CSK_CRYPTO_ASSERT(result != NULL);

    CSK_CRYPTO_CHK_EXIT(csk_ecp_load_curve_parameter(grp, curve));

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(m, mult, mod_bits));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&P->X, point, mod_bits));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&P->Y, point + mod_bits, mod_bits));

    CSK_CRYPTO_CHK_EXIT(csk_ecp_mul(curve, (uint32_t *)result, (uint32_t *)point, (uint32_t *)mult));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary_le(&R->X, result, mod_bits));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary_le(&R->Y, result + mod_bits, mod_bits));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&R->Z, 1));

cleanup:
    mbedtls_free(result);
    mbedtls_free(mult);
    mbedtls_free(point);
    mbedtls_free(curve);
    
    return ret;

}

// 声明原始函数
int __real_mbedtls_ecp_mul_restartable(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                                const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                                int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                mbedtls_ecp_restart_ctx *rs_ctx);

int __wrap_mbedtls_ecp_mul_restartable(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                                const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                                int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    CSK_CRYPTO_CHECK_RET(grp != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(R   != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(m   != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(P   != NULL, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((grp->id == MBEDTLS_ECP_DP_SECP192R1) 
            || (grp->id == MBEDTLS_ECP_DP_SECP224R1)
            || (grp->id == MBEDTLS_ECP_DP_SECP256R1)
            || (grp->id == MBEDTLS_ECP_DP_SECP384R1)
            || (grp->id == MBEDTLS_ECP_DP_BP256R1)
            || (grp->id == MBEDTLS_ECP_DP_BP384R1)
            || (grp->id == MBEDTLS_ECP_DP_BP512R1)
            || (grp->id == MBEDTLS_ECP_DP_SECP192K1)
            || (grp->id == MBEDTLS_ECP_DP_SECP256K1)) {

        /* Common sanity checks */
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_privkey(grp, m));
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));

        ret = csk_ecp_mul_restartable(grp, R, m, P, f_rng, p_rng, rs_ctx);
    } else {
        return __real_mbedtls_ecp_mul_restartable(grp, R, m, P, f_rng, p_rng, rs_ctx);
    }

cleanup:
    return ret;
}
