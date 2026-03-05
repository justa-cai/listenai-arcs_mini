# mbedTLS ECDH 密钥交换示例

## 功能说明

演示如何使用 mbedTLS 库进行 ECDH（椭圆曲线 Diffie-Hellman）密钥交换。本示例展示了 ECDH 的完整流程，包括客户端和服务器端密钥对生成、公钥交换、共享密钥计算和验证。

ECDH 是一种密钥交换协议，允许两个通信方在不安全的信道上建立共享密钥，而无需预先共享密钥。

## 硬件连接

无需外部连接，mbedTLS 为纯软件加密库。如果启用硬件加速（`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`），可以使用硬件加速功能。

## 示例内容

本示例循环测试所有支持的椭圆曲线（从 `MBEDTLS_ECP_DP_SECP192R1` 到 `MBEDTLS_ECP_DP_CURVE448`），对每条曲线执行以下步骤：

1. 初始化随机数生成器（CTR_DRBG）和熵源
2. 加载椭圆曲线组
3. 客户端生成密钥对（私钥和公钥）
4. 服务器端生成密钥对（私钥和公钥）
5. 客户端使用服务器公钥和客户端私钥计算共享密钥
6. 服务器端使用客户端公钥和服务器私钥计算共享密钥
7. 验证客户端和服务器端计算的共享密钥是否相同

## 编译运行

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
********Arcs SDK 0.1.0 @ v0.0.23.temp.docs-96-gf56c5084660d********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
Hello, world!
mbedtls ecdh test
mbedtls ecdh test begin

=================0================

  . setup rng ... ok

  . select ecp group 0 ... ok
  1. ecdh client generate public parameter:
     04...
  2. ecdh server generate public parameter:
     04...
  3. ecdh client generate secret:
     ...
  4. ecdh server generate secret:
     ...
  5. ecdh checking secrets ... ok

=================1================
...

mbedtls ecdh test end
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 对每条椭圆曲线都会输出测试编号和测试结果
- 客户端和服务器端的公钥参数以十六进制格式输出
- 共享密钥以十六进制格式输出
- 最后验证两个共享密钥是否相同

## 核心 API

| API | 说明 |
|-----|------|
| `mbedtls_ecp_group_load()` | 加载椭圆曲线组 |
| `mbedtls_ecdh_gen_public()` | 生成 ECDH 密钥对（私钥和公钥） |
| `mbedtls_ecdh_compute_shared()` | 计算共享密钥 |
| `mbedtls_ecp_point_write_binary()` | 将椭圆曲线点写入二进制格式 |
| `mbedtls_mpi_write_binary()` | 将大整数写入二进制格式 |
| `mbedtls_mpi_cmp_mpi()` | 比较两个大整数 |
| `mbedtls_ctr_drbg_seed()` | 初始化 CTR_DRBG 随机数生成器 |
| `mbedtls_entropy_init()` | 初始化熵源 |

## 关键代码

```c
/* 初始化随机数生成器 */
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_entropy_init(&entropy);
mbedtls_ctr_drbg_init(&ctr_drbg);
mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, 
                      (const uint8_t *) pers, strlen(pers));

/* 加载椭圆曲线组 */
mbedtls_ecp_group grp;
mbedtls_ecp_group_init(&grp);
mbedtls_ecp_group_load(&grp, grp_id);

/* 客户端生成密钥对 */
mbedtls_mpi cli_pri;
mbedtls_ecp_point cli_pub;
mbedtls_mpi_init(&cli_pri);
mbedtls_ecp_point_init(&cli_pub);
mbedtls_ecdh_gen_public(&grp, &cli_pri, &cli_pub, 
                        mbedtls_ctr_drbg_random, &ctr_drbg);

/* 服务器端生成密钥对 */
mbedtls_mpi srv_pri;
mbedtls_ecp_point srv_pub;
mbedtls_mpi_init(&srv_pri);
mbedtls_ecp_point_init(&srv_pub);
mbedtls_ecdh_gen_public(&grp, &srv_pri, &srv_pub, 
                        mbedtls_ctr_drbg_random, &ctr_drbg);

/* 客户端计算共享密钥 */
mbedtls_mpi cli_secret;
mbedtls_mpi_init(&cli_secret);
mbedtls_ecdh_compute_shared(&grp, &cli_secret, &srv_pub, &cli_pri, 
                            mbedtls_ctr_drbg_random, &ctr_drbg);

/* 服务器端计算共享密钥 */
mbedtls_mpi srv_secret;
mbedtls_mpi_init(&srv_secret);
mbedtls_ecdh_compute_shared(&grp, &srv_secret, &cli_pub, &srv_pri, 
                            mbedtls_ctr_drbg_random, &ctr_drbg);

/* 验证共享密钥是否相同 */
int ret = mbedtls_mpi_cmp_mpi(&cli_secret, &srv_secret);
if (ret == 0) {
    mbedtls_printf("  5. ecdh checking secrets ... ok\n");
}
```

## 椭圆曲线说明

本示例测试所有 mbedTLS 支持的椭圆曲线，包括：

- **SECP 曲线**：SECP192R1, SECP224R1, SECP256R1, SECP384R1, SECP521R1
- **SECP Koblitz 曲线**：SECP192K1, SECP224K1, SECP256K1
- **Brainpool 曲线**：BP256R1, BP384R1, BP512R1
- **其他曲线**：CURVE25519, CURVE448

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_MBEDTLS=y`**: 启用 mbedTLS 模块
- **`CONFIG_MBEDTLS_ECP_C=y`**: 启用椭圆曲线支持
- **`CONFIG_MBEDTLS_ECDH_C=y`**: 启用 ECDH 支持
- **`CONFIG_MBEDTLS_ECP_ALL_ENABLED=y`**: 启用所有椭圆曲线
- **`CONFIG_MBEDTLS_HARDWARE_ENTROPY=y`**: 启用硬件熵源（可选）

### 硬件加速配置（可选）

- **`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`**: 启用 ECC 硬件加速（用于 ECDSA 验证，可能对 ECDH 也有帮助）

## 注意事项

1. **随机数生成器**: ECDH 密钥生成需要高质量的随机数，必须正确初始化熵源和 CTR_DRBG
2. **资源清理**: 使用完所有 mbedTLS 结构后，必须调用相应的 `free()` 函数释放资源
3. **椭圆曲线选择**: 不同的椭圆曲线提供不同的安全级别和性能，应根据实际需求选择
4. **公钥格式**: 公钥以未压缩格式（`MBEDTLS_ECP_PF_UNCOMPRESSED`）输出，包含完整的点坐标
5. **共享密钥**: 计算出的共享密钥是原始的大整数，实际使用时可能需要进一步处理（如密钥派生）
6. **测试范围**: 本示例测试所有支持的椭圆曲线，某些曲线可能在某些平台上不支持或性能较差
7. **错误处理**: 代码使用 `assert_exit` 宏进行错误处理，实际应用中应使用更完善的错误处理机制

