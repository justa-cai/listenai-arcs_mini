# mbedTLS RSA 加密解密示例

## 功能说明

演示如何使用 mbedTLS 库进行 RSA 加密、解密、签名和验证操作。本示例展示了 RSA 密钥的导入（或生成）、PKCS#1 加密解密、以及基于 SHA1 的签名和验证功能。

RSA 是一种非对称加密算法，广泛用于数据加密和数字签名。

## 硬件连接

无需外部连接，mbedTLS 为纯软件加密库。如果启用硬件加速（`CONFIG_MBEDTLS_HARDWARE_MPI_MODULAR_EXPONENTIATION=y`），可以使用硬件加速的模幂运算。

## 示例内容

1. 初始化随机数生成器（CTR_DRBG）和熵源
2. 初始化 RSA 上下文（使用 PKCS#1 v2.1 填充和 SHA256）
3. **密钥处理**（根据 `IS_GENERATE_KEY` 宏）：
   - 如果 `IS_GENERATE_KEY == 0`：从预定义的十六进制字符串导入 RSA 密钥（支持 1024、2048、4096、8192 位）
   - 如果 `IS_GENERATE_KEY == 1`：生成新的 2048 位 RSA 密钥对
4. 打印 RSA 密钥对信息（N, E, D, P, Q, DP, DQ, QP）
5. **加密解密测试**：
   - 使用公钥加密消息 "Hello, World!"
   - 使用私钥解密加密后的数据
   - 验证解密结果与原始消息一致
6. **签名验证测试**：
   - 计算消息的 SHA1 哈希值
   - 使用私钥对哈希值进行签名
   - 使用公钥验证签名

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
mbedtls rsa test

  . setup rng ... ok

  1. RSA generate key ... ok

  +++++++++++++++++ rsa keypair +++++++++++++++++

N: [RSA 模数，十六进制]
E: [公钥指数，十六进制]
D: [私钥指数，十六进制]
P: [质数 P，十六进制]
Q: [质数 Q，十六进制]
DP: [DP 值，十六进制]
DQ: [DQ 值，十六进制]
QP: [QP 值，十六进制]

  +++++++++++++++++ rsa keypair +++++++++++++++++

  2. RSA encryption ... ok
     [加密后的数据，十六进制]

  3. RSA decryption ... ok
     Hello, World!

  4. RSA Compare results and plaintext ... ok

  5. PKCS#1 data sign  : passed
  PKCS#1 sig. verify: all passed
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- RSA 密钥对信息以十六进制格式输出
- 加密后的数据以十六进制格式输出
- 解密后的明文直接输出
- 签名和验证操作的结果会显示

## 核心 API

| API | 说明 |
|-----|------|
| `mbedtls_rsa_init()` | 初始化 RSA 上下文 |
| `mbedtls_rsa_gen_key()` | 生成 RSA 密钥对 |
| `mbedtls_rsa_import()` | 导入 RSA 密钥组件 |
| `mbedtls_rsa_complete()` | 完成 RSA 密钥设置（计算 DP, DQ, QP） |
| `mbedtls_rsa_pkcs1_encrypt()` | PKCS#1 加密 |
| `mbedtls_rsa_pkcs1_decrypt()` | PKCS#1 解密 |
| `mbedtls_rsa_pkcs1_sign()` | PKCS#1 签名 |
| `mbedtls_rsa_pkcs1_verify()` | PKCS#1 验证 |
| `mbedtls_sha1_ret()` | 计算 SHA1 哈希值 |

## 关键代码

