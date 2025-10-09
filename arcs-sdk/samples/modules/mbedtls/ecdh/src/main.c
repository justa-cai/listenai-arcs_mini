#include <stdio.h>
#include <string.h>

#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"

#define assert_exit(cond, ret) \
    do { if (!(cond)) { \
        printf("  !. assert: failed [line: %d, error: -0x%04X]\n", __LINE__, -ret); \
        goto cleanup; \
    } } while (0)


static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    mbedtls_printf("%s", info);
    for (int i = 0; i < len; i++) {
        mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

static void sys_init(void)
{
    printf("Hello, world!\n");
    mbedtls_printf("mbedtls ecdh test\n");
}

int test_ecdh(mbedtls_ecp_group_id grp_id)
{
    int ret = 0;
    size_t olen;
    char buf[2048];
    mbedtls_ecp_group grp;
    mbedtls_mpi cli_secret, srv_secret;
    mbedtls_mpi cli_pri, srv_pri;
    mbedtls_ecp_point cli_pub, srv_pub;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    uint8_t *pers = "simple_ecdh";


    mbedtls_mpi_init(&cli_pri); 
    mbedtls_mpi_init(&srv_pri);
    mbedtls_mpi_init(&cli_secret); 
    mbedtls_mpi_init(&srv_secret);
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&cli_pub); 
    mbedtls_ecp_point_init(&srv_pub);
    mbedtls_entropy_init(&entropy); 
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, 
                                (const uint8_t *) pers, strlen(pers));
    mbedtls_printf("\n  . setup rng ... ok\n");

    ret = mbedtls_ecp_group_load(&grp, grp_id);
    mbedtls_printf("\n  . select ecp group %d ... ok\n", grp_id);

    ret = mbedtls_ecdh_gen_public(&grp, &cli_pri, &cli_pub, 
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(&grp, &cli_pub, 
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, sizeof(buf));
    dump_buf("  1. ecdh client generate public parameter:", buf, olen);

    ret = mbedtls_ecdh_gen_public(&grp, &srv_pri, &srv_pub, 
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(&grp, &srv_pub, 
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, sizeof(buf));
    dump_buf("  2. ecdh server generate public parameter:", buf, olen);

    ret = mbedtls_ecdh_compute_shared(&grp, &cli_secret, &srv_pub, &cli_pri, 
                                        mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_mpi_write_binary(&cli_secret, buf, mbedtls_mpi_size(&cli_secret));
    dump_buf("  3. ecdh client generate secret:", buf, mbedtls_mpi_size(&cli_secret));

    ret = mbedtls_ecdh_compute_shared(&grp, &srv_secret, &cli_pub, &srv_pri, 
                                        mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_mpi_write_binary(&srv_secret, buf, mbedtls_mpi_size(&srv_secret));
    dump_buf("  4. ecdh server generate secret:", buf, mbedtls_mpi_size(&srv_secret));

    ret = mbedtls_mpi_cmp_mpi(&cli_secret, &srv_secret);
    assert_exit(ret == 0, ret);
    mbedtls_printf("  5. ecdh checking secrets ... ok\n");

cleanup:
    mbedtls_mpi_free(&cli_pri); 
    mbedtls_mpi_free(&srv_pri);
    mbedtls_mpi_free(&cli_secret); 
    mbedtls_mpi_free(&srv_secret);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&cli_pub); 
    mbedtls_ecp_point_free(&srv_pub);
    mbedtls_entropy_free(&entropy); 
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return ret;
}


int main(void)
{
    int ret = 0;
    sys_init();

    mbedtls_printf("mbedtls ecdh test begin\n\n");

    for (int i = MBEDTLS_ECP_DP_SECP192R1; i < (MBEDTLS_ECP_DP_CURVE448 + 1); i++) {
        mbedtls_printf("=================%d================\n\n", i);
        ret = test_ecdh(i);
        if (ret != 0) {
            mbedtls_printf("mbedtls ecdh test failed, i = %d, ret = %d\n", i, ret);
            return ret;
        }
    }

    mbedtls_printf("mbedtls ecdh test end\n");

}