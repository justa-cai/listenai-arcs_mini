/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * iperf3 协议客户端实现（支持上行 / 下行 / 双向）
 *
 * 参考: https://github.com/esnet/iperf/wiki/IperfProtocolStates
 */

#include "coex_iperf.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "cJSON.h"

#include "coex_app.h"
#include "coex_wifi.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

#define LOG_TAG "coex.iperf3"
#include "lisa_log.h"

/* iperf3 协议状态码（与 iperf3 源码 iperf.h 一致） */
#define IPERF3_TEST_START       1
#define IPERF3_TEST_RUNNING     2
#define IPERF3_TEST_END         4
#define IPERF3_SERVER_TERMINATE 11
#define IPERF3_CLIENT_TERMINATE 12
#define IPERF3_PARAM_EXCHANGE   9
#define IPERF3_CREATE_STREAMS   10
#define IPERF3_EXCHANGE_RESULTS 13
#define IPERF3_DISPLAY_RESULTS  14
#define IPERF3_IPERF_DONE       16
#define IPERF3_ACCESS_DENIED    (-1)
#define IPERF3_SERVER_ERROR     (-2)

/* ACCESS_DENIED 重试配置 */
#define IPERF3_MAX_RETRIES      3
#define IPERF3_RETRY_DELAY_MS   2000

/* cookie 长度（含尾部 '\0' 占位，实际发 37 字节） */
#define IPERF3_COOKIE_SIZE      37
#define IPERF3_CTRL_POLL_DELAY_MS 50
#define IPERF3_CTRL_RECV_TIMEOUT_MS 35000
#define IPERF3_CLIENT_VERSION   "3.18"
#define COEX_IPERF_SERVER_IP_MAX_LEN 16

static volatile int g_ctrl_sock = -1;
static volatile int g_tx_sock = -1;
static volatile int g_rx_sock = -1;
static char g_coex_iperf_server_ip[COEX_IPERF_SERVER_IP_MAX_LEN] = COEX_SERVER_IP;
static int g_coex_iperf_server_port = COEX_SERVER_PORT;

/* ---------- 辅助函数 ---------- */

static void set_sock(volatile int *slot, int sock)
{
    taskENTER_CRITICAL();
    *slot = sock;
    taskEXIT_CRITICAL();
}

void coex_runner_iperf3_abort(void)
{
    int sock;

    taskENTER_CRITICAL();
    sock = g_tx_sock;
    taskEXIT_CRITICAL();
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        set_sock(&g_tx_sock, -1);
    }

    taskENTER_CRITICAL();
    sock = g_rx_sock;
    taskEXIT_CRITICAL();
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        set_sock(&g_rx_sock, -1);
    }

    taskENTER_CRITICAL();
    sock = g_ctrl_sock;
    taskEXIT_CRITICAL();
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        set_sock(&g_ctrl_sock, -1);
    }
}

static void close_test_socket(volatile int *slot, int *sock)
{
    if (sock == NULL || *sock < 0) {
        return;
    }

    shutdown(*sock, SHUT_RDWR);
    close(*sock);
    *sock = -1;
    if (slot != NULL) {
        set_sock(slot, -1);
    }
}

static void wait_retry_delay(volatile bool *enabled, uint32_t delay_ms)
{
    uint32_t waited_ms = 0;

    while (waited_ms < delay_ms) {
        if (!*enabled || !coex_wifi_is_ready()) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        waited_ms += 100;
    }
}

static void format_transfer(uint64_t bytes, char *buf, size_t len)
{
    static const char *units[] = { "Bytes", "KBytes", "MBytes", "GBytes" };
    double value = (double)bytes;
    size_t unit = 0;

    while (value >= 1024.0 && unit < (sizeof(units) / sizeof(units[0])) - 1U) {
        value /= 1024.0;
        unit++;
    }

    if (unit == 0) {
        (void)snprintf(buf, len, "%4llu %s", (unsigned long long)bytes, units[unit]);
    } else if (value >= 100.0) {
        (void)snprintf(buf, len, "%4.0f %s", value, units[unit]);
    } else if (value >= 10.0) {
        (void)snprintf(buf, len, "%4.1f %s", value, units[unit]);
    } else {
        (void)snprintf(buf, len, "%4.2f %s", value, units[unit]);
    }
}

static void format_bitrate(uint64_t bytes, uint32_t duration_ms, char *buf, size_t len)
{
    static const char *units[] = { "bits/sec", "Kbits/sec", "Mbits/sec", "Gbits/sec" };
    double value = 0.0;
    size_t unit = 0;

    if (duration_ms > 0) {
        value = ((double)bytes * 8.0 * 1000.0) / (double)duration_ms;
    }

    while (value >= 1000.0 && unit < (sizeof(units) / sizeof(units[0])) - 1U) {
        value /= 1000.0;
        unit++;
    }

    if (value >= 100.0) {
        (void)snprintf(buf, len, "%4.0f %s", value, units[unit]);
    } else if (value >= 10.0) {
        (void)snprintf(buf, len, "%4.1f %s", value, units[unit]);
    } else {
        (void)snprintf(buf, len, "%4.2f %s", value, units[unit]);
    }
}

