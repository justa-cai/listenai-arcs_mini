## 示例说明
- 演示了ECDH的整个过程，包括密钥生成，共享密钥计算
- 测试了mbedtls的所有椭圆曲线的ECDH实现
- `prj.conf`文件`CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY=y`选项可以打开ECC硬件加速