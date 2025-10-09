## 示例说明
- 演示了ECDSA的整个过程，包括密钥生成，签名和验证签名
- 测试了mbedtls的所有椭圆曲线（除了`curve25519`和`curve448`）的ECDSA实现
- 测试了所有哈希算法的ECDSA的签名和验证签名
- `prj.conf`文件`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`选项可以打开ECC硬件加速