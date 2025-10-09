#include <stdint.h>
#include <string.h>
#include "mbedtls/platform.h"
#include "mbedtls/error.h"
#include "mbedtls/bignum.h"

#include "csk_common.h"

static int csk_hardware_mpi_exp_mod(uint32_t *output, size_t output_len, 
                                    uint32_t *a, size_t a_len,
                                    uint32_t *e, size_t e_len, 
                                    uint32_t *n, size_t n_len)
{
    int ret = CSK_CRYPTO_OK;

    void *crypto_handler = csk_crypto_acquire_hardware(CSK_CRYPTO_TYPE_RSA);
    CSK_CRYPTO_CHECK_RET(crypto_handler != NULL, CSK_CRYPTO_ERR_HARDWARE_ACQUIRED);

    CRYPTO_Control(crypto_handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

#if CONFIG_DCACHE_ENABLE
    // dcache_clean_range((uint32_t)a, (uint32_t)a + a_len);
    // dcache_clean_range((uint32_t)e, (uint32_t)e + e_len);
    // dcache_clean_range((uint32_t)n, (uint32_t)n + n_len);
    // dcache_invalidate_range((uint32_t)output, (uint32_t)output + output_len);
#endif

    CSK_CRYPTO_CHK_EXIT(crypto_mod_exp(crypto_handler, output, NULL, a, a_len, e, e_len, n, n_len));

cleanup:
    csk_crypto_release_hardware(CSK_CRYPTO_TYPE_RSA);

    return ret;
}

/*
 * Sliding-window exponentiation: X = A^E mod N  (HAC 14.85)
 */
int csk_mpi_exp_mod(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *E, const mbedtls_mpi *N,
                        mbedtls_mpi *prec_RR)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n_len;
    size_t e_len;
    size_t a_len;
    uint8_t *csk_n = NULL;
    uint8_t *csk_e = NULL;
    uint8_t *csk_a = NULL;
    uint8_t *output = NULL;

    CSK_CRYPTO_CHECK_RET(X != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(A != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(E != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(N != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);

    (void)prec_RR;

    if (mbedtls_mpi_cmp_int(N, 0) <= 0 || (N->p[0] & 1) == 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_cmp_int(E, 0) < 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_bitlen(E) > MBEDTLS_MPI_MAX_BITS ||
        mbedtls_mpi_bitlen(N) > MBEDTLS_MPI_MAX_BITS) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    n_len = mbedtls_mpi_size(N);
    e_len = mbedtls_mpi_size(E);
    a_len = mbedtls_mpi_size(A);

    // mbedtls_printf("csk_mpi_exp_mod n_len = %d, e_len = %d, a_len = %d\n", n_len, e_len, a_len);

    csk_n =mbedtls_calloc(1, n_len);
    CSK_CRYPTO_ASSERT(csk_n != NULL);

    csk_e =mbedtls_calloc(1, e_len);
    CSK_CRYPTO_ASSERT(csk_e != NULL);

    csk_a =mbedtls_calloc(1, a_len);
    CSK_CRYPTO_ASSERT(csk_a != NULL);

    int max = (n_len > e_len) ? ((n_len > a_len) ? n_len : a_len) : ((e_len > a_len) ? e_len : a_len);
    output =mbedtls_calloc(1, max);
    CSK_CRYPTO_ASSERT(output != NULL);
    memset(output, 0, max);

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(N, csk_n, n_len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(E, csk_e, e_len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(A, csk_a, a_len));

    CSK_CRYPTO_CHK_EXIT(csk_hardware_mpi_exp_mod((uint32_t *)output, max, 
                                                 (uint32_t *)csk_a, a_len, 
                                                 (uint32_t *)csk_e, e_len, 
                                                 (uint32_t *)csk_n, n_len));


    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary_le(X, output, max));

cleanup:

    mbedtls_free(csk_n);
    mbedtls_free(csk_e);
    mbedtls_free(csk_a);
    mbedtls_free(output);

    return ret;
}
/*
 * Check parameters first
 */
static int csk_mpi_exp_mod_parameter_check(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *E, const mbedtls_mpi *N,
                        mbedtls_mpi *prec_RR)
{
    size_t n_len;
    size_t e_len;
    size_t a_len;
    size_t prec_len;
    int csk_crypto_rsa_max_len = CRYPTO_RSA_MAX_LEN / 8;

    CSK_CRYPTO_CHECK_RET(X != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(A != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(E != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    CSK_CRYPTO_CHECK_RET(N != NULL, MBEDTLS_ERR_MPI_BAD_INPUT_DATA);

    (void)prec_RR;

    if (mbedtls_mpi_cmp_int(N, 0) <= 0 || (N->p[0] & 1) == 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_cmp_int(E, 0) < 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_bitlen(E) > MBEDTLS_MPI_MAX_BITS ||
        mbedtls_mpi_bitlen(N) > MBEDTLS_MPI_MAX_BITS) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    n_len = mbedtls_mpi_size(N);
    e_len = mbedtls_mpi_size(E);
    a_len = mbedtls_mpi_size(A);
    prec_len = mbedtls_mpi_size(prec_RR);

    if ((n_len > csk_crypto_rsa_max_len) ||
        (e_len > csk_crypto_rsa_max_len) ||
        (a_len > csk_crypto_rsa_max_len) ||
        (prec_len > csk_crypto_rsa_max_len)) {
        /* 超过了硬件rsa最大支持长度 */
        return 0;
    } else {
        return 1;
    }
}

// 声明原始函数
int __real_mbedtls_mpi_exp_mod(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *E, const mbedtls_mpi *N,
                        mbedtls_mpi *prec_RR);

int __wrap_mbedtls_mpi_exp_mod(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *E, const mbedtls_mpi *N,
                        mbedtls_mpi *prec_RR)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = csk_mpi_exp_mod_parameter_check(X, A, E, N, prec_RR);
    if (ret < 0) {
        return ret;
    } else if (ret == 0) {
        return __real_mbedtls_mpi_exp_mod(X, A, E, N, prec_RR);
    } else if (ret == 1) {
        return csk_mpi_exp_mod(X, A, E, N, prec_RR);
    }
}
