/**
 * @file xz_tls.h
 * @brief 小智云端 mbedTLS 封装层
 * @note 封装 mbedTLS 提供类似 OpenSSL 的接口
 */

#ifndef __XZ_TLS_H__
#define __XZ_TLS_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** TLS 连接句柄 */
typedef struct xz_tls_s *xz_tls_t;

/** TLS 配置 */
typedef struct {
    const char *ca_file;       /**< CA 证书文件路径 (可选) */
    const char *cert_file;     /**< 客户端证书文件 (可选) */
    const char *key_file;      /**< 客户端私钥文件 (可选) */
    bool verify_cert;          /**< 是否验证服务器证书 */
    uint32_t timeout_ms;       /**< 连接超时时间 (毫秒) */
} xz_tls_config_t;

/**
 * @brief 创建并初始化 TLS 连接
 * @param config TLS 配置
 * @return TLS 句柄，失败返回 NULL
 */
xz_tls_t xz_tls_create(const xz_tls_config_t *config);

/**
 * @brief 销毁 TLS 连接
 * @param tls TLS 句柄
 */
void xz_tls_destroy(xz_tls_t tls);

/**
 * @brief 连接到 TLS 服务器
 * @param tls TLS 句柄
 * @param host 服务器主机名
 * @param port 服务器端口
 * @return 0 成功, -1 失败
 */
int xz_tls_connect(xz_tls_t tls, const char *host, uint16_t port);

/**
 * @brief 发送数据
 * @param tls TLS 句柄
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 实际发送的字节数，-1 表示失败
 */
int xz_tls_send(xz_tls_t tls, const void *data, size_t len);

/**
 * @brief 接收数据
 * @param tls TLS 句柄
 * @param buf 接收缓冲区
 * @param len 缓冲区大小
 * @return 实际接收的字节数，-1 表示失败
 */
int xz_tls_recv(xz_tls_t tls, void *buf, size_t len);

/**
 * @brief 关闭 TLS 连接
 * @param tls TLS 句柄
 */
void xz_tls_close(xz_tls_t tls);

/**
 * @brief 检查连接状态
 * @param tls TLS 句柄
 * @return true 已连接, false 未连接
 */
bool xz_tls_is_connected(xz_tls_t tls);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_TLS_H__ */