```c
/* 初始化 RSA 上下文 */
mbedtls_rsa_context ctx;
mbedtls_rsa_init(&ctx, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

/* 导入 RSA 密钥（从十六进制字符串） */
mbedtls_mpi N, E, D, P, Q, DP, DQ, QP;
mbedtls_mpi_init(&N);
mbedtls_mpi_init(&E);
mbedtls_mpi_init(&D);
mbedtls_mpi_init(&P);
mbedtls_mpi_init(&Q);
mbedtls_mpi_init(&DP);
mbedtls_mpi_init(&DQ);
mbedtls_mpi_init(&QP);

mbedtls_mpi_read_string(&N, 16, S_N);  // 从十六进制字符串读取
mbedtls_mpi_read_string(&E, 16, S_E);
mbedtls_mpi_read_string(&D, 16, S_D);
mbedtls_mpi_read_string(&P, 16, S_P);
mbedtls_mpi_read_string(&Q, 16, S_Q);
// 注意：虽然代码读取了 DP, DQ, QP，但 mbedtls_rsa_import() 不使用它们
mbedtls_mpi_read_string(&DP, 16, S_DP);
mbedtls_mpi_read_string(&DQ, 16, S_DQ);
mbedtls_mpi_read_string(&QP, 16, S_QP);

// mbedtls_rsa_import() 只使用 N, P, Q, D, E
mbedtls_rsa_import(&ctx, &N, &P, &Q, &D, &E);
// mbedtls_rsa_complete() 会自动计算 DP, DQ, QP
mbedtls_rsa_complete(&ctx);

/* 加密 */
const char *msg = "Hello, World!";
uint8_t out[256];
mbedtls_rsa_pkcs1_encrypt(&ctx, mbedtls_ctr_drbg_random, &ctr_drbg, 
                         MBEDTLS_RSA_PUBLIC, strlen(msg), msg, out);

/* 解密 */
uint8_t out1[256];
size_t olen;
mbedtls_rsa_pkcs1_decrypt(&ctx, mbedtls_ctr_drbg_random, &ctr_drbg, 
                         MBEDTLS_RSA_PRIVATE, &olen, out, out1, sizeof(out1));

/* 签名 */
unsigned char sha1sum[20];
mbedtls_sha1_ret(msg, strlen(msg), sha1sum);
uint8_t sign[256];
mbedtls_rsa_pkcs1_sign(&ctx, mbedtls_ctr_drbg_random, &ctr_drbg,
                      MBEDTLS_RSA_PRIVATE, MBEDTLS_MD_SHA1, 0,
                      sha1sum, sign);

/* 验证 */
mbedtls_rsa_pkcs1_verify(&ctx, mbedtls_ctr_drbg_random, &ctr_drbg, 
                        MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA1, 20, 
                        sha1sum, sign);
```

## RSA 密钥格式说明

### 密钥组件

- **N**: RSA 模数（公钥和私钥共有）
- **E**: 公钥指数（通常为 65537 或 0x010001）
- **D**: 私钥指数
- **P**: 质数 P（N = P × Q）
- **Q**: 质数 Q（N = P × Q）
- **DP**: D mod (P-1)
- **DQ**: D mod (Q-1)
- **QP**: Q^(-1) mod P

### 支持的密钥长度

代码中预定义了以下密钥长度（通过 `RSA_KEY_SIZE` 宏选择）：
- **1024 位**：`RSA_KEY_SIZE == 1024`
- **2048 位**：`RSA_KEY_SIZE == 2048`（默认）
- **4096 位**：`RSA_KEY_SIZE == 4096`
- **8192 位**：`RSA_KEY_SIZE == 8192`

**注意**：代码虽然读取了 DP, DQ, QP 值，但 `mbedtls_rsa_import()` 只使用 N, P, Q, D, E，然后通过 `mbedtls_rsa_complete()` 自动计算 DP, DQ, QP，因此预定义的 DP, DQ, QP 值实际上不会被使用

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_MBEDTLS=y`**: 启用 mbedTLS 模块
- **`CONFIG_MBEDTLS_KEY_EXCHANGE_RSA_ENABLED=y`**: 启用 RSA 密钥交换
- **`CONFIG_MBEDTLS_GENPRIME_ENABLED=y`**: 启用质数生成（如果使用 `IS_GENERATE_KEY == 1`）
- **`CONFIG_MBEDTLS_HASH_ALL_ENABLED=y`**: 启用所有哈希算法（用于签名）

### 硬件加速配置（可选）

- **`CONFIG_MBEDTLS_HARDWARE_ENTROPY=y`**: 启用硬件熵源
- **`CONFIG_MBEDTLS_HARDWARE_MPI_MODULAR_EXPONENTIATION=y`**: 启用硬件模幂运算加速

## 注意事项

1. **密钥生成**: 如果 `IS_GENERATE_KEY == 1`，生成大质数可能需要较长时间，特别是对于 4096 位或更大的密钥
2. **填充方式**: 代码使用 PKCS#1 v2.1（OAEP）填充进行加密，使用 PKCS#1 v1.5 填充进行签名。代码中初始化时使用 `MBEDTLS_RSA_PKCS_V21`，虽然注释中有 `MBEDTLS_RSA_PKCS_V15` 的示例，但实际运行使用的是 v2.1
3. **哈希算法**: 签名使用 SHA1 哈希算法，实际应用中建议使用更安全的算法（如 SHA256）
4. **密钥导入**: 从十六进制字符串导入密钥时，确保所有密钥组件格式正确
5. **资源清理**: 使用完 RSA 上下文和 MPI 后，必须调用相应的 `free()` 函数释放资源
6. **加密数据大小**: RSA 加密的数据大小受密钥长度限制，2048 位密钥最多可加密 245 字节（使用 OAEP 填充）
7. **随机数生成器**: 加密和签名操作需要随机数，必须正确初始化 CTR_DRBG
8. **密钥安全**: 预定义的密钥仅用于测试，实际应用中应使用安全生成的密钥

