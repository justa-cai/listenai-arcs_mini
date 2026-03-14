/**
 * @file xz_ota.c
 * @brief 小智云端 OTA 激活模块实现
 */

#define TAG "xz_ota"

#include "xz_ota.h"
#include "xz_tls.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_kv.h"
#include "lisa_time.h"
#include "wifi/wifi_api.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* KV 存储键 */
#define KV_XZ_WS_URL        "xz.ws_url"
#define KV_XZ_WS_TOKEN      "xz.ws_token"
#define KV_XZ_SERVER_TIME   "xz.server_time"

/* HTTP 响应缓冲区大小 */
#define HTTP_RESPONSE_BUF_SIZE  4096

/**
 * @brief 简单的 URL 解析
 */
static int parse_url(const char *url, char *host, size_t host_buf_len,
                     uint16_t *port, char *path, size_t path_len,
                     bool *use_ssl)
{
    if (strncmp(url, "https://", 8) == 0) {
        *use_ssl = true;
        url += 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        *use_ssl = false;
        url += 7;
    } else {
        return -1;
    }

    const char *slash = strchr(url, '/');
    const char *colon = strchr(url, ':');

    /* 提取主机和端口 */
    if (colon && (!slash || colon < slash)) {
        size_t len = colon - url;
        if (len >= host_buf_len) len = host_buf_len - 1;
        strncpy(host, url, len);
        host[len] = '\0';
        *port = (uint16_t)atoi(colon + 1);

        if (slash) {
            strncpy(path, slash, path_len - 1);
        } else {
            strcpy(path, "/");
        }
    } else if (slash) {
        size_t len = slash - url;
        if (len >= host_buf_len) len = host_buf_len - 1;
        strncpy(host, url, len);
        host[len] = '\0';
        *port = *use_ssl ? 443 : 80;
        strncpy(path, slash, path_len - 1);
    } else {
        strncpy(host, url, host_buf_len - 1);
        *port = *use_ssl ? 443 : 80;
        strcpy(path, "/");
    }

    return 0;
}

/**
 * @brief 发送 HTTP POST 请求 (使用 mbedTLS + lwIP)
 */
static int http_post(const char *url, const char *json_data,
                     const char *device_id, const char *app_version,
                     const char *chip_model, const char *board_type,
                     char *response, size_t response_len)
{
    char host[256];
    char path[256];
    uint16_t port;
    bool use_ssl;
    xz_tls_t tls = NULL;
    int ret = -1;

    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path), &use_ssl) != 0) {
        LISA_LOGE(TAG, "Invalid URL: %s", url);
        return -1;
    }

    LISA_LOGI(TAG, "HTTP POST to %s:%u%s (SSL=%d)", host, port, path, use_ssl);

    /* 如果使用 SSL，创建 TLS 连接 */
    if (use_ssl) {
        xz_tls_config_t tls_config = {
            .verify_cert = false,
            .timeout_ms = 5000,  /* 5秒超时 */
        };
        tls = xz_tls_create(&tls_config);
        if (!tls) {
            LISA_LOGE(TAG, "Failed to create TLS context");
            return -1;
        }

        if (xz_tls_connect(tls, host, port) != 0) {
            LISA_LOGE(TAG, "TLS connection failed");
            xz_tls_destroy(tls);
            return -1;
        }
    } else {
        LISA_LOGE(TAG, "HTTP not supported, only HTTPS");
        return -1;
    }

    /* 构建 HTTP POST 请求 */
    char request[4096];
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "User-Agent: XiaoZhi-linux/1.0\r\n"
        "Content-Length: %zu\r\n"
        "%s%s%s"  /* Device-Id */
        "%s%s%s"  /* App-Version */
        "%s%s%s"  /* Chip-Model */
        "%s%s%s"  /* Board-Type */
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, strlen(json_data),
        device_id ? "Device-Id: " : "", device_id ? device_id : "", device_id ? "\r\n" : "",
        app_version ? "App-Version: " : "", app_version ? app_version : "", app_version ? "\r\n" : "",
        chip_model ? "Chip-Model: " : "", chip_model ? chip_model : "", chip_model ? "\r\n" : "",
        board_type ? "Board-Type: " : "", board_type ? board_type : "", board_type ? "\r\n" : "",
        json_data);

    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        LISA_LOGE(TAG, "Request too large");
        goto cleanup;
    }

    LISA_LOGI(TAG, "Sending HTTP request (%d bytes):\n%.*s", req_len, req_len > 500 ? 500 : req_len, request);
    LISA_LOGI(TAG, "JSON body (%zu bytes): %s", strlen(json_data), json_data);
    LISA_LOGI(TAG, "Params: device_id=%s, app_version=%s, chip_model=%s, board_type=%s",
              device_id ? device_id : "(null)",
              app_version ? app_version : "(null)",
              chip_model ? chip_model : "(null)",
              board_type ? board_type : "(null)");

    /* 发送请求 */
    int sent = use_ssl ? xz_tls_send(tls, request, req_len) : -1;
    if (sent != req_len) {
        LISA_LOGE(TAG, "Failed to send request: sent=%d, expected=%d", sent, req_len);
        goto cleanup;
    }

    LISA_LOGI(TAG, "Request sent successfully (%d bytes)", sent);

    /* 接收响应 */
    size_t total_received = 0;
    char *resp_ptr = response;
    int retry_count = 0;
    const int max_retries = 10;  /* 防止无限循环 */

    while (total_received < response_len - 1 && retry_count < max_retries) {
        int to_read = response_len - total_received - 1;
        if (to_read > 1024) to_read = 1024;

        int n = use_ssl ? xz_tls_recv(tls, resp_ptr, to_read) : -1;
        if (n < 0) {
            /* Error or connection closed */
            LISA_LOGE(TAG, "Receive error at %d bytes", total_received);
            break;
        }
        if (n == 0) {
            /* No data available right now (WANT_READ/WANT_WRITE), retry */
            retry_count++;
            LISA_LOGW(TAG, "No data available, retry %d/%d", retry_count, max_retries);
            continue;
        }

        /* 成功接收数据，重置重试计数 */
        retry_count = 0;
        total_received += n;
        resp_ptr += n;

        LISA_LOGI(TAG, "Received %d bytes (total: %d)", n, total_received);

        /* 检查是否收到完整响应 */
        if (total_received > 4) {
            /* 查找 Content-Length 或连接关闭 */
            if (strstr(response, "\r\n\r\n") != NULL) {
                /* 检查是否可以关闭连接 (Connection: close) */
                if (strstr(response, "Connection: close") != NULL) {
                    /* 继续读取直到连接关闭 */
                } else {
                    /* 检查 Content-Length */
                    char *content_len = strstr(response, "Content-Length:");
                    if (content_len) {
                        int len = atoi(content_len + 16);
                        char *body_start = strstr(response, "\r\n\r\n");
                        if (body_start) {
                            body_start += 4;
                            int current_body_len = total_received - (body_start - response);
                            LISA_LOGI(TAG, "Body progress: %d/%d", current_body_len, len);
                            if (current_body_len >= len) {
                                LISA_LOGI(TAG, "Complete response received");
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (retry_count >= max_retries) {
        LISA_LOGE(TAG, "Max retries exceeded");
        ret = -1;
        goto cleanup;
    }

    response[total_received] = '\0';
    ret = 0;

cleanup:
    if (tls) {
        xz_tls_destroy(tls);
    }

    return ret;
}

/**
 * @brief 解析 JSON 响应
 */
static int parse_activation_response(const char *json_str, xz_ota_response_t *response)
{
    /* 简单 JSON 解析 - 查找关键字段 */
    const char *ws_url_start = strstr(json_str, "\"url\"");
    const char *ws_token_start = strstr(json_str, "\"token\"");

    if (ws_url_start) {
        /* 跳过 "url": " */
        ws_url_start = strchr(ws_url_start + 6, '"');
        if (ws_url_start) {
            ws_url_start++; /* 跳过引号 */
            const char *ws_url_end = strchr(ws_url_start, '"');
            if (ws_url_end) {
                size_t len = ws_url_end - ws_url_start;
                if (len < sizeof(response->websocket.url)) {
                    strncpy(response->websocket.url, ws_url_start, len);
                    response->websocket.url[len] = '\0';
                }
            }
        }
    }

    if (ws_token_start) {
        /* 跳过 "token": " */
        ws_token_start = strchr(ws_token_start + 8, '"');
        if (ws_token_start) {
            ws_token_start++; /* 跳过引号 */
            const char *ws_token_end = strchr(ws_token_start, '"');
            if (ws_token_end) {
                size_t len = ws_token_end - ws_token_start;
                if (len < sizeof(response->websocket.token)) {
                    strncpy(response->websocket.token, ws_token_start, len);
                    response->websocket.token[len] = '\0';
                }
            }
        }
    }

    /* 检查是否成功获取了 URL 和 token */
    if (strlen(response->websocket.url) > 0 && strlen(response->websocket.token) > 0) {
        response->success = true;
        return 0;
    }

    response->success = false;
    strcpy(response->error_message, "Failed to parse activation response");
    return -1;
}

int xz_device_get_mac(char *mac_out, size_t mac_len)
{
    uint8_t mac_bytes[6];

    if (mac_len < 18) {
        return -1;
    }

    /* 从 WiFi 模块获取 MAC 地址 */
    if (wifi_get_sta_mac(mac_bytes) == LS_OK) {
        /* 格式化为 AA:BB:CC:DD:EE:FF (小写) */
        snprintf(mac_out, mac_len, "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac_bytes[0], mac_bytes[1], mac_bytes[2],
                 mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        LISA_LOGI(TAG, "Got MAC address: %s", mac_out);
        return 0;
    }

    /* 获取失败，使用默认值 */
    LISA_LOGW(TAG, "Failed to get MAC from WiFi, using default");
    strcpy(mac_out, "aa:bb:cc:dd:ee:ff");
    return 0;
}

int xz_device_generate_uuid(char *uuid_out, size_t uuid_len)
{
    if (uuid_len < 37) {
        return -1;
    }

    /* 简单的 UUID 生成 - 基于 MAC 地址和时间 */
    uint32_t tick = lisa_os_get_tick_ms();
    snprintf(uuid_out, uuid_len,
             "%08x-%04x-%04x-%04x-%012x",
             (unsigned int)(tick ^ 0x12345678),
             (unsigned int)((tick >> 16) & 0xFFFF),
             (unsigned int)((tick >> 8) & 0xFFFF),
             (unsigned int)(tick & 0xFFFF),
             (unsigned long long)(tick * 0xABCD));

    return 0;
}

int xz_device_init_info(xz_device_info_t *info)
{
    if (!info) {
        return -1;
    }

    memset(info, 0, sizeof(xz_device_info_t));

    /* 获取 MAC 地址 */
    if (xz_device_get_mac(info->mac_address, sizeof(info->mac_address)) != 0) {
        strcpy(info->mac_address, "AA:BB:CC:DD:EE:FF");
    }

    /* 生成客户端 UUID */
    if (xz_device_generate_uuid(info->client_id, sizeof(info->client_id)) != 0) {
        strcpy(info->client_id, "default-client-id");
    }

    /* 设置硬件信息 */
    strcpy(info->board_type, "arcs_mini");
    strcpy(info->app_version, "1.7.0");
    strcpy(info->chip_model, "AB230NxA");
    info->flash_size = 16 * 1024 * 1024;  /* 16MB */
    info->ram_size = 192 * 1024;           /* 192KB SRAM + 8MB PSRAM */

    return 0;
}

int xz_ota_activate(const char *server_url,
                    const xz_device_info_t *device,
                    xz_ota_response_t *response)
{
    if (!server_url || !device || !response) {
        return -1;
    }

    memset(response, 0, sizeof(xz_ota_response_t));

    /* 构建 JSON 请求数据 - 匹配 xiaozhi-linux 格式 */
    char request_data[2048];
    int len = snprintf(request_data, sizeof(request_data),
        "{"
        "\"flash_size\": %d,"
        "\"minimum_free_heap_size\": 8318916,"
        "\"mac_address\": \"%s\","
        "\"chip_model_name\": \"%s\","
        "\"chip_info\": {"
        "\"model\": 9,"
        "\"cores\": 8,"
        "\"revision\": 2,"
        "\"features\": 18"
        "},"
        "\"application\": {"
        "\"name\": \"xiaozhi\","
        "\"version\": \"%s\","
        "\"idf_version\": \"v5.3.2\""
        "},"
        "\"partition_table\": [],"
        "\"ota\": {"
        "\"label\": \"factory\""
        "},"
        "\"board\": {"
        "\"type\": \"%s\","
        "\"mac\": \"%s\""
        "}"
        "}",
        device->flash_size,
        device->mac_address,
        device->chip_model,
        device->app_version,
        device->board_type,
        device->mac_address);

    if (len < 0 || len >= (int)sizeof(request_data)) {
        LISA_LOGE(TAG, "Request data too large");
        return -1;
    }

    LISA_LOGI(TAG, "Sending activation request to %s", server_url);

    /* 发送 HTTP POST 请求 */
    char response_buf[HTTP_RESPONSE_BUF_SIZE];
    if (http_post(server_url, request_data,
                  device->mac_address, device->app_version,
                  device->chip_model, device->board_type,
                  response_buf, sizeof(response_buf)) != 0) {
        LISA_LOGE(TAG, "HTTP POST request failed");
        response->success = false;
        strcpy(response->error_message, "HTTP request failed");
        return -1;
    }

    LISA_LOGI(TAG, "Activation response: %s", response_buf);

    /* 解析响应 */
    if (parse_activation_response(response_buf, response) != 0) {
        LISA_LOGE(TAG, "Failed to parse activation response");
        return -1;
    }

    if (response->success) {
        LISA_LOGI(TAG, "Activation successful!");
        LISA_LOGI(TAG, "  WebSocket URL: %s", response->websocket.url);
        LISA_LOGI(TAG, "  WebSocket Token: %.*s", 20, response->websocket.token);

        /* 保存到 KV 存储 */
        xz_ota_save_activation(response);
    }

    return 0;
}

int xz_ota_save_activation(const xz_ota_response_t *response)
{
    if (!response || !response->success) {
        return -1;
    }

    lisa_kv_set_string(KV_XZ_WS_URL, response->websocket.url);
    lisa_kv_set_string(KV_XZ_WS_TOKEN, response->websocket.token);

    LISA_LOGI(TAG, "Activation info saved to KV storage");
    return 0;
}

int xz_ota_load_activation(xz_ota_response_t *response)
{
    if (!response) {
        return -1;
    }

    memset(response, 0, sizeof(xz_ota_response_t));

    char *url = NULL;
    char *token = NULL;

    if (lisa_kv_get_string(KV_XZ_WS_URL, &url) != 0 || !url) {
        return -1;
    }

    if (lisa_kv_get_string(KV_XZ_WS_TOKEN, &token) != 0 || !token) {
        lisa_mem_free(url);
        return -1;
    }

    strncpy(response->websocket.url, url, sizeof(response->websocket.url) - 1);
    strncpy(response->websocket.token, token, sizeof(response->websocket.token) - 1);
    response->success = true;

    lisa_mem_free(url);
    lisa_mem_free(token);

    LISA_LOGI(TAG, "Loaded activation from KV storage");
    LISA_LOGI(TAG, "  WebSocket URL: %s", response->websocket.url);
    return 0;
}

bool xz_ota_is_activated(void)
{
    xz_ota_response_t response;
    return xz_ota_load_activation(&response) == 0 && response.success;
}

void xz_ota_clear_activation(void)
{
    lisa_kv_del(KV_XZ_WS_URL);
    lisa_kv_del(KV_XZ_WS_TOKEN);
    LISA_LOGI(TAG, "Activation info cleared");
}
