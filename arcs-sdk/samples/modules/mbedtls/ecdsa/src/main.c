#include <stdio.h>
#include <string.h>

#include "mbedtls/ecdsa.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"

#define assert_exit(cond, ret) \
    do { if (!(cond)) { \
        printf("  !. assert: failed [line: %d, error: -0x%04X]\n", __LINE__, -ret); \
        goto cleanup; \
    } } while (0)

#define assert_return(cond, ret) \
    do { if (!(cond)) { \
        printf("  !. assert: failed [line: %d, error: -0x%04X]\n", __LINE__, -ret); \
        return ret; \
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
    mbedtls_printf("mbedtls ecdsa test\n");
}

int test_ecdsa(mbedtls_ecp_group_id grp_id, mbedtls_md_type_t md_type)
{
    int ret = 0;
    char buf[2048];
    uint8_t hash[64], msg[100];
    uint8_t *pers = "simple_ecdsa";
    size_t rlen, slen, qlen, dlen;
    memset(msg, 0x12, sizeof(msg));
    uint8_t hash_len = 0;

    mbedtls_mpi r, s;
    mbedtls_ecdsa_context ctx;
    mbedtls_md_context_t md_ctx;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_ecdsa_init(&ctx);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, 
                                (const uint8_t *) pers, strlen(pers));
    assert_exit(ret == 0, ret);
    mbedtls_printf("\n  . setup rng ... ok\n\n");

    mbedtls_md_init(&md_ctx);
    mbedtls_md(mbedtls_md_info_from_type(md_type), msg, sizeof(msg), hash);
    mbedtls_printf("  1. hash msg ... ok\n");

    ret = mbedtls_ecdsa_genkey(&ctx, grp_id,
                              mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(&ctx.grp, &ctx.Q, 
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &qlen, buf, sizeof(buf));
    dlen = mbedtls_mpi_size(&ctx.d);
    mbedtls_mpi_write_binary(&ctx.d, buf + qlen, dlen);
    dump_buf("  2. ecdsa generate keypair:", buf, qlen + dlen);

    switch (md_type) {
        case MBEDTLS_MD_MD2:
        case MBEDTLS_MD_MD4:
        case MBEDTLS_MD_MD5:
            hash_len = 16;
            break;
        case MBEDTLS_MD_SHA1:
            hash_len = 20;
            break;
        case MBEDTLS_MD_SHA224:
            hash_len = 28;
            break;
        case MBEDTLS_MD_SHA256:
            hash_len = 32;
            break;
        case MBEDTLS_MD_SHA384:
            hash_len = 48;
            break;
        case MBEDTLS_MD_SHA512:
            hash_len = 64;
            break;
        case MBEDTLS_MD_RIPEMD160:
            hash_len = 20;
            break;
        default:
            break;
    }

    ret = mbedtls_ecdsa_sign(&ctx.grp, &r, &s, &ctx.d, 
                        hash, hash_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    rlen = mbedtls_mpi_size(&r);
    slen = mbedtls_mpi_size(&s);
    mbedtls_mpi_write_binary(&r, buf, rlen);
    mbedtls_mpi_write_binary(&s, buf + rlen, slen);
    dump_buf("  3. ecdsa generate signature:", buf, rlen + slen);

    ret = mbedtls_ecdsa_verify(&ctx.grp, hash, hash_len, &ctx.Q, &r, &s);
    assert_exit(ret == 0, ret);
    mbedtls_printf("  4. ecdsa verify signature ... ok\n\n");

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_md_free(&md_ctx);
    mbedtls_ecdsa_free(&ctx);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}


int main(void)
{
    int ret = 0;
    sys_init();
    int total = 11;

    mbedtls_printf("mbedtls ecdsa test begin\n\n");

    for (int i = MBEDTLS_MD_MD2; i < (MBEDTLS_MD_RIPEMD160 + 1); i++) {
        mbedtls_printf("=================%2d================\n\n", i * total + 1);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP192R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 2);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP224R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 3);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP256R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 4);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP384R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 5);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP521R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 6);

        ret = test_ecdsa(MBEDTLS_ECP_DP_BP256R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 7);
        ret = test_ecdsa(MBEDTLS_ECP_DP_BP384R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 8);
        ret = test_ecdsa(MBEDTLS_ECP_DP_BP512R1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 9);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP192K1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 10);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP224K1, i);
        assert_return(ret == 0, ret);
        mbedtls_printf("=================%2d================\n\n", i * total + 11);
        ret = test_ecdsa(MBEDTLS_ECP_DP_SECP256K1, i);
        assert_return(ret == 0, ret);

        // /* 下面两种曲线不能用于ECDSA，所以会验证失败 */
        // mbedtls_printf("=================%d================\n\n", i * total + 12);
        // test_ecdsa(MBEDTLS_ECP_DP_CURVE25519, i);
        // mbedtls_printf("=================%d================\n\n", i * total + 13);
        // test_ecdsa(MBEDTLS_ECP_DP_CURVE448, i);
    }

    mbedtls_printf("mbedtls ecdsa test done\n");
    return ret;
}