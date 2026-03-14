/**
 * @file xz_websocket.c
 * @brief 小智云端 WebSocket 客户端实现
 */

#define TAG "xz_ws"

#include "xz_websocket.h"
#include "xz_tls.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_time.h"
#include "lisa_thread.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WS_BUFFER_SIZE      8192
#define WS_MAX_HEADER_SIZE  14

/** WebSocket 连接状态 */
typedef enum {
    WS_STATE_CLOSED = 0,
    WS_STATE_CONNECTING,
    WS_STATE_OPEN,
    WS_STATE_CLOSING,
} ws_state_e;

/** WebSocket 连接结构 */
struct xz_websocket_s {
    xz_tls_t tls;
    char host[128];
    uint16_t port;
    char path[256];
    char token[256];
    char device_id[64];
    char client_id[64];
    uint32_t timeout_ms;
    void *user;
    xz_ws_event_cb on_event;
    xz_ws_data_cb on_data;

    ws_state_e state;
    bool running;
    lisa_thread_t *thread;
    uint8_t buffer[WS_BUFFER_SIZE];
};

/** Base64 编码表 */
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Base64 编码
 */
static void base64_encode(const uint8_t *data, size_t len, char *out)
{
    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        uint32_t octet_a = i < len ? data[i] : 0;
        uint32_t octet_b = i + 1 < len ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < len ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = base64_table[(triple >> 18) & 0x3F];
        out[j++] = base64_table[(triple >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? base64_table[(triple >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? base64_table[triple & 0x3F] : '=';
    }
    out[j] = '\0';
}

/**
 * @brief 生成 WebSocket 握手密钥
 */
static void generate_ws_key(char *key)
{
    uint8_t random_bytes[16];
    uint32_t tick = lisa_os_get_tick_ms();
    for (int i = 0; i < 16; i++) {
        random_bytes[i] = (tick >> (i % 4) * 8) ^ (i * 0x5A);
    }
    base64_encode(random_bytes, 16, key);
}

/**
 * @brief 解析 WebSocket URL
 * @return 0 成功, -1 失败
 */
static int parse_ws_url(xz_websocket_t ws, const char *url)
{
    if (strncmp(url, "wss://", 6) != 0) {
        LISA_LOGE(TAG, "Only wss:// scheme is supported");
        return -1;
    }

    const char *p = url + 6;
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    /* 提取主机和端口 */
    if (colon && (!slash || colon < slash)) {
        size_t host_len = colon - p;
        if (host_len >= sizeof(ws->host)) host_len = sizeof(ws->host) - 1;
        strncpy(ws->host, p, host_len);
        ws->host[host_len] = '\0';
        ws->port = atoi(colon + 1);

        if (slash) {
            strncpy(ws->path, slash, sizeof(ws->path) - 1);
        } else {
            strcpy(ws->path, "/");
        }
    } else if (slash) {
        size_t host_len = slash - p;
        if (host_len >= sizeof(ws->host)) host_len = sizeof(ws->host) - 1;
        strncpy(ws->host, p, host_len);
        ws->host[host_len] = '\0';
        ws->port = 443;
        strncpy(ws->path, slash, sizeof(ws->path) - 1);
    } else {
        strncpy(ws->host, p, sizeof(ws->host) - 1);
        ws->port = 443;
        strcpy(ws->path, "/");
    }

    LISA_LOGI(TAG, "Parsed URL: host=%s, port=%u, path=%s", ws->host, ws->port, ws->path);
    return 0;
}

/**
 * @brief 简单的字符串查找（不区分大小写）
 */
static bool str_contains(const char *haystack, const char *needle)
{
    if (!haystack || !needle) {
        return false;
    }

    /* 将字符串转换为小写进行比较 */
    char *lower_haystack = lisa_mem_alloc(strlen(haystack) + 1);
    char *lower_needle = lisa_mem_alloc(strlen(needle) + 1);

    if (!lower_haystack || !lower_needle) {
        lisa_mem_free(lower_haystack);
        lisa_mem_free(lower_needle);
        return false;
    }

    for (int i = 0; haystack[i]; i++) {
        lower_haystack[i] = (haystack[i] >= 'A' && haystack[i] <= 'Z') ?
                           haystack[i] + 32 : haystack[i];
    }
    lower_haystack[strlen(haystack)] = '\0';

    for (int i = 0; needle[i]; i++) {
        lower_needle[i] = (needle[i] >= 'A' && needle[i] <= 'Z') ?
                          needle[i] + 32 : needle[i];
    }
    lower_needle[strlen(needle)] = '\0';

    bool found = strstr(lower_haystack, lower_needle) != NULL;

    lisa_mem_free(lower_haystack);
    lisa_mem_free(lower_needle);

    return found;
}

/**
 * @brief 解析握手响应
 * @return 0 成功, -1 失败
 */
static int parse_handshake_response(const char *response, size_t len)
{
    (void)len;

    /* 检查状态行 */
    if (strstr(response, "101") == NULL) {
        LISA_LOGE(TAG, "Server did not respond with 101");
        return -1;
    }

    /* 检查 Upgrade 头 - 不区分大小写 */
    if (!str_contains(response, "Upgrade:") ||
        !str_contains(response, "websocket")) {
        LISA_LOGE(TAG, "Missing Upgrade header");
        return -1;
    }

    /* 检查 Connection 头 - 不区分大小写 */
    if (!str_contains(response, "Connection:") ||
        !str_contains(response, "upgrade")) {
        LISA_LOGE(TAG, "Missing Connection header");
        return -1;
    }

    /* 检查 Sec-WebSocket-Accept 头 */
    if (!str_contains(response, "Sec-WebSocket-Accept:")) {
        LISA_LOGE(TAG, "Missing Sec-WebSocket-Accept header");
        return -1;
    }

    LISA_LOGI(TAG, "WebSocket handshake validation passed");
    return 0;
}

/**
 * @brief 执行 WebSocket 握手
 * @return 0 成功, -1 失败
 */
static int perform_handshake(xz_websocket_t ws)
{
    char request[512];
    char ws_key[64];
    char response[512];

    generate_ws_key(ws_key);

    LISA_LOGI(TAG, "WebSocket key: %s", ws_key);

    /* 构建握手请求 */
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Authorization: Bearer %s\r\n"
        "Protocol-Version: 1\r\n"
        "Device-Id: %s\r\n"
        "Client-Id: %s\r\n"
        "\r\n",
        ws->path, ws->host, ws_key, ws->token, ws->device_id, ws->client_id);

    LISA_LOGI(TAG, "Sending handshake request...");

    /* 发送握手请求 */
    int sent = xz_tls_send(ws->tls, request, len);
    if (sent != len) {
        LISA_LOGE(TAG, "Failed to send handshake request");
        return -1;
    }

    LISA_LOGI(TAG, "Waiting for handshake response...");

    /* 接收完整响应 */
    int recv_len = 0;
    int total_received = 0;
    size_t response_len = sizeof(response) - 1;
    int retry_count = 0;
    const int max_retries = 50;  /* 防止无限循环 */

    /* 分块接收直到遇到完整的头结束标记 (\r\n\r\n) */
    while (total_received < (int)response_len && retry_count < max_retries) {
        int n = xz_tls_recv(ws->tls, response + total_received,
                            response_len - total_received);
        if (n < 0) {
            /* 真正的错误 */
            LISA_LOGE(TAG, "Failed to receive handshake response: %d", n);
            return -1;
        }
        if (n == 0) {
            /* TLS WANT_READ/WANT_WRITE，需要重试 */
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(10));  /* 短暂延迟后重试 */
            continue;
        }

        /* 成功接收数据，重置重试计数 */
        retry_count = 0;
        total_received += n;
        response[total_received] = '\0';

        /* 检查是否收到完整的头 */
        if (strstr(response, "\r\n\r\n") != NULL) {
            break;
        }
    }

    if (retry_count >= max_retries) {
        LISA_LOGE(TAG, "Handshake response timeout after %d retries", max_retries);
        return -1;
    }

    LISA_LOGI(TAG, "Handshake response received (%d bytes)", total_received);
    LISA_LOGI(TAG, "Response:\n%s", response);

    /* 解析响应 */
    if (parse_handshake_response(response, total_received) != 0) {
        LISA_LOGE(TAG, "Invalid handshake response");
        return -1;
    }

    LISA_LOGI(TAG, "WebSocket handshake completed");
    return 0;
}

/**
 * @brief WebSocket 接收线程
 */
static void ws_thread(void *arg)
{
    xz_websocket_t ws = (struct xz_websocket_s *)arg;

    LISA_LOGI(TAG, "Thread started");

    while (ws->running) {
        if (ws->state != WS_STATE_OPEN) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* 接收帧头 */
        uint8_t header[2];
        int n = xz_tls_recv(ws->tls, header, 2);
        if (n < 0) {
            if (ws->running) {
                LISA_LOGW(TAG, "Connection error");
                ws->state = WS_STATE_CLOSED;
                if (ws->on_event) {
                    ws->on_event(ws, XZ_WS_EVENT_DISCONNECTED, ws->user);
                }
            }
            continue;
        }
        if (n == 0) {
            /* TLS WANT_READ/WANT_WRITE，短暂延迟后重试 */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payload_len = header[1] & 0x7F;
        bool fin = (header[0] & 0x80) != 0;

        LISA_LOGD(TAG, "WS frame: opcode=%u, masked=%d, len=%llu, fin=%d",
                  opcode, masked, (unsigned long long)payload_len, fin);

        /* 读取扩展长度 */
        if (payload_len == 126) {
            uint8_t ext[2];
            n = xz_tls_recv(ws->tls, ext, 2);
            if (n < 0) {
                LISA_LOGE(TAG, "Failed to read extended length");
                ws->state = WS_STATE_CLOSED;
                continue;
            }
            if (n == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            payload_len = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            n = xz_tls_recv(ws->tls, ext, 8);
            if (n < 0) {
                LISA_LOGE(TAG, "Failed to read extended length");
                ws->state = WS_STATE_CLOSED;
                continue;
            }
            if (n == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }

        /* 读取掩码 */
        uint8_t mask[4] = {0};
        if (masked) {
            n = xz_tls_recv(ws->tls, mask, 4);
            if (n < 0) {
                LISA_LOGE(TAG, "Failed to read mask");
                ws->state = WS_STATE_CLOSED;
                continue;
            }
            if (n == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }

        /* 处理关闭帧 */
        if (opcode == 0x08) {
            LISA_LOGW(TAG, "WebSocket CLOSE frame received");
            ws->state = WS_STATE_CLOSED;
            if (ws->on_event) {
                ws->on_event(ws, XZ_WS_EVENT_DISCONNECTED, ws->user);
            }
            continue;
        }

        /* 处理 Ping 帧 */
        if (opcode == 0x09) {
            LISA_LOGI(TAG, "WebSocket PING frame received");
            /* 读取 ping 负载 */
            if (payload_len > 0 && payload_len < WS_BUFFER_SIZE) {
                uint64_t total = 0;
                while (total < payload_len) {
                    int to_read = payload_len - total;
                    if (to_read > 1024) to_read = 1024;
                    n = xz_tls_recv(ws->tls, ws->buffer + total, to_read);
                    if (n < 0) break;
                    if (n == 0) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                        continue;
                    }
                    total += n;
                }
                /* 反掩码 */
                if (masked) {
                    for (uint64_t i = 0; i < payload_len; i++) {
                        ws->buffer[i] ^= mask[i % 4];
                    }
                }
            }

            /* 发送 Pong 响应 */
            uint8_t pong_header[2] = {0x8A, 0x00};
            xz_tls_send(ws->tls, pong_header, 2);
            continue;
        }

        /* 处理 Pong 帧 */
        if (opcode == 0x0A) {
            LISA_LOGI(TAG, "WebSocket PONG frame received (%llu bytes)", (unsigned long long)payload_len);
            /* 读取并丢弃 */
            if (payload_len > 0 && payload_len < WS_BUFFER_SIZE) {
                uint64_t total = 0;
                while (total < payload_len) {
                    int to_read = payload_len - total;
                    if (to_read > 1024) to_read = 1024;
                    n = xz_tls_recv(ws->tls, ws->buffer + total, to_read);
                    if (n < 0) break;
                    if (n == 0) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                        continue;
                    }
                    total += n;
                }
            }
            /* 通知应用层收到 PONG - 用于心跳监控 */
            if (ws->on_data) {
                ws->on_data(ws, XZ_WS_DATA_PONG, NULL, 0, ws->user);
            }
            continue;
        }

        /* 读取负载数据 */
        if (payload_len == 0) {
            if (ws->on_data && (opcode == 1 || opcode == 2)) {
                xz_ws_data_type_e type = (opcode == 1) ? XZ_WS_DATA_TEXT : XZ_WS_DATA_BINARY;
                ws->on_data(ws, type, NULL, 0, ws->user);
            }
        } else if (payload_len < WS_BUFFER_SIZE) {
            uint64_t total = 0;
            while (total < payload_len) {
                int to_read = payload_len - total;
                if (to_read > 1024) to_read = 1024;
                n = xz_tls_recv(ws->tls, ws->buffer + total, to_read);
                if (n < 0) {
                    LISA_LOGE(TAG, "Failed to read payload");
                    ws->state = WS_STATE_CLOSED;
                    break;
                }
                if (n == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                total += n;
            }

            /* 反掩码 */
            if (masked) {
                for (uint64_t i = 0; i < payload_len; i++) {
                    ws->buffer[i] ^= mask[i % 4];
                }
            }

            /* 调用数据回调 */
            if (ws->on_data && (opcode == 1 || opcode == 2)) {
                xz_ws_data_type_e type = (opcode == 1) ? XZ_WS_DATA_TEXT : XZ_WS_DATA_BINARY;
                ws->on_data(ws, type, ws->buffer, payload_len, ws->user);
            }
        } else {
            /* 跳过大帧 */
            LISA_LOGW(TAG, "Skipping large frame: %llu bytes", (unsigned long long)payload_len);
            uint64_t total = 0;
            while (total < payload_len) {
                int to_read = payload_len - total;
                if (to_read > 1024) to_read = 1024;
                n = xz_tls_recv(ws->tls, ws->buffer, to_read);
                if (n < 0) break;
                if (n == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                total += n;
            }
        }
    }

    LISA_LOGI(TAG, "Thread exited");
}

xz_websocket_t xz_ws_create(const xz_ws_config_t *config)
{
    struct xz_websocket_s *ws;

    if (!config || !config->url) {
        LISA_LOGE(TAG, "Invalid config");
        return NULL;
    }

    ws = (struct xz_websocket_s *)lisa_mem_calloc(1, sizeof(struct xz_websocket_s));
    if (!ws) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    /* 解析 URL */
    if (parse_ws_url(ws, config->url) != 0) {
        lisa_mem_free(ws);
        return NULL;
    }

    /* 复制配置 */
    if (config->token) {
        strncpy(ws->token, config->token, sizeof(ws->token) - 1);
    }
    if (config->device_id) {
        strncpy(ws->device_id, config->device_id, sizeof(ws->device_id) - 1);
    }
    if (config->client_id) {
        strncpy(ws->client_id, config->client_id, sizeof(ws->client_id) - 1);
    }
    ws->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 10000;
    ws->user = config->user;
    ws->on_event = config->on_event;
    ws->on_data = config->on_data;
    ws->state = WS_STATE_CLOSED;
    ws->running = false;

    /* 创建 TLS 连接 */
    xz_tls_config_t tls_config = {
        .verify_cert = false,
        .timeout_ms = ws->timeout_ms,
    };
    ws->tls = xz_tls_create(&tls_config);
    if (!ws->tls) {
        LISA_LOGE(TAG, "Failed to create TLS context");
        lisa_mem_free(ws);
        return NULL;
    }

    LISA_LOGI(TAG, "WebSocket created: %s", config->url);
    return ws;
}

void xz_ws_destroy(xz_websocket_t ws)
{
    if (!ws) {
        return;
    }

    ws->running = false;

    if (ws->state == WS_STATE_OPEN) {
        xz_ws_disconnect(ws);
    }

    if (ws->tls) {
        xz_tls_destroy(ws->tls);
    }

    lisa_mem_free(ws);
}

int xz_ws_connect(xz_websocket_t ws)
{
    if (!ws) {
        return -1;
    }

    LISA_LOGI(TAG, "Connecting to %s:%u", ws->host, ws->port);

    /* 连接 TLS */
    if (xz_tls_connect(ws->tls, ws->host, ws->port) != 0) {
        LISA_LOGE(TAG, "TLS connection failed");
        if (ws->on_event) {
            ws->on_event(ws, XZ_WS_EVENT_ERROR, ws->user);
        }
        return -1;
    }

    /* 执行 WebSocket 握手 */
    if (perform_handshake(ws) != 0) {
        LISA_LOGE(TAG, "WebSocket handshake failed");
        xz_tls_close(ws->tls);
        if (ws->on_event) {
            ws->on_event(ws, XZ_WS_EVENT_ERROR, ws->user);
        }
        return -1;
    }

    ws->state = WS_STATE_OPEN;

    /* 启动接收线程 */
    if (!ws->running) {
        lisa_thread_attr_t attr = {
            .name = "xz_ws",
            .stack_size = 16 * 1024,
            .priority = LISA_OS_PRIORITY_NORMAL,
        };
        ws->running = true;
        ws->thread = lisa_thread_create(&attr, ws_thread, ws);
        if (!ws->thread) {
            LISA_LOGE(TAG, "Failed to create thread");
            ws->running = false;
            xz_tls_close(ws->tls);
            ws->state = WS_STATE_CLOSED;
            return -1;
        }
    }

    LISA_LOGI(TAG, "WebSocket connected");

    if (ws->on_event) {
        ws->on_event(ws, XZ_WS_EVENT_CONNECTED, ws->user);
    }

    return 0;
}

int xz_ws_disconnect(xz_websocket_t ws)
{
    if (!ws || ws->state != WS_STATE_OPEN) {
        return -1;
    }

    LISA_LOGI(TAG, "Disconnecting...");

    /* 发送关闭帧 */
    uint8_t close_frame[2] = {0x88, 0x00};
    xz_tls_send(ws->tls, close_frame, 2);

    ws->running = false;
    ws->state = WS_STATE_CLOSED;

    xz_tls_close(ws->tls);

    return 0;
}

int xz_ws_send_text(xz_websocket_t ws, const char *text)
{
    if (!ws || !text || ws->state != WS_STATE_OPEN) {
        LISA_LOGE(TAG, "send_text failed: ws=%p, text=%p, state=%d", ws, text, ws ? ws->state : -1);
        return -1;
    }

    size_t len = strlen(text);
    uint8_t frame[WS_MAX_HEADER_SIZE + len];
    size_t frame_len = 0;

    /* 构建帧头 */
    frame[0] = 0x81;  /* FIN + TEXT */

    if (len <= 125) {
        frame[1] = len | 0x80;  /* MASK */
        frame_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126 | 0x80;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        LISA_LOGE(TAG, "Payload too large: %zu", len);
        return -1;
    }

    /* 生成掩码 */
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(frame + frame_len, mask, 4);
    frame_len += 4;

    /* 掩码数据 */
    for (size_t i = 0; i < len; i++) {
        frame[frame_len + i] = text[i] ^ mask[i % 4];
    }
    frame_len += len;

    /* 发送 */
    int sent = xz_tls_send(ws->tls, frame, frame_len);
    if (sent != (int)frame_len) {
        LISA_LOGE(TAG, "Failed to send frame");
        return -1;
    }

    LISA_LOGI(TAG, "Sent text frame: %zu bytes", frame_len);
    return 0;
}

int xz_ws_send_binary(xz_websocket_t ws, const void *data, uint32_t len)
{
    if (!ws || !data || ws->state != WS_STATE_OPEN) {
        return -1;
    }

    uint8_t frame[WS_MAX_HEADER_SIZE];
    size_t frame_len = 0;

    /* 构建帧头 */
    frame[0] = 0x82;  /* FIN + BINARY */

    if (len <= 125) {
        frame[1] = len | 0x80;  /* MASK */
        frame_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126 | 0x80;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        LISA_LOGE(TAG, "Payload too large: %u", len);
        return -1;
    }

    /* 生成掩码 */
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(frame + frame_len, mask, 4);
    frame_len += 4;

    /* 发送帧头 */
    int sent = xz_tls_send(ws->tls, frame, frame_len);
    if (sent != (int)frame_len) {
        LISA_LOGE(TAG, "Failed to send frame header");
        return -1;
    }

    /* 掩码并发送数据 */
    const uint8_t *src = (const uint8_t *)data;
    uint8_t *masked_buf = (uint8_t *)lisa_mem_alloc(len);
    if (!masked_buf) {
        return -1;
    }

    for (uint32_t i = 0; i < len; i++) {
        masked_buf[i] = src[i] ^ mask[i % 4];
    }

    sent = xz_tls_send(ws->tls, masked_buf, len);
    lisa_mem_free(masked_buf);

    if (sent != (int)len) {
        LISA_LOGE(TAG, "Failed to send frame data");
        return -1;
    }

    LISA_LOGI(TAG, "Sent binary frame: %u bytes", len);
    return 0;
}

bool xz_ws_is_connected(xz_websocket_t ws)
{
    return ws && ws->state == WS_STATE_OPEN;
}

void *xz_ws_get_user_data(xz_websocket_t ws)
{
    return ws ? ws->user : NULL;
}
