# mbedTLS ECDSA 数字签名示例

## 功能说明

演示如何使用 mbedTLS 库进行 ECDSA（椭圆曲线数字签名算法）签名和验证操作。本示例展示了 ECDSA 密钥生成、消息哈希、签名生成和签名验证的完整流程。

ECDSA 是一种基于椭圆曲线的数字签名算法，提供与 RSA 相当的安全性，但使用更短的密钥长度。

## 硬件连接

无需外部连接，mbedTLS 为纯软件加密库。如果启用硬件加速（`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`），可以使用硬件加速的 ECDSA 验证。

## 示例内容

本示例测试所有支持的哈希算法（从 `MBEDTLS_MD_MD2` 到 `MBEDTLS_MD_RIPEMD160`）和所有支持的椭圆曲线（11 条曲线），对每个组合执行以下步骤：

1. 初始化随机数生成器（CTR_DRBG）和熵源
2. 计算消息的哈希值（使用指定的哈希算法）
3. 生成 ECDSA 密钥对（私钥和公钥）
4. 使用私钥对哈希值进行签名（生成 r 和 s 值）
5. 使用公钥验证签名

### 测试的哈希算法

- MD2, MD4, MD5
- SHA1
- SHA224, SHA256, SHA384, SHA512
- RIPEMD160

### 测试的椭圆曲线

- SECP192R1, SECP224R1, SECP256R1, SECP384R1, SECP521R1
- BP256R1, BP384R1, BP512R1
- SECP192K1, SECP224K1, SECP256K1

**注意**：代码中注释说明 CURVE25519 和 CURVE448 不能用于 ECDSA，因此不测试这两条曲线。

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
mbedtls ecdsa test
mbedtls ecdsa test begin

================= 1================

  . setup rng ... ok

  1. hash msg ... ok
  2. ecdsa generate keypair:
     04...
  3. ecdsa generate signature:
     ...
  4. ecdsa verify signature ... ok

================= 2================
...

mbedtls ecdsa test done
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 对每个哈希算法和椭圆曲线的组合都会输出测试编号
- 密钥对以十六进制格式输出（包含公钥和私钥）
- 签名（r 和 s 值）以十六进制格式输出
- 验证成功会显示 "ok"

## 核心 API

| API | 说明 |
|-----|------|
| `mbedtls_ecdsa_genkey()` | 生成 ECDSA 密钥对 |
| `mbedtls_ecdsa_sign()` | 使用私钥对哈希值进行签名 |
| `mbedtls_ecdsa_verify()` | 使用公钥验证签名 |
| `mbedtls_md()` | 计算消息的哈希值 |
| `mbedtls_ecp_point_write_binary()` | 将椭圆曲线点写入二进制格式 |
| `mbedtls_mpi_write_binary()` | 将大整数写入二进制格式 |
| `mbedtls_ctr_drbg_seed()` | 初始化 CTR_DRBG 随机数生成器 |

## 关键代码

```c
/* 计算消息哈希 */
mbedtls_md_context_t md_ctx;
uint8_t hash[64];
uint8_t msg[100];
memset(msg, 0x12, sizeof(msg));
mbedtls_md(mbedtls_md_info_from_type(md_type), msg, sizeof(msg), hash);

/* 生成 ECDSA 密钥对 */
mbedtls_ecdsa_context ctx;
mbedtls_ecdsa_init(&ctx);
mbedtls_ecdsa_genkey(&ctx, grp_id,
                    mbedtls_ctr_drbg_random, &ctr_drbg);

/* 签名 */
mbedtls_mpi r, s;
mbedtls_mpi_init(&r);
mbedtls_mpi_init(&s);
mbedtls_ecdsa_sign(&ctx.grp, &r, &s, &ctx.d, 
                  hash, hash_len, mbedtls_ctr_drbg_random, &ctr_drbg);

/* 验证 */
mbedtls_ecdsa_verify(&ctx.grp, hash, hash_len, &ctx.Q, &r, &s);
```

## 哈希算法说明

### 哈希长度

不同哈希算法产生不同长度的哈希值：

- **MD2, MD4, MD5, RIPEMD160**: 16 或 20 字节
- **SHA1, RIPEMD160**: 20 字节
- **SHA224**: 28 字节
- **SHA256**: 32 字节
- **SHA384**: 48 字节
- **SHA512**: 64 字节

代码中根据哈希算法类型自动确定哈希长度。

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_MBEDTLS=y`**: 启用 mbedTLS 模块
- **`CONFIG_MBEDTLS_ECP_C=y`**: 启用椭圆曲线支持
- **`CONFIG_MBEDTLS_ECDSA_C=y`**: 启用 ECDSA 支持
- **`CONFIG_MBEDTLS_ECP_ALL_ENABLED=y`**: 启用所有椭圆曲线
- **`CONFIG_MBEDTLS_HASH_ALL_ENABLED=y`**: 启用所有哈希算法

### 硬件加速配置（可选）

- **`CONFIG_MBEDTLS_HARDWARE_ENTROPY=y`**: 启用硬件熵源
- **`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`**: 启用 ECC 硬件加速（用于 ECDSA 验证）
- **`CONFIG_MBEDTLS_HARDWARE_MPI_MODULAR_EXPONENTIATION=y`**: 启用硬件模幂运算

## 注意事项

1. **哈希算法选择**: 实际应用中应使用安全的哈希算法（如 SHA256 或 SHA512），避免使用 MD2、MD4、MD5 等已被认为不安全的算法
2. **椭圆曲线选择**: 不同的椭圆曲线提供不同的安全级别，应根据实际需求选择合适的曲线
3. **消息哈希**: ECDSA 是对消息的哈希值进行签名，而不是直接对消息签名
4. **签名格式**: ECDSA 签名由两个大整数 r 和 s 组成，需要同时保存和传输
5. **随机数生成器**: 签名生成需要高质量的随机数，必须正确初始化熵源和 CTR_DRBG
6. **资源清理**: 使用完所有 mbedTLS 结构后，必须调用相应的 `free()` 函数释放资源
7. **测试范围**: 本示例测试所有支持的哈希算法和椭圆曲线组合，某些组合可能在某些平台上不支持
8. **曲线限制**: CURVE25519 和 CURVE448 不能用于 ECDSA，只能用于 ECDH 等其他用途

