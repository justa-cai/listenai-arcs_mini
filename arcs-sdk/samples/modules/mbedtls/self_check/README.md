# mbedTLS 自检测试示例

## 功能说明

演示如何运行 mbedTLS 库的完整自检测试套件。本示例自动测试所有已启用的 mbedTLS 功能模块，包括加密算法、哈希算法、消息认证码、密钥交换算法等，确保 mbedTLS 库在目标平台上正常工作。

自检测试是验证 mbedTLS 库是否正确编译和配置的重要工具，特别适用于验证硬件加速功能是否正常工作。

## 硬件连接

无需外部连接，mbedTLS 为纯软件加密库。如果启用了硬件加速功能，测试会验证硬件加速是否正常工作。

## 示例内容

本示例运行以下自检测试（根据配置启用，按代码中的顺序）：

1. **内存分配测试** (`calloc`): 测试 `mbedtls_calloc` 和 `mbedtls_free` 的正确性
2. **哈希算法测试**: MD2, MD4, MD5, RIPEMD160, SHA1, SHA256, SHA512
3. **对称加密算法测试**: ARC4, DES, AES, Camellia, ARIA, XTEA
4. **认证加密测试**: GCM, CCM, NIST KW (NIST Key Wrapping)
5. **流密码测试**: ChaCha20
6. **消息认证码测试**: CMAC, Poly1305, ChaCha20-Poly1305
7. **大整数运算测试** (`mpi`): 测试大整数运算功能
8. **RSA 测试**: RSA 加密解密和签名验证
9. **X.509 测试**: X.509 证书解析
10. **椭圆曲线测试** (`ecp`): 椭圆曲线点运算
11. **ECJPAKE 测试**: ECJPAKE 密钥交换
12. **DHM 测试**: Diffie-Hellman 密钥交换
13. **随机数生成器测试**: CTR_DRBG, HMAC_DRBG
14. **熵源测试** (`entropy`): 熵源功能测试
15. **Base64 测试**: Base64 编码解码
16. **PKCS#5 测试**: PBKDF2 密钥派生
17. **定时测试** (`timing`): 定时功能测试（较慢的测试）
18. **内存缓冲区分配测试** (`memory_buffer_alloc`): 内存管理测试（最后执行）

**注意**：代码中还会在运行测试前进行以下检查：
- NULL 指针表示检查（验证全零位模式是否为 NULL 指针）
- `snprintf` 实现检查（验证格式化字符串函数的正确性）

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
mbedtls self test
run all tests: v:1, e:0
  CALLOC(0,1): passed (NULL)
  CALLOC(1,0): passed (NULL)
  CALLOC(1): passed
  CALLOC(1 again): passed

  MD2 test #1: passed
  MD2 test #2: passed
  ...

  Executed XX test suites

  [ All tests PASS ]
```

**说明**：
- 输出开头包含系统启动信息和日志系统初始化信息
- 每个测试套件会输出测试结果（passed 或 failed）
- 最后汇总显示执行的测试套件数量和通过/失败情况
- 如果所有测试通过，显示 "All tests PASS"
- 如果有测试失败，显示失败数量

## 核心 API

本示例主要调用 mbedTLS 库提供的自检函数，包括：

| API | 说明 |
|-----|------|
| `mbedtls_*_self_test()` | 各种模块的自检函数（如 `mbedtls_aes_self_test`, `mbedtls_sha256_self_test` 等） |
| `mbedtls_snprintf()` | 格式化字符串函数（用于测试） |
| `mbedtls_calloc()` / `mbedtls_free()` | 内存分配函数（用于测试） |

## 关键代码

```c
/* NULL 指针表示检查 */
void *pointer;
memset(&pointer, 0, sizeof(void *));
if (pointer != NULL) {
    mbedtls_printf("all-bits-zero is not a NULL pointer\n");
    mbedtls_exit(MBEDTLS_EXIT_FAILURE);
}

/* 测试 snprintf 实现 */
if (run_test_snprintf() != 0) {
    mbedtls_printf("the snprintf implementation is broken\n");
    mbedtls_exit(MBEDTLS_EXIT_FAILURE);
}

/* 初始化内存缓冲区分配器（如果启用） */
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
mbedtls_memory_buffer_alloc_init(init_buf, sizeof(init_buf));
#endif

/* 运行所有自检测试 */
const selftest_t *test;
for (test = selftests; test->name != NULL; test++) {
    if (test->function(v) != 0) {
        suites_failed++;
    }
    suites_tested++;
}

/* 输出测试结果 */
if (v != 0) {
    mbedtls_printf("  Executed %d test suites\n\n", suites_tested);
    if (suites_failed > 0) {
        mbedtls_printf("  [ %d tests FAIL ]\n\n", suites_failed);
    } else {
        mbedtls_printf("  [ All tests PASS ]\n\n");
    }
}

/* 根据测试结果退出 */
if (suites_failed > 0) {
    mbedtls_exit(MBEDTLS_EXIT_FAILURE);
}
mbedtls_exit(MBEDTLS_EXIT_SUCCESS);
```

## 硬件加速测试

如果启用了硬件加速，自检测试会验证以下硬件加速功能：

- **AES**: AES 加密解密硬件加速
- **AES-GCM**: AES-GCM 认证加密硬件加速
- **AES-CCM**: AES-CCM 认证加密硬件加速
- **AES-CMAC**: AES-CMAC 消息认证码硬件加速
- **HMAC**: HMAC 消息认证码硬件加速
- **Entropy**: 硬件熵源
- **RSA**: RSA 模幂运算硬件加速
- **ECDH**: ECDH 密钥交换硬件加速（如果支持）
- **ECDSA**: ECDSA 签名验证硬件加速

## 配置说明

### 必需配置

- **`CONFIG_SDK_MODULE_MBEDTLS=y`**: 启用 mbedTLS 模块
- **`CONFIG_MBEDTLS_TEST=y`**: 启用 mbedTLS 自检测试功能

### 可选配置（启用更多测试）

- **`CONFIG_MBEDTLS_CIPHER_ALL_ENABLED=y`**: 启用所有对称加密算法
- **`CONFIG_MBEDTLS_HASH_ALL_ENABLED=y`**: 启用所有哈希算法
- **`CONFIG_MBEDTLS_MAC_ALL_ENABLED=y`**: 启用所有消息认证码
- **`CONFIG_MBEDTLS_ECP_ALL_ENABLED=y`**: 启用所有椭圆曲线

### 硬件加速配置

- **`CONFIG_MBEDTLS_HARDWARE_ENTROPY=y`**: 启用硬件熵源
- **`CONFIG_MBEDTLS_HARDWARE_AES=y`**: 启用 AES 硬件加速
- **`CONFIG_MBEDTLS_HARDWARE_AES_GCM=y`**: 启用 AES-GCM 硬件加速
- **`CONFIG_MBEDTLS_HARDWARE_AES_CCM=y`**: 启用 AES-CCM 硬件加速
- **`CONFIG_MBEDTLS_HARDWARE_AES_CMAC=y`**: 启用 AES-CMAC 硬件加速
- **`CONFIG_MBEDTLS_HARDWARE_SHA_HMAC=y`**: 启用 SHA-HMAC 硬件加速
- **`CONFIG_MBEDTLS_HARDWARE_MPI_MODULAR_EXPONENTIATION=y`**: 启用 RSA 硬件加速
- **`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`**: 启用 ECDSA 硬件加速

## 注意事项

1. **测试时间**: 完整运行所有测试可能需要较长时间，特别是在资源受限的嵌入式系统上
2. **内存使用**: 某些测试（如大整数运算、RSA）可能需要较多内存
3. **硬件加速验证**: 如果启用了硬件加速，测试失败可能表示硬件加速配置不正确或硬件有问题
4. **测试覆盖**: 测试覆盖范围取决于编译时启用的功能模块，未启用的模块不会运行测试
5. **平台兼容性**: 某些测试可能在某些平台上失败，需要根据实际情况调整配置
6. **调试信息**: 使用详细模式（`v=1`）可以查看每个测试的详细输出，有助于定位问题
7. **内存分配器**: 如果使用自定义内存分配器（`MBEDTLS_MEMORY_BUFFER_ALLOC_C`），测试会验证内存分配器的正确性