static void log_iperf3_report_preamble(const char *server_ip, int server_port)
{
    LOGI("-----------------------------------------------------------");
    LOGI("Client connecting to %s, TCP port %d", server_ip, server_port);
    LOGI("[ ID] Interval           Transfer     Bitrate");
}

static bool coex_mode_has_tx(coex_mode_t mode)
{
    return mode == COEX_MODE_UPLINK || mode == COEX_MODE_BIDIRECTIONAL;
}

static bool coex_mode_has_rx(coex_mode_t mode)
{
    return mode == COEX_MODE_DOWNLINK || mode == COEX_MODE_BIDIRECTIONAL;
}

static void log_iperf3_report_line(int stream_id,
                                   uint32_t start_ms,
                                   uint32_t end_ms,
                                   uint64_t bytes,
                                   const char *role)
{
    char transfer[24];
    char bitrate[24];

    format_transfer(bytes, transfer, sizeof(transfer));
    format_bitrate(bytes, end_ms > start_ms ? end_ms - start_ms : 0U, bitrate, sizeof(bitrate));

    if (role != NULL) {
        LOGI("[%3d] %6.2f-%-6.2f sec  %10s  %12s  %s",
             stream_id,
             start_ms / 1000.0,
             end_ms / 1000.0,
             transfer,
             bitrate,
             role);
    } else {
        LOGI("[%3d] %6.2f-%-6.2f sec  %10s  %12s",
             stream_id,
             start_ms / 1000.0,
             end_ms / 1000.0,
             transfer,
             bitrate);
    }
}

/* 生成随机 cookie 字符串 */
static void generate_cookie(char *cookie, size_t len)
{
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz234567";
    uint32_t seed = (uint32_t)xTaskGetTickCount();

    if (cookie == NULL || len == 0) {
        return;
    }

    for (size_t i = 0; i < (len - 1); i++) {
        seed = seed * 1103515245 + 12345;
        cookie[i] = charset[(seed >> 16) % (sizeof(charset) - 1)];
    }
    cookie[len - 1] = '\0';
}

/* 阻塞式连接（带超时） */
static int connect_blocking(const char *ip, int port, uint32_t timeout_ms, volatile bool *enabled)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("socket create failed: %d", errno);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_aton(ip, &addr.sin_addr) == 0) {
        LOGE("invalid server ip: %s", ip);
        close(sock);
        return -1;
    }

    /* 非阻塞连接 */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS && errno != EWOULDBLOCK && errno != EINTR) {
            LOGE("connect to %s:%d failed: errno=%d", ip, port, errno);
            close(sock);
            return -1;
        }

        /* 等待连接完成 */
        uint32_t waited = 0;
        while (waited < timeout_ms) {
            if (!*enabled || !coex_wifi_is_ready()) {
                close(sock);
                return -1;
            }

            fd_set wfds;
            struct timeval tv = { .tv_sec = 0, .tv_usec = 200 * 1000 };
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);

            int ret = select(sock + 1, NULL, &wfds, NULL, &tv);
            if (ret > 0 && FD_ISSET(sock, &wfds)) {
                int so_err = 0;
                socklen_t elen = sizeof(so_err);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_err, &elen);
                if (so_err == 0) {
                    goto connected;
                }
                LOGE("connect so_error=%d", so_err);
                close(sock);
                return -1;
            }
            waited += 200;
        }
        LOGE("connect timeout after %u ms", timeout_ms);
        close(sock);
        return -1;
    }

connected:
    /* 切回阻塞模式 */
    flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }

    return sock;
}

/* 精确发送 n 字节 */
static int send_exact(int sock, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;

    while (sent < len) {
        int ret = send(sock, p + sent, len - sent, 0);
        if (ret <= 0) {
            LOGE("send_exact failed: sent=%u/%u errno=%d", (unsigned)sent, (unsigned)len, errno);
            return -1;
        }
        sent += (size_t)ret;
    }
    return 0;
}

/* 精确接收 n 字节 */
static int recv_exact(int sock, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    uint32_t waited_ms = 0;

    while (got < len) {
        int ret = recv(sock, p + got, len - got, 0);
        if (ret == 0) {
            LOGE("recv_exact failed: got=%u/%u errno=%d", (unsigned)got, (unsigned)len, errno);
            return -1;
        }
        if (ret < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
                if (waited_ms >= IPERF3_CTRL_RECV_TIMEOUT_MS) {
                    LOGE("recv_exact timeout: got=%u/%u errno=%d", (unsigned)got, (unsigned)len, errno);
                    return -1;
                }
                vTaskDelay(pdMS_TO_TICKS(IPERF3_CTRL_POLL_DELAY_MS));
                waited_ms += IPERF3_CTRL_POLL_DELAY_MS;
                continue;
            }

            LOGE("recv_exact failed: got=%u/%u errno=%d", (unsigned)got, (unsigned)len, errno);
            return -1;
        }
        got += (size_t)ret;
        waited_ms = 0;
    }
    return 0;
}

/* 读取 1 字节状态码 */
static int recv_state(int sock, int8_t *state)
{
    return recv_exact(sock, state, 1);
}

static int wait_for_test_start(int ctrl_sock, int8_t *state)
{
    uint32_t waited_ms = 0;

    while (waited_ms < IPERF3_CTRL_RECV_TIMEOUT_MS) {
        fd_set rfds;
        struct timeval tv = {
            .tv_sec = 0,
            .tv_usec = IPERF3_CTRL_POLL_DELAY_MS * 1000,
        };

        FD_ZERO(&rfds);
        FD_SET(ctrl_sock, &rfds);

        int ret = select(ctrl_sock + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOGE("wait TEST_START select failed: errno=%d", errno);
            return -1;
        }
        if (ret == 0) {
            waited_ms += IPERF3_CTRL_POLL_DELAY_MS;
            continue;
        }

        if (FD_ISSET(ctrl_sock, &rfds)) {
            return recv_state(ctrl_sock, state);
        }
    }

    LOGE("wait TEST_START timeout after %u ms", IPERF3_CTRL_RECV_TIMEOUT_MS);
    return -1;
}

/* 发送 1 字节状态码 */
static int send_state(int sock, int8_t state)
{
    return send_exact(sock, &state, 1);
}

/* 发送 JSON 字符串（前缀 4 字节网络序长度） */
static int send_json(int sock, const char *json_str)
{
    uint32_t len = (uint32_t)strlen(json_str);
    uint32_t net_len = htonl(len);

    if (send_exact(sock, &net_len, 4) != 0) {
        return -1;
    }
    return send_exact(sock, json_str, len);
}

/* 接收 JSON 字符串（前缀 4 字节网络序长度），调用方需 free 返回值 */
static char *recv_json(int sock)
{
    uint32_t net_len;
    if (recv_exact(sock, &net_len, 4) != 0) {
        return NULL;
    }

    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 65536) {
        LOGE("recv_json invalid length: %u", len);
        return NULL;
    }

    char *buf = (char *)pvPortMalloc(len + 1);
    if (buf == NULL) {
        LOGE("recv_json alloc %u failed", len + 1);
        return NULL;
    }

    if (recv_exact(sock, buf, len) != 0) {
        vPortFree(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static int create_data_stream(const char *server_ip,
                              int server_port,
                              const char *cookie,
                              volatile int *slot,
                              volatile bool *enabled)
{
    int data_sock;
    int flag = 1;

    data_sock = connect_blocking(server_ip, server_port,
                                 COEX_CONNECT_TIMEOUT_MS, enabled);
    if (data_sock < 0) {
        LOGE("data stream connect failed");
        return -1;
    }

    set_sock(slot, data_sock);
    setsockopt(data_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    if (send_exact(data_sock, cookie, IPERF3_COOKIE_SIZE) != 0) {
        LOGE("send cookie on data stream failed");
        close_test_socket(slot, &data_sock);
        return -1;
    }

    return data_sock;
}

/* ---------- iperf3 协议流程 ---------- */

static void log_interval_report(coex_mode_t mode,
                                int stream_id,
                                uint32_t start_ms,
                                uint32_t end_ms,
                                uint64_t interval_sent,
                                uint64_t interval_recv)
{
    if (mode == COEX_MODE_UPLINK) {
        if (interval_sent > 0) {
            log_iperf3_report_line(stream_id, start_ms, end_ms, interval_sent, NULL);
        }
        return;
    }

    if (mode == COEX_MODE_DOWNLINK) {
        if (interval_recv > 0) {
            log_iperf3_report_line(stream_id, start_ms, end_ms, interval_recv, NULL);
        }
        return;
    }

    if (interval_sent > 0) {
        log_iperf3_report_line(stream_id, start_ms, end_ms, interval_sent, "sender");
    }
    if (interval_recv > 0) {
        log_iperf3_report_line(stream_id, start_ms, end_ms, interval_recv, "receiver");
    }
}

static void log_final_report(coex_mode_t mode, int stream_id, const coex_round_result_t *result)
{
    LOGI("- - - - - - - - - - - - - - - - - - - - - - - - -");
    if (mode == COEX_MODE_UPLINK) {
        log_iperf3_report_line(stream_id, 0, result->duration_ms, result->bytes_sent, "sender");
    } else if (mode == COEX_MODE_DOWNLINK) {
        log_iperf3_report_line(stream_id, 0, result->duration_ms, result->bytes_received, "receiver");
    } else {
        log_iperf3_report_line(stream_id, 0, result->duration_ms, result->bytes_sent, "sender");
        log_iperf3_report_line(stream_id, 0, result->duration_ms, result->bytes_received, "receiver");
    }
    LOGI("-----------------------------------------------------------");
}

static int run_test_data_flow(coex_mode_t mode,
                              int tx_sock,
                              int rx_sock,
                              volatile bool *enabled,
                              coex_round_result_t *result)
{
    uint8_t send_buf[COEX_SEND_BLOCK_SIZE];
    uint8_t recv_buf[1460];
    TickType_t start_tick = xTaskGetTickCount();
    uint32_t last_report_ms = 0;
    uint64_t interval_sent = 0;
    uint64_t interval_recv = 0;
    uint32_t test_duration_ms = COEX_ROUND_SECONDS * 1000U;
    bool did_work;

    memset(send_buf, 0x5A, sizeof(send_buf));

    while (*enabled) {
        uint32_t elapsed_ms;

        if (!coex_wifi_is_ready()) {
            result->stop_reason = COEX_STOP_REASON_NETWORK_LOST;
            break;
        }

        elapsed_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - start_tick);
        if (elapsed_ms >= test_duration_ms) {
            result->stop_reason = COEX_STOP_REASON_COMPLETE;
            break;
        }

        did_work = false;

        if (coex_mode_has_tx(mode)) {
            int n = send(tx_sock, send_buf, sizeof(send_buf), 0);
            if (n > 0) {
                result->bytes_sent += (uint64_t)n;
                interval_sent += (uint64_t)n;
                did_work = true;
            } else if (n < 0 && errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                result->error_count++;
                result->stop_reason = coex_wifi_is_ready()
                    ? COEX_STOP_REASON_SEND_FAIL
                    : COEX_STOP_REASON_NETWORK_LOST;
                LOGE("send failed: errno=%d", errno);
                break;
            }
        }

        if (coex_mode_has_rx(mode)) {
            int n = recv(rx_sock, recv_buf, sizeof(recv_buf), 0);
            if (n > 0) {
                result->bytes_received += (uint64_t)n;
                interval_recv += (uint64_t)n;
                did_work = true;
            } else if (n == 0) {
                result->error_count++;
                result->stop_reason = COEX_STOP_REASON_RECV_FAIL;
                LOGE("recv failed: peer closed connection");
                break;
            } else if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                result->error_count++;
                result->stop_reason = coex_wifi_is_ready()
                    ? COEX_STOP_REASON_RECV_FAIL
                    : COEX_STOP_REASON_NETWORK_LOST;
                LOGE("recv failed: errno=%d", errno);
                break;
            }
        }

        elapsed_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - start_tick);
        if (elapsed_ms - last_report_ms >= 1000U) {
            log_interval_report(mode, tx_sock >= 0 ? tx_sock : rx_sock,
                                last_report_ms, elapsed_ms, interval_sent, interval_recv);
            last_report_ms = elapsed_ms;
            interval_sent = 0;
            interval_recv = 0;
        }

        if (!did_work) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (!*enabled && result->stop_reason == COEX_STOP_REASON_NONE) {
        result->stop_reason = COEX_STOP_REASON_USER_STOP;
    }

    result->duration_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - start_tick);

    if ((interval_sent > 0 || interval_recv > 0) && result->duration_ms > last_report_ms) {
        log_interval_report(mode, tx_sock >= 0 ? tx_sock : rx_sock,
                            last_report_ms, result->duration_ms, interval_sent, interval_recv);
    }

    log_final_report(mode, tx_sock >= 0 ? tx_sock : rx_sock, result);
    return (result->stop_reason == COEX_STOP_REASON_COMPLETE ||
            result->stop_reason == COEX_STOP_REASON_USER_STOP) ? 0 : -1;
}

int coex_runner_iperf3_run(coex_mode_t mode, volatile bool *enabled, coex_round_result_t *result)
{
    int ctrl_sock = -1;
    int tx_sock = -1;
    int rx_sock = -1;
    char server_ip[COEX_IPERF_SERVER_IP_MAX_LEN];
    int server_port;
    char cookie[IPERF3_COOKIE_SIZE];
    int8_t state;
    int ret = -1;
    bool server_busy = false;
    int attempt = 0;

    if (result == NULL || enabled == NULL) {
        return -1;
    }

    coex_iperf_get_server(server_ip, sizeof(server_ip), &server_port);

retry:
    attempt++;
    server_busy = false;
    memset(result, 0, sizeof(*result));
    result->mode = mode;
    result->stop_reason = COEX_STOP_REASON_NONE;

    /* 生成 cookie */
    generate_cookie(cookie, IPERF3_COOKIE_SIZE);

    /* === 1. 建立控制连接 === */
    LOGI("connect to iperf3 server %s:%d", server_ip, server_port);
    ctrl_sock = connect_blocking(server_ip, server_port,
                                 COEX_CONNECT_TIMEOUT_MS, enabled);
    if (ctrl_sock < 0) {
        result->stop_reason = coex_wifi_is_ready()
            ? COEX_STOP_REASON_CONNECT_FAIL
            : COEX_STOP_REASON_NETWORK_LOST;
        return -1;
    }
    set_sock(&g_ctrl_sock, ctrl_sock);

    /* 控制连接设置 TCP_NODELAY（iperf3 协议要求）和收发超时 */
    {
        int flag = 1;
        setsockopt(ctrl_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        struct timeval so_timeout = { .tv_sec = 30, .tv_usec = 0 };
        setsockopt(ctrl_sock, SOL_SOCKET, SO_RCVTIMEO, &so_timeout, sizeof(so_timeout));
        setsockopt(ctrl_sock, SOL_SOCKET, SO_SNDTIMEO, &so_timeout, sizeof(so_timeout));
    }
    LOGD("control connection established (TCP_NODELAY set)");

    /* 用 select 检查服务器是否立即发送了 ACCESS_DENIED（200ms 超时） */
    {
        fd_set rfds;
        struct timeval tv = { .tv_sec = 0, .tv_usec = 200 * 1000 };
        FD_ZERO(&rfds);
        FD_SET(ctrl_sock, &rfds);
        int sel = select(ctrl_sock + 1, &rfds, NULL, NULL, &tv);
        if (sel > 0) {
            uint8_t peek;
            int n = recv(ctrl_sock, &peek, 1, 0);
            if (n == 1 && (int8_t)peek == IPERF3_ACCESS_DENIED) {
                LOGW("server busy (ACCESS_DENIED)");
                server_busy = true;
                goto cleanup;
            } else if (n <= 0) {
                LOGE("server closed connection immediately (errno=%d)", errno);
                goto cleanup;
            }
            /* 服务器在 cookie 之前发了非 ACCESS_DENIED 的数据，异常 */
            LOGW("unexpected pre-cookie byte: 0x%02x (%d)", peek, (int)(int8_t)peek);
            goto cleanup;
        }
    }

    /* === 2. 发送 cookie === */
    LOGD("sending cookie (%d bytes)", IPERF3_COOKIE_SIZE);
    if (send_exact(ctrl_sock, cookie, IPERF3_COOKIE_SIZE) != 0) {
        LOGE("send cookie failed (errno=%d)", errno);
        goto cleanup;
    }

    /* === 3. PARAM_EXCHANGE: 等待服务器发状态码 === */
    {
        int recv_ret = recv_state(ctrl_sock, &state);
        if (recv_ret != 0) {
            LOGE("recv state failed (errno=%d), server may have closed connection", errno);
            goto cleanup;
        }
        LOGD("server state: %d", (int)state);
        if (state == IPERF3_ACCESS_DENIED) {
            LOGW("server busy (ACCESS_DENIED after cookie)");
            server_busy = true;
            goto cleanup;
        }
        if (state != IPERF3_PARAM_EXCHANGE) {
            LOGE("expected PARAM_EXCHANGE(9), got %d", (int)state);
            goto cleanup;
        }
    }

    {
        /* 构造测试参数 JSON */
        cJSON *params = cJSON_CreateObject();
        cJSON_AddBoolToObject(params, "tcp", 1);
        cJSON_AddNumberToObject(params, "omit", 0);
        cJSON_AddNumberToObject(params, "time", COEX_ROUND_SECONDS);
        cJSON_AddNumberToObject(params, "num", 0);
        cJSON_AddNumberToObject(params, "blockcount", 0);
        cJSON_AddNumberToObject(params, "parallel", 1);
        cJSON_AddNumberToObject(params, "len", COEX_SEND_BLOCK_SIZE);
        cJSON_AddStringToObject(params, "client_version", IPERF3_CLIENT_VERSION);
        if (mode == COEX_MODE_DOWNLINK) {
            cJSON_AddBoolToObject(params, "reverse", 1);
        } else if (mode == COEX_MODE_BIDIRECTIONAL) {
            cJSON_AddBoolToObject(params, "bidirectional", 1);
        }

        char *json_str = cJSON_PrintUnformatted(params);
        cJSON_Delete(params);

        if (json_str == NULL) {
            LOGE("JSON serialize failed");
            goto cleanup;
        }

        LOGD("send params: %s", json_str);
        int send_ret = send_json(ctrl_sock, json_str);
        cJSON_free(json_str);

        if (send_ret != 0) {
            LOGE("send params JSON failed");
            goto cleanup;
        }
    }

    /* === 4. CREATE_STREAMS: 建立数据流连接 === */
    if (recv_state(ctrl_sock, &state) != 0 || state != IPERF3_CREATE_STREAMS) {
        LOGE("expected CREATE_STREAMS(10), got %d", state);
        goto cleanup;
    }

    if (mode == COEX_MODE_BIDIRECTIONAL) {
        LOGD("creating bidirectional tx stream to %s:%d", server_ip, server_port);
        tx_sock = create_data_stream(server_ip, server_port, cookie, &g_tx_sock, enabled);
        if (tx_sock < 0) {
            goto cleanup;
        }

        LOGD("creating bidirectional rx stream to %s:%d", server_ip, server_port);
        rx_sock = create_data_stream(server_ip, server_port, cookie, &g_rx_sock, enabled);
        if (rx_sock < 0) {
            goto cleanup;
        }
    } else if (mode == COEX_MODE_UPLINK) {
        LOGD("creating uplink data stream to %s:%d", server_ip, server_port);
        tx_sock = create_data_stream(server_ip, server_port, cookie, &g_tx_sock, enabled);
        if (tx_sock < 0) {
            goto cleanup;
        }
    } else {
        LOGD("creating downlink data stream to %s:%d", server_ip, server_port);
        rx_sock = create_data_stream(server_ip, server_port, cookie, &g_rx_sock, enabled);
        if (rx_sock < 0) {
            goto cleanup;
        }
    }
    LOGD("data stream cookie sent, waiting for TEST_START on ctrl...");

    /* === 5. TEST_START === */
    state = 0;
    if (wait_for_test_start(ctrl_sock, &state) != 0 || state != IPERF3_TEST_START) {
        LOGE("expected TEST_START(1), got %d (errno=%d)", (int)state, errno);
        goto cleanup;
    }
    LOGD("test start");

    /* 数据流切换为非阻塞（发送/接收循环需要） */
    if (tx_sock >= 0) {
        int flags = fcntl(tx_sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(tx_sock, F_SETFL, flags | O_NONBLOCK);
        }
    }
    if (rx_sock >= 0 && rx_sock != tx_sock) {
        int flags = fcntl(rx_sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(rx_sock, F_SETFL, flags | O_NONBLOCK);
        }
    }

    /* === 6. TEST_RUNNING: 持续发送数据 === */
    state = 0;
    if (recv_state(ctrl_sock, &state) != 0 || state != IPERF3_TEST_RUNNING) {
        LOGE("expected TEST_RUNNING(2), got %d", state);
        goto cleanup;
    }
    LOGI("test running, mode=%s for %d seconds", coex_mode_str(mode), COEX_ROUND_SECONDS);
    log_iperf3_report_preamble(server_ip, server_port);
    ret = run_test_data_flow(mode, tx_sock, rx_sock, enabled, result);

    /* === 7. TEST_END: 通知服务器测试结束 === */
    LOGD("sending TEST_END");
    send_state(ctrl_sock, IPERF3_TEST_END);

    /* === 8. EXCHANGE_RESULTS: 等待服务器请求结果交换 === */
    state = 0;
    if (recv_state(ctrl_sock, &state) != 0 || state != IPERF3_EXCHANGE_RESULTS) {
        LOGW("expected EXCHANGE_RESULTS(13), got %d (non-fatal)", state);
        /* 非致命，继续清理 */
    } else {
        /* 发送客户端结果 JSON */
        cJSON *client_result = cJSON_CreateObject();

        cJSON_AddNumberToObject(client_result, "cpu_util_total", 0);
        cJSON_AddNumberToObject(client_result, "cpu_util_user", 0);
        cJSON_AddNumberToObject(client_result, "cpu_util_system", 0);
        cJSON_AddNumberToObject(client_result, "sender_has_retransmits", -1);

        cJSON *streams = cJSON_CreateArray();
        cJSON *stream = cJSON_CreateObject();
        cJSON_AddNumberToObject(stream, "id", 1);
        cJSON_AddNumberToObject(stream, "bytes", (double)result->bytes_sent);
        cJSON_AddNumberToObject(stream, "retransmits", -1);
        cJSON_AddNumberToObject(stream, "jitter", 0);
        cJSON_AddNumberToObject(stream, "errors", 0);
        cJSON_AddNumberToObject(stream, "omitted_errors", 0);
        cJSON_AddNumberToObject(stream, "packets", 0);
        cJSON_AddNumberToObject(stream, "omitted_packets", 0);

        double seconds = result->duration_ms / 1000.0;
        cJSON_AddNumberToObject(stream, "start_time", 0);
        cJSON_AddNumberToObject(stream, "end_time", seconds);

        cJSON_AddItemToArray(streams, stream);
        cJSON_AddItemToObject(client_result, "streams", streams);

        char *json_str = cJSON_PrintUnformatted(client_result);
        cJSON_Delete(client_result);

        if (json_str != NULL) {
            send_json(ctrl_sock, json_str);
            cJSON_free(json_str);
        }

        /* 接收服务端结果（读取但不解析） */
        char *server_json = recv_json(ctrl_sock);
        if (server_json != NULL) {
            LOGD("server result: %.128s%s",
                 server_json, strlen(server_json) > 128 ? "..." : "");
            vPortFree(server_json);
        }
    }

    /* === 9. DISPLAY_RESULTS === */
    if (recv_state(ctrl_sock, &state) == 0) {
        if (state == IPERF3_DISPLAY_RESULTS) {
            LOGD("display results received");
        }
    }

    /* === 10. IPERF_DONE === */
    send_state(ctrl_sock, IPERF3_IPERF_DONE);
    LOGD("iperf3 protocol done");
    if (ret == 0 || result->stop_reason == COEX_STOP_REASON_COMPLETE) {
        ret = 0;
    }

cleanup:
    if (ret != 0 && ctrl_sock >= 0 && !server_busy) {
        LOGD("sending CLIENT_TERMINATE");
        (void)send_state(ctrl_sock, IPERF3_CLIENT_TERMINATE);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (result->stop_reason == COEX_STOP_REASON_NONE && ret != 0) {
        result->stop_reason = COEX_STOP_REASON_CONNECT_FAIL;
    }

    close_test_socket(&g_tx_sock, &tx_sock);
    close_test_socket(&g_rx_sock, &rx_sock);
    close_test_socket(&g_ctrl_sock, &ctrl_sock);

    if (server_busy && attempt < IPERF3_MAX_RETRIES) {
        LOGW("iperf3 server busy, retry %d/%d after %u ms",
             attempt, IPERF3_MAX_RETRIES, IPERF3_RETRY_DELAY_MS);
        wait_retry_delay(enabled, IPERF3_RETRY_DELAY_MS);
        if (*enabled && coex_wifi_is_ready()) {
            goto retry;
        }
    }

    return ret;
}

#undef LOG_TAG
#define LOG_TAG "coex.iperf"
#include <lisa_log.h>

#include <string.h>

static TaskHandle_t g_coex_iperf_task;
static volatile bool g_coex_iperf_initialized;
static volatile bool g_coex_iperf_enabled;
static volatile coex_iperf_state_t g_coex_iperf_state = COEX_IPERF_STATE_INIT;
static volatile coex_mode_t g_coex_iperf_mode = COEX_DEFAULT_MODE;
static volatile coex_mode_t g_coex_iperf_pending_mode = COEX_DEFAULT_MODE;
static volatile uint32_t g_coex_iperf_round_id;
static coex_round_result_t g_coex_iperf_last_result;
static bool g_coex_iperf_stop_hint_logged;

static void coex_iperf_set_state_locked(coex_iperf_state_t state)
{
    taskENTER_CRITICAL();
    g_coex_iperf_state = state;
    taskEXIT_CRITICAL();
}

static void coex_iperf_store_last_result(const coex_round_result_t *result)
{
    taskENTER_CRITICAL();
    memcpy(&g_coex_iperf_last_result, result, sizeof(g_coex_iperf_last_result));
    taskEXIT_CRITICAL();
}

static uint32_t coex_iperf_next_round_id(void)
{
    uint32_t round_id;

    taskENTER_CRITICAL();
    g_coex_iperf_round_id++;
    round_id = g_coex_iperf_round_id;
    taskEXIT_CRITICAL();

    return round_id;
}

static void coex_iperf_log_placeholder_warning(void)
{
    char server_ip[COEX_IPERF_SERVER_IP_MAX_LEN];
    int server_port;

    coex_iperf_get_server(server_ip, sizeof(server_ip), &server_port);

    if (strcmp(COEX_TARGET_WIFI_SSID, COEX_PLACEHOLDER_WIFI_SSID) == 0 ||
        strcmp(COEX_TARGET_WIFI_PWD, COEX_PLACEHOLDER_WIFI_PWD) == 0) {
        LOGW("please update CONFIG_IPERF_WIFI_SSID/CONFIG_IPERF_WIFI_PWD before running");
    }

    if (strcmp(server_ip, COEX_PLACEHOLDER_SERVER_IP) == 0) {
        LOGW("CONFIG_IPERF_SERVER_IP is still the example value: %s", server_ip);
    }
}

static void coex_iperf_log_round_summary(uint32_t round_id, const coex_round_result_t *result)
{
    uint64_t tx_bps = coex_result_tx_throughput_bps(result);
    uint64_t rx_bps = coex_result_rx_throughput_bps(result);

    LOGI("round %u summary: mode=%s duration=%u ms tx_bytes=%llu tx_throughput=%llu bps "
         "rx_bytes=%llu rx_throughput=%llu bps errors=%u reason=%s",
         round_id,
         coex_mode_str(result->mode),
         result->duration_ms,
         result->bytes_sent,
         tx_bps,
         result->bytes_received,
         rx_bps,
         result->error_count,
         coex_stop_reason_str(result->stop_reason));
}

void coex_iperf_set_enabled(bool enabled)
{
    taskENTER_CRITICAL();
    g_coex_iperf_enabled = enabled;
    g_coex_iperf_state = enabled ? COEX_IPERF_STATE_WAIT_NETWORK : COEX_IPERF_STATE_STOPPED;
    taskEXIT_CRITICAL();

    if (!enabled) {
        coex_runner_iperf3_abort();
    }
}

bool coex_iperf_is_enabled(void)
{
    return g_coex_iperf_enabled;
}

void coex_iperf_set_mode(coex_mode_t mode)
{
    taskENTER_CRITICAL();
    g_coex_iperf_pending_mode = mode;
    taskEXIT_CRITICAL();
}

coex_mode_t coex_iperf_get_mode(void)
{
    return g_coex_iperf_pending_mode;
}

int coex_iperf_set_server(const char *ip, int port)
{
    struct in_addr addr;
    size_t ip_len;

    if (ip == NULL || ip[0] == '\0' || port <= 0 || port > 65535) {
        return -EINVAL;
    }

    if (g_coex_iperf_enabled) {
        return -EBUSY;
    }

    ip_len = strlen(ip);
    if (ip_len >= sizeof(g_coex_iperf_server_ip)) {
        return -ENAMETOOLONG;
    }

    if (inet_aton(ip, &addr) == 0) {
        return -EINVAL;
    }

    taskENTER_CRITICAL();
    strncpy(g_coex_iperf_server_ip, ip, sizeof(g_coex_iperf_server_ip) - 1);
    g_coex_iperf_server_ip[sizeof(g_coex_iperf_server_ip) - 1] = '\0';
    g_coex_iperf_server_port = port;
    taskEXIT_CRITICAL();

    return 0;
}

void coex_iperf_get_server(char *ip, size_t ip_len, int *port)
{
    if (ip == NULL || ip_len == 0) {
        return;
    }

    taskENTER_CRITICAL();
    strncpy(ip, g_coex_iperf_server_ip, ip_len - 1);
    ip[ip_len - 1] = '\0';
    if (port != NULL) {
        *port = g_coex_iperf_server_port;
    }
    taskEXIT_CRITICAL();
}

void coex_iperf_get_status(coex_iperf_status_t *status)
{
    if (status == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    status->initialized = g_coex_iperf_initialized;
    status->enabled = g_coex_iperf_enabled;
    status->state = g_coex_iperf_state;
    status->current_mode = g_coex_iperf_mode;
    status->pending_mode = g_coex_iperf_pending_mode;
    status->round_id = g_coex_iperf_round_id;
    memcpy(&status->last_result, &g_coex_iperf_last_result, sizeof(g_coex_iperf_last_result));
    taskEXIT_CRITICAL();
}

const char *coex_iperf_state_str(coex_iperf_state_t state)
{
    switch (state) {
    case COEX_IPERF_STATE_INIT:
        return "init";
    case COEX_IPERF_STATE_WAIT_NETWORK:
        return "wait_network";
    case COEX_IPERF_STATE_RUNNING:
        return "running";
    case COEX_IPERF_STATE_STOPPED:
        return "stopped";
    default:
        return "unknown";
    }
}

static void coex_iperf_task(void *arg)
{
    int ret;

    (void)arg;

    coex_iperf_log_placeholder_warning();
    coex_iperf_set_state_locked(COEX_IPERF_STATE_STOPPED);

    while (1) {
        if (!g_coex_iperf_enabled) {
            if (!g_coex_iperf_stop_hint_logged) {
                LOGI("iperf loop paused, waiting for shell command 'iperf start'");
                g_coex_iperf_stop_hint_logged = true;
            }
            coex_iperf_set_state_locked(COEX_IPERF_STATE_STOPPED);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (!coex_wifi_is_ready()) {
            if (g_coex_iperf_state != COEX_IPERF_STATE_WAIT_NETWORK) {
                LOGI("waiting for wifi connect and DHCP ready");
            }
            coex_iperf_set_state_locked(COEX_IPERF_STATE_WAIT_NETWORK);
            g_coex_iperf_stop_hint_logged = false;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        g_coex_iperf_stop_hint_logged = false;
        coex_iperf_set_state_locked(COEX_IPERF_STATE_RUNNING);
        taskENTER_CRITICAL();
        g_coex_iperf_mode = g_coex_iperf_pending_mode;
        taskEXIT_CRITICAL();

        uint32_t round_id = coex_iperf_next_round_id();
        LOGI("round %u start, mode=%s", round_id, coex_mode_str(g_coex_iperf_mode));

        coex_round_result_t round_result;
        memset(&round_result, 0, sizeof(round_result));
        round_result.mode = g_coex_iperf_mode;
        ret = coex_runner_iperf3_run(g_coex_iperf_mode, &g_coex_iperf_enabled, &round_result);
        coex_iperf_store_last_result(&round_result);

        if (ret != 0) {
            LOGW("round %u ended with runner return=%d", round_id, ret);
        }
        coex_iperf_log_round_summary(round_id, &round_result);

        if (!g_coex_iperf_enabled) {
            coex_iperf_set_state_locked(COEX_IPERF_STATE_STOPPED);
            continue;
        }

        if (!coex_wifi_is_ready()) {
            coex_iperf_set_state_locked(COEX_IPERF_STATE_WAIT_NETWORK);
            LOGI("wifi lost, iperf loop paused until reconnect");
            continue;
        }

        if (ret != 0) {
            LOGI("iperf3 failed, waiting 5s before retry");
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(COEX_ROUND_INTERVAL_MS));
        }
    }
}

int coex_iperf_init(void)
{
    char server_ip[COEX_IPERF_SERVER_IP_MAX_LEN];
    int server_port;

    if (g_coex_iperf_initialized) {
        return 0;
    }

    coex_iperf_get_server(server_ip, sizeof(server_ip), &server_port);
    LOGI("iperf init: default_mode=%s server=%s:%d round_seconds=%d block_size=%d interval_ms=%d",
         coex_mode_str(COEX_DEFAULT_MODE),
         server_ip,
         server_port,
         COEX_ROUND_SECONDS,
         COEX_SEND_BLOCK_SIZE,
         COEX_ROUND_INTERVAL_MS);

    if (xTaskCreate(coex_iperf_task, "coex_iperf", 8192, NULL, 8, &g_coex_iperf_task) != pdPASS) {
        LOGE("failed to create iperf task");
        return -1;
    }

    g_coex_iperf_initialized = true;
    g_coex_iperf_enabled = false;
    return 0;
}
