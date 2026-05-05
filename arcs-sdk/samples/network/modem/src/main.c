/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 4G Modem 模块示例
 *
 * 本示例演示如何使用 LISA Modem 模块实现以下功能：
 * 1. 初始化 4G 模块（ML307）
 * 2. 通过 4G 模块建立 TCP 连接
 * 3. 发送和接收 TCP 数据
 * 4. UDP 数据收发
 * 5. DNS 域名解析
 * 6. 关闭连接和资源清理
 */

#define LOG_TAG "modem"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include <lwip/sockets.h>
#include <lwip/inet.h>

#include "lisa_modem_module.h"
#include "user_fs.h"
#include "lisa_kv.h"
#include "modem_backend.h"

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define MODEM_UART_DEVICE   "uart2"

#define TEST_SERVER_HOST    "httpbin.org"
#define TEST_SERVER_PORT    80

// UDP 测试服务器配置（可以使用公共 NTP 服务器或 echo 服务器测试）
#define UDP_TEST_HOST       "ntp.aliyun.com"
#define UDP_TEST_PORT       123  // NTP 端口

// HTTP GET 请求示例
#define HTTP_GET_REQUEST    "GET /get HTTP/1.1\r\n" \
                            "Host: httpbin.org\r\n" \
                            "Connection: close\r\n" \
                            "\r\n"

// 接收缓冲区大小
#define RECV_BUFFER_SIZE    1024

// 接收超时时间（毫秒）
#define RECV_TIMEOUT_MS     10000

/**
 * @brief 演示 DNS 解析功能
 */
static int demo_dns_resolve(lisa_modem_t *modem, const char *domain, char *ip_addr, size_t ip_len)
{
    LISA_LOGI(LOG_TAG, "=== DNS Resolve Test ===");
    LISA_LOGI(LOG_TAG, "Resolving domain: %s", domain);

    if (!lisa_modem_dns_resolve_on(modem, domain, ip_addr, ip_len)) {
        LISA_LOGE(LOG_TAG, "Error: DNS resolve failed for %s", domain);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Resolved IP: %s", ip_addr);
    return 0;
}

static bool resolve_ipv4_sockaddr(lisa_modem_t *modem, const char *host, int port, struct sockaddr_in *addr)
{
    char ip_addr[64] = {0};

    if (!host || !addr) {
        return false;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr->sin_addr) == 1) {
        return true;
    }

    if (!lisa_modem_dns_resolve_on(modem, host, ip_addr, sizeof(ip_addr))) {
        return false;
    }

    return inet_pton(AF_INET, ip_addr, &addr->sin_addr) == 1;
}

/**
 * @brief 演示 TCP 连接和数据传输
 */
