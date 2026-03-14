/**
 * @file xz_tls.c
 * @brief 小智云端 mbedTLS 封装层实现
 */

#define TAG "xz_tls"

#include "xz_tls.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include <string.h>

/* lwIP includes */
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

/* mbedTLS includes */
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

/** TLS 连接结构 */
struct xz_tls_s {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    int socket_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    bool initialized;
    bool connected;
    uint32_t timeout_ms;
};

/** 全局 RNG 初始化标志 */
static bool g_rng_initialized = false;
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

/**
 * @brief 初始化全局 RNG
 */
static void init_global_rng(void)
{
    if (!g_rng_initialized) {
        mbedtls_entropy_init(&g_entropy);
        mbedtls_ctr_drbg_init(&g_ctr_drbg);
        const char *pers = "xz_tls_rng";
        mbedtls_ctr_drbg_seed(&g_ctr_drbg, mbedtls_entropy_func, &g_entropy,
                             (const unsigned char *)pers, strlen(pers));
        g_rng_initialized = true;
    }
}

/**
 * @brief 自定义发送函数
 */
static int tls_send(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int ret = send(fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    return ret;
}

/**
 * @brief 自定义接收函数
 */
static int tls_recv(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int ret = recv(fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    return ret;
}

xz_tls_t xz_tls_create(const xz_tls_config_t *config)
{
    if (!config) {
        LISA_LOGE(TAG, "Invalid config");
        return NULL;
    }

    struct xz_tls_s *tls = (struct xz_tls_s *)lisa_mem_calloc(1, sizeof(struct xz_tls_s));
    if (!tls) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    /* 初始化全局 RNG */
    init_global_rng();

    /* 初始化 SSL */
    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->conf);
    tls->socket_fd = -1;

    tls->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 10000;

    /* 配置 SSL */
    int ret = mbedtls_ssl_config_defaults(&tls->conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        LISA_LOGE(TAG, "ssl_config_defaults failed: -0x%04X", -ret);
        goto cleanup;
    }

    mbedtls_ssl_conf_rng(&tls->conf, mbedtls_ctr_drbg_random, &g_ctr_drbg);

    /* 设置证书验证 */
    if (config->verify_cert) {
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        LISA_LOGW(TAG, "Certificate verification requested but not supported");
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
    } else {
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    /* 设置超时 */
    mbedtls_ssl_conf_read_timeout(&tls->conf, tls->timeout_ms / 1000);

    /* 设置 SSL 上下文 */
    ret = mbedtls_ssl_setup(&tls->ssl, &tls->conf);
    if (ret != 0) {
        LISA_LOGE(TAG, "ssl_setup failed: -0x%04X", -ret);
        goto cleanup;
    }

    tls->initialized = true;
    tls->connected = false;

    LISA_LOGI(TAG, "TLS context created");
    return tls;

cleanup:
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ssl_free(&tls->ssl);
    lisa_mem_free(tls);
    return NULL;
}

void xz_tls_destroy(xz_tls_t tls)
{
    if (!tls) {
        return;
    }

    if (tls->connected) {
        xz_tls_close(tls);
    }

    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ssl_free(&tls->ssl);

    lisa_mem_free(tls);
}

int xz_tls_connect(xz_tls_t tls, const char *host, uint16_t port)
{
    if (!tls || !host) {
        LISA_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    if (tls->connected) {
        LISA_LOGW(TAG, "Already connected, closing first");
        xz_tls_close(tls);
    }

    /* 重置 SSL 会话状态 (必须在重连前调用) */
    int ret = mbedtls_ssl_session_reset(&tls->ssl);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to reset SSL session: -0x%04X", -ret);
        return -1;
    }

    LISA_LOGI(TAG, "Connecting to %s:%u", host, port);

    /* 创建 TCP socket */
    tls->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tls->socket_fd < 0) {
        LISA_LOGE(TAG, "Failed to create socket");
        return -1;
    }

    /* 设置超时 - 仅用于初始连接，SSL层有自己的超时 */
    /* 使用较长的socket超时以避免与SSL超时冲突 */
    struct timeval tv;
    tv.tv_sec = 60;  /* 60秒socket超时 */
    tv.tv_usec = 0;
    setsockopt(tls->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(tls->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* 解析主机地址 */
    struct hostent *he = gethostbyname(host);
    if (!he) {
        LISA_LOGE(TAG, "Failed to resolve host: %s", host);
        close(tls->socket_fd);
        tls->socket_fd = -1;
        return -1;
    }

    /* 连接 TCP */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(tls->socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LISA_LOGE(TAG, "Failed to connect to %s:%u", host, port);
        close(tls->socket_fd);
        tls->socket_fd = -1;
        return -1;
    }

    /* 设置 SNI 主机名 */
    mbedtls_ssl_set_hostname(&tls->ssl, host);

    /* 设置 BIO */
    mbedtls_ssl_set_bio(&tls->ssl, &tls->socket_fd, tls_send, tls_recv, NULL);

    /* 执行 SSL 握手 */
    LISA_LOGI(TAG, "Starting TLS handshake...");
    int ret = mbedtls_ssl_handshake(&tls->ssl);
    if (ret != 0) {
        LISA_LOGE(TAG, "ssl_handshake failed: -0x%04X", -ret);
        close(tls->socket_fd);
        tls->socket_fd = -1;
        return -1;
    }

    /* 验证证书 */
    if (mbedtls_ssl_get_verify_result(&tls->ssl) != 0) {
        LISA_LOGW(TAG, "Certificate verification failed");
    }

    tls->connected = true;
    LISA_LOGI(TAG, "TLS connected to %s:%u", host, port);

    return 0;
}

int xz_tls_send(xz_tls_t tls, const void *data, size_t len)
{
    if (!tls || !tls->connected || !data) {
        return -1;
    }

    size_t total_sent = 0;
    const unsigned char *ptr = (const unsigned char *)data;

    while (total_sent < len) {
        int ret = mbedtls_ssl_write(&tls->ssl, ptr + total_sent, len - total_sent);
        if (ret < 0) {
            if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ) {
                /* 需要重试，继续循环 */
                continue;
            }
            LISA_LOGE(TAG, "ssl_write failed: -0x%04X at offset %d", -ret, total_sent);
            tls->connected = false;
            return -1;
        }
        if (ret == 0) {
            LISA_LOGW(TAG, "ssl_write returned 0 at offset %d", total_sent);
            break;
        }
        total_sent += ret;
    }

    return total_sent;
}

int xz_tls_recv(xz_tls_t tls, void *buf, size_t len)
{
    if (!tls || !tls->connected || !buf) {
        return -1;
    }

    int ret = mbedtls_ssl_read(&tls->ssl, (unsigned char *)buf, len);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ) {
            /* 需要重试 - 返回0让调用者知道需要重试 */
            return 0;
        }
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            LISA_LOGW(TAG, "Peer closed connection");
        } else if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
            LISA_LOGW(TAG, "SSL read timeout");
        } else {
            LISA_LOGE(TAG, "ssl_read failed: -0x%04X", -ret);
        }
        tls->connected = false;
        return -1;
    }

    /* ret == 0 表示对等方关闭连接 */
    if (ret == 0) {
        LISA_LOGW(TAG, "Connection closed by peer (EOF)");
        tls->connected = false;
        return -1;
    }

    return ret;
}

void xz_tls_close(xz_tls_t tls)
{
    if (!tls || !tls->connected) {
        return;
    }

    /* 关闭 SSL 连接 */
    mbedtls_ssl_close_notify(&tls->ssl);

    /* 关闭 socket */
    if (tls->socket_fd >= 0) {
        close(tls->socket_fd);
        tls->socket_fd = -1;
    }

    tls->connected = false;
    LISA_LOGI(TAG, "TLS connection closed");
}