static int demo_tcp_communication(lisa_modem_t *modem, const char *host, int port)
{
    int sockfd = -1;
    int ret = -1;
    char *recv_buf = NULL;
    int recv_len;
    struct sockaddr_in dest_addr;

    LISA_LOGI(LOG_TAG, "=== TCP Communication Test ===");

    /* 分配接收缓冲区 */
    recv_buf = (char *)pvPortMalloc(RECV_BUFFER_SIZE);
    if (!recv_buf) {
        LISA_LOGE(LOG_TAG, "Error: Failed to allocate receive buffer");
        return -1;
    }

    /* 1. 创建 TCP socket */
    LISA_LOGI(LOG_TAG, "Step 1: Creating TCP socket...");
    sockfd = lisa_modem_socket_open_on(modem, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to create TCP socket");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "TCP socket created: id=%d", sockfd);

    /* 2. 连接到服务器 */
    LISA_LOGI(LOG_TAG, "Step 2: Connecting to %s:%d...", host, port);
    if (!resolve_ipv4_sockaddr(modem, host, port, &dest_addr) ||
        !lisa_modem_socket_connect_on(modem, sockfd, (const struct sockaddr *)&dest_addr, (int)sizeof(dest_addr))) {
        LISA_LOGE(LOG_TAG, "Error: Failed to connect to server");
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "Connected to server successfully");

    /* 3. 发送 HTTP 请求 */
    LISA_LOGI(LOG_TAG, "Step 3: Sending HTTP request...");
    ret = lisa_modem_socket_send_on(modem, sockfd, HTTP_GET_REQUEST, strlen(HTTP_GET_REQUEST), 5000);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to send data (ret=%d)", ret);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "Sent %d bytes", ret);

    /* 4. 接收响应 */
    LISA_LOGI(LOG_TAG, "Step 4: Receiving response...");
    memset(recv_buf, 0, RECV_BUFFER_SIZE);
    recv_len = lisa_modem_socket_recv_on(modem, sockfd, recv_buf, RECV_BUFFER_SIZE - 1, RECV_TIMEOUT_MS);
    if (recv_len > 0) {
        LISA_LOGI(LOG_TAG, "Received %d bytes:", recv_len);
        /* 打印前 512 字节的响应内容 */
        if (recv_len > 512) {
            recv_buf[512] = '\0';
            LISA_LOGI(LOG_TAG, "%s...(truncated)", recv_buf);
        } else {
            LISA_LOGI(LOG_TAG, "%s", recv_buf);
        }
    } else if (recv_len == 0) {
        LISA_LOGI(LOG_TAG, "Connection closed by server");
    } else {
        LISA_LOGE(LOG_TAG, "Error: Receive failed (ret=%d)", recv_len);
    }

    ret = 0;

cleanup:
    /* 5. 关闭连接 */
    if (sockfd >= 0) {
        LISA_LOGI(LOG_TAG, "Step 5: Closing connection...");
        lisa_modem_socket_close_on(modem, sockfd);
        LISA_LOGI(LOG_TAG, "Connection closed");
    }

    if (recv_buf) {
        vPortFree(recv_buf);
    }

    return ret;
}

/**
 * @brief 构建 NTP 请求包
 * @param buffer 缓冲区，至少 48 字节
 */
static void build_ntp_request(uint8_t *buffer)
{
    memset(buffer, 0, 48);
    /* NTP 版本 3，客户端模式 */
    buffer[0] = 0x1B;  // LI=0, VN=3, Mode=3 (client)
}

/**
 * @brief 演示 UDP 通信（使用 NTP 协议测试）
 */
static int demo_udp_communication(lisa_modem_t *modem, const char *host, int port)
{
    int sockfd = -1;
    int ret = -1;
    uint8_t ntp_request[48];
    uint8_t ntp_response[48];
    struct sockaddr_in dest_addr;
    struct sockaddr_in src_addr;
    int addr_len = sizeof(src_addr);
    char dest_ip[INET_ADDRSTRLEN] = {0};

    LISA_LOGI(LOG_TAG, "=== UDP Communication Test (NTP) ===");

    /* 1. 创建 UDP socket */
    LISA_LOGI(LOG_TAG, "Step 1: Creating UDP socket...");
    sockfd = lisa_modem_socket_open_on(modem, AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to create UDP socket");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "UDP socket created: id=%d", sockfd);

    /* 2. 准备目标地址 */
    LISA_LOGI(LOG_TAG, "Step 2: Resolving %s...", host);
    if (!resolve_ipv4_sockaddr(modem, host, port, &dest_addr)) {
        LISA_LOGE(LOG_TAG, "Error: Failed to resolve %s", host);
        goto cleanup;
    }
    inet_ntop(AF_INET, &dest_addr.sin_addr, dest_ip, sizeof(dest_ip));

    /* 3. 构建并发送 NTP 请求 */
    LISA_LOGI(LOG_TAG, "Step 3: Sending NTP request to %s:%d...", dest_ip, port);
    build_ntp_request(ntp_request);
    ret = lisa_modem_socket_sendto_on(modem, sockfd, ntp_request, sizeof(ntp_request), 0,
                                      (const struct sockaddr *)&dest_addr, (int)sizeof(dest_addr), 5000);
    if (ret < 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to send UDP data (ret=%d)", ret);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "Sent %d bytes", ret);

    /* 4. 接收 NTP 响应 */
    LISA_LOGI(LOG_TAG, "Step 4: Receiving NTP response...");
    memset(ntp_response, 0, sizeof(ntp_response));
    memset(&src_addr, 0, sizeof(src_addr));
    ret = lisa_modem_socket_recvfrom_on(modem, sockfd, ntp_response, sizeof(ntp_response), 0,
                                        (struct sockaddr *)&src_addr, &addr_len, RECV_TIMEOUT_MS);
    if (ret > 0) {
        char src_ip[INET_ADDRSTRLEN] = {0};

        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, sizeof(src_ip));
        LISA_LOGI(LOG_TAG, "Received %d bytes from NTP server", ret);
        LISA_LOGI(LOG_TAG, "recvfrom src=%s:%u", src_ip, ntohs(src_addr.sin_port));

        /* 解析 NTP 响应中的时间戳（简化版） */
        if (ret >= 48) {
            /* NTP 时间戳在偏移量 40-43（秒）*/
            uint32_t ntp_time = (ntp_response[40] << 24) | (ntp_response[41] << 16) |
                                (ntp_response[42] << 8) | ntp_response[43];
            /* NTP 时间从 1900 年开始，Unix 时间从 1970 年开始 */
            uint32_t unix_time = ntp_time - 2208988800UL;
            LISA_LOGI(LOG_TAG, "NTP timestamp: %u (Unix: %u)", ntp_time, unix_time);
        }
    } else if (ret == 0) {
        LISA_LOGI(LOG_TAG, "No data received");
    } else {
        LISA_LOGE(LOG_TAG, "Error: Receive failed (ret=%d)", ret);
    }

    ret = 0;

cleanup:
    /* 5. 关闭 UDP socket */
    if (sockfd >= 0) {
        LISA_LOGI(LOG_TAG, "Step 5: Closing UDP socket...");
        lisa_modem_socket_close_on(modem, sockfd);
        LISA_LOGI(LOG_TAG, "UDP socket closed");
    }

    return ret;
}

/**
 * @brief 主函数
 */
int main(int argc, char **argv)
{
    char ip_addr[64] = {0};
    lisa_modem_t *modem = NULL;

    (void)argc;
    (void)argv;

    LISA_LOGI(LOG_TAG, "=== 4G Modem Example ===");

    /* 初始化文件系统和 KV 存储 */
    user_fs_init();
    lisa_kv_init();

    /* 初始化 4G 模块 */
    LISA_LOGI(LOG_TAG, "Initializing 4G modem module via backend=%s...", sample_modem_backend_name());
    modem = sample_modem_open(MODEM_UART_DEVICE);
    if (!modem) {
        LISA_LOGE(LOG_TAG, "Error: Failed to initialize 4G modem");
        goto exit;
    }
    LISA_LOGI(LOG_TAG, "4G modem initialized successfully");

    /* 等待网络注册完成 */
    LISA_LOGI(LOG_TAG, "Waiting for network registration...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 演示 DNS 解析 */
    if (demo_dns_resolve(modem, TEST_SERVER_HOST, ip_addr, sizeof(ip_addr)) != 0) {
        LISA_LOGW(LOG_TAG, "DNS resolve failed, using hostname directly");
    }

    /* 演示 TCP 通信 */
    demo_tcp_communication(modem, TEST_SERVER_HOST, TEST_SERVER_PORT);

    /* 演示 UDP 通信（NTP 时间同步） */
    demo_udp_communication(modem, UDP_TEST_HOST, UDP_TEST_PORT);

    LISA_LOGI(LOG_TAG, "=== Example completed ===");

exit:
    if (modem) {
        sample_modem_close(modem);
    }

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
