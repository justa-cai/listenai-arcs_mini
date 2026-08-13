/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "4g.usb.speed"
#include <lisa_log.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <lwip/inet.h>
#include <lwip/sockets.h>

#include "lisa_modem_module.h"
#include "modem_usb_at_backend.h"

#define SPEEDTEST_URL_1M       "http://47.107.81.30:80/test_1M.bin"
#define SPEEDTEST_URL_5M       "http://47.107.81.30:80/test_5M.bin"
#define SPEEDTEST_URL_CUSTOMER "http://test-qiniu-oss1.phone580.net/fzs-box/rk3308ota/music/yiqiannianyihou.mp3"

#define SPEEDTEST_RECV_BUF_SIZE   (16 * 1024)
#define SPEEDTEST_HEADER_BUF_SIZE (2 * 1024)
#define SPEEDTEST_SEND_TIMEOUT_MS 10000
#define SPEEDTEST_RECV_TIMEOUT_MS 30000
#define SPEEDTEST_PROGRESS_LOG_INTERVAL_MS 1000U

typedef struct {
    const char *name;
    const char *url;
} speedtest_target_t;

typedef struct {
    char host[128];
    char path[512];
    uint16_t port;
} speedtest_url_info_t;

static uint32_t speedtest_now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static bool speedtest_parse_int_field(const char **cursor, int *value)
{
    const char *p;
    int parsed = 0;
    bool has_digit = false;

    if (!cursor || !*cursor || !value) {
        return false;
    }

    p = *cursor;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        has_digit = true;
        parsed = (parsed * 10) + (*p - '0');
        p++;
    }

    if (!has_digit) {
        return false;
    }

    *cursor = p;
    *value = parsed;
    return true;
}

static bool speedtest_parse_csq(const char *response, int *csq, int *ber)
{
    const char *line;

    if (!response || !csq || !ber) {
        return false;
    }

    line = strstr(response, "+CSQ:");
    if (!line) {
        return false;
    }

    line += strlen("+CSQ:");

    if (!speedtest_parse_int_field(&line, csq)) {
        return false;
    }

    while (*line == ' ' || *line == '\t') {
        line++;
    }

    if (*line != ',') {
        return false;
    }
    line++;

    return speedtest_parse_int_field(&line, ber);
}

static bool speedtest_csq_to_dbm(int csq, int *dbm)
{
    if (!dbm || csq == 99 || csq < 0) {
        return false;
    }

    if (csq == 0) {
        *dbm = -113;
    } else if (csq >= 31) {
        *dbm = -51;
    } else {
        *dbm = -113 + (2 * csq);
    }

    return true;
}

static void speedtest_log_signal(lisa_modem_t *modem, const char *name, const char *phase)
{
    at_client_t *client;
    char response[96] = {0};
    int csq = -1;
    int ber = -1;
    int dbm = 0;

    client = lisa_modem_get_client(modem);
    if (!client) {
        LISA_LOGE(LOG_TAG, "%s %s signal: modem AT client unavailable", phase, name);
        return;
    }

    if (!at_client_exec_text_cmd(client, "AT+CSQ", response, sizeof(response), 2000U)) {
        LISA_LOGE(LOG_TAG, "%s %s signal: AT+CSQ failed", phase, name);
        return;
    }

    if (!speedtest_parse_csq(response, &csq, &ber)) {
        LISA_LOGE(LOG_TAG, "%s %s signal: parse failed, raw=%s", phase, name, response);
        return;
    }

    if (speedtest_csq_to_dbm(csq, &dbm)) {
        LISA_LOGI(LOG_TAG, "%s %s signal: csq=%d, rssi=%d dBm, ber=%d",
                  phase, name, csq, dbm, ber);
    } else {
        LISA_LOGI(LOG_TAG, "%s %s signal: csq=%d, rssi=unknown, ber=%d",
                  phase, name, csq, ber);
    }
}

static bool ascii_equal_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return a == b;
}

static bool ascii_starts_with_ci(const char *s, size_t len, const char *prefix)
{
    size_t prefix_len = strlen(prefix);

    if (len < prefix_len) {
        return false;
    }

    for (size_t i = 0; i < prefix_len; i++) {
        if (!ascii_equal_ci(s[i], prefix[i])) {
            return false;
        }
    }

    return true;
}

static bool speedtest_parse_url(const char *url, speedtest_url_info_t *info)
{
    const char *host_start;
    const char *host_end;
    const char *path_start;
    const char *colon;
    size_t host_len;

    if (!url || !info || strncmp(url, "http://", 7) != 0) {
        return false;
    }

    memset(info, 0, sizeof(*info));
    info->port = 80;

    host_start = url + 7;
    path_start = strchr(host_start, '/');
    host_end = path_start ? path_start : url + strlen(url);
    colon = memchr(host_start, ':', (size_t)(host_end - host_start));

    if (colon) {
        int port = atoi(colon + 1);

        if (port <= 0 || port > UINT16_MAX) {
            return false;
        }
        info->port = (uint16_t)port;
        host_len = (size_t)(colon - host_start);
    } else {
        host_len = (size_t)(host_end - host_start);
    }

    if (host_len == 0 || host_len >= sizeof(info->host)) {
        return false;
    }
    memcpy(info->host, host_start, host_len);
    info->host[host_len] = '\0';

    if (path_start) {
        if (strlen(path_start) >= sizeof(info->path)) {
            return false;
        }
        strcpy(info->path, path_start);
    } else {
        strcpy(info->path, "/");
    }

    return true;
}

static bool speedtest_resolve_addr(lisa_modem_t *modem, const speedtest_url_info_t *url, struct sockaddr_in *addr)
{
    char ip_addr[64] = {0};

    if (!modem || !url || !addr) {
        return false;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(url->port);

    if (inet_pton(AF_INET, url->host, &addr->sin_addr) == 1) {
        return true;
    }

    LISA_LOGI(LOG_TAG, "Resolving %s", url->host);
    if (!lisa_modem_dns_resolve_on(modem, url->host, ip_addr, sizeof(ip_addr))) {
        LISA_LOGE(LOG_TAG, "DNS resolve failed: %s", url->host);
        return false;
    }

    LISA_LOGI(LOG_TAG, "Resolved %s -> %s", url->host, ip_addr);
    return inet_pton(AF_INET, ip_addr, &addr->sin_addr) == 1;
}

static int speedtest_send_all(lisa_modem_t *modem, int sockfd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        int ret = lisa_modem_socket_send_on(modem, sockfd, data + sent, len - sent, SPEEDTEST_SEND_TIMEOUT_MS);

        if (ret <= 0) {
            return -1;
        }
        sent += (size_t)ret;
    }

    return 0;
}

static int speedtest_parse_http_status(const char *header)
{
    const char *space = strchr(header, ' ');

    if (!space) {
        return -1;
    }

    return atoi(space + 1);
}

static int speedtest_parse_content_length(const char *header)
{
    const char *line = header;

    while (line && *line) {
        const char *next = strstr(line, "\r\n");
        size_t line_len = next ? (size_t)(next - line) : strlen(line);

        if (ascii_starts_with_ci(line, line_len, "Content-Length:")) {
            const char *value = line + strlen("Content-Length:");

            while (value < line + line_len && (*value == ' ' || *value == '\t')) {
                value++;
            }
            return atoi(value);
        }

        if (!next) {
            break;
        }
        line = next + 2;
    }

    return -1;
}

static void speedtest_log_result(const char *name, uint32_t bytes, uint32_t elapsed_ms)
{
    uint32_t kib_s_x100;
    uint32_t mbps_x100;
    uint32_t total_kib_x100;
    uint32_t total_mib_x100;

    if (elapsed_ms == 0) {
        elapsed_ms = 1;
    }

    kib_s_x100 = (uint32_t)(((uint64_t)bytes * 1000ULL * 100ULL) / elapsed_ms / 1024ULL);
    mbps_x100 = (uint32_t)(((uint64_t)bytes * 8ULL * 1000ULL * 100ULL) / elapsed_ms / 1000000ULL);
    total_kib_x100 = (uint32_t)(((uint64_t)bytes * 100ULL) / 1024ULL);
    total_mib_x100 = (uint32_t)(((uint64_t)bytes * 100ULL) / 1024ULL / 1024ULL);

    LISA_LOGI(LOG_TAG, "%s result:", name);
    LISA_LOGI(LOG_TAG, "  total size: %u bytes (%u.%02u KiB, %u.%02u MiB)",
              bytes,
              total_kib_x100 / 100,
              total_kib_x100 % 100,
              total_mib_x100 / 100,
              total_mib_x100 % 100);
    LISA_LOGI(LOG_TAG, "  total time: %u ms (%u.%03u s)", elapsed_ms, elapsed_ms / 1000, elapsed_ms % 1000);
    LISA_LOGI(LOG_TAG, "  average speed: %u.%02u KiB/s (%u.%02u Mbps)",
	              kib_s_x100 / 100,
	              kib_s_x100 % 100,
	              mbps_x100 / 100,
	              mbps_x100 % 100);
}

static void speedtest_log_recv_progress(const char *name,
                                        uint32_t now_ms,
                                        uint32_t *window_ms,
                                        uint32_t *window_body_start,
                                        uint32_t *window_raw_bytes,
                                        uint32_t *window_calls,
                                        uint32_t *window_wait_ms,
                                        uint32_t *window_max_wait_ms,
                                        uint32_t total_body,
                                        int content_length,
                                        bool header_done)
{
    uint32_t elapsed_ms;
    uint32_t body_delta;
    uint32_t body_rate_kib_s_x100;
    uint32_t raw_rate_kib_s_x100;
    uint32_t avg_wait_ms;

    if (!window_ms || !window_body_start || !window_raw_bytes ||
        !window_calls || !window_wait_ms || !window_max_wait_ms) {
        return;
    }

    elapsed_ms = now_ms - *window_ms;
    if (elapsed_ms < SPEEDTEST_PROGRESS_LOG_INTERVAL_MS) {
        return;
    }

    body_delta = total_body - *window_body_start;
    body_rate_kib_s_x100 = elapsed_ms > 0U
                         ? (uint32_t)(((uint64_t)body_delta * 1000ULL * 100ULL) /
                                      (uint64_t)elapsed_ms / 1024ULL)
                         : 0U;
    raw_rate_kib_s_x100 = elapsed_ms > 0U
                        ? (uint32_t)(((uint64_t)*window_raw_bytes * 1000ULL * 100ULL) /
                                     (uint64_t)elapsed_ms / 1024ULL)
                        : 0U;
    avg_wait_ms = *window_calls > 0U ? *window_wait_ms / *window_calls : 0U;

    LISA_LOGI(LOG_TAG,
              "%s app recv: body=%u(+%u, %u.%02u KiB/s), raw=%u (%u.%02u KiB/s), calls=%u, avg_wait=%u ms, max_wait=%u ms, header=%d, content_length=%d",
              name,
              total_body,
              body_delta,
              body_rate_kib_s_x100 / 100U,
              body_rate_kib_s_x100 % 100U,
              *window_raw_bytes,
              raw_rate_kib_s_x100 / 100U,
              raw_rate_kib_s_x100 % 100U,
              *window_calls,
              avg_wait_ms,
              *window_max_wait_ms,
              header_done ? 1 : 0,
              content_length);

    *window_ms = now_ms;
    *window_body_start = total_body;
    *window_raw_bytes = 0U;
    *window_calls = 0U;
    *window_wait_ms = 0U;
    *window_max_wait_ms = 0U;
}

static int speedtest_count_body(const char *name,
                                const uint8_t *body,
                                size_t body_len,
                                int content_length,
                                uint32_t *total_body,
                                uint32_t *start_ms)
{
    size_t count = body_len;

    if (body_len == 0) {
        return 0;
    }

    if (*start_ms == 0) {
        *start_ms = speedtest_now_ms();
    }

    if (content_length > 0 && (*total_body + count) > (uint32_t)content_length) {
        count = (uint32_t)content_length - *total_body;
        LISA_LOGI(LOG_TAG, "%s received %u extra bytes after Content-Length", name, (uint32_t)(body_len - count));
    }

    *total_body += (uint32_t)count;
    return (content_length > 0 && *total_body >= (uint32_t)content_length) ? 1 : 0;
}

static int speedtest_pull_one(lisa_modem_t *modem, const speedtest_target_t *target)
{
    int sockfd = -1;
    int ret = -1;
    int http_status = -1;
    int content_length = -1;
    bool header_done = false;
    uint32_t total_body = 0;
    uint32_t body_start_ms = 0;
    uint8_t *recv_buf = NULL;
    char header_buf[SPEEDTEST_HEADER_BUF_SIZE];
    size_t header_len = 0;
    speedtest_url_info_t url;
    struct sockaddr_in addr;
    char request[1024];
    char host_header[160];
    char ip_text[32] = {0};
    uint32_t recv_window_ms = 0;
    uint32_t recv_window_body_start = 0;
    uint32_t recv_window_raw_bytes = 0;
    uint32_t recv_window_calls = 0;
    uint32_t recv_window_wait_ms = 0;
    uint32_t recv_window_max_wait_ms = 0;

    if (!target || !speedtest_parse_url(target->url, &url)) {
        LISA_LOGE(LOG_TAG, "%s invalid URL: %s", target ? target->name : "unknown", target ? target->url : "(null)");
        return -1;
    }

    recv_buf = (uint8_t *)pvPortMalloc(SPEEDTEST_RECV_BUF_SIZE);
    if (!recv_buf) {
        LISA_LOGE(LOG_TAG, "alloc %u bytes recv buffer failed", SPEEDTEST_RECV_BUF_SIZE);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "start %s: %s", target->name, target->url);
    speedtest_log_signal(modem, target->name, "before");

    if (!speedtest_resolve_addr(modem, &url, &addr)) {
        goto cleanup;
    }
    (void)inet_ntop(AF_INET, &addr.sin_addr, ip_text, sizeof(ip_text));
    LISA_LOGI(LOG_TAG, "%s target resolved: host=%s, ip=%s, port=%u, path=%s",
              target->name, url.host, ip_text, url.port, url.path);

    sockfd = lisa_modem_socket_open_on(modem, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LISA_LOGE(LOG_TAG, "%s open socket failed, errno=%d", target->name, errno);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "%s socket opened: sockfd=%d", target->name, sockfd);

    if (!lisa_modem_socket_connect_on(modem, sockfd, (const struct sockaddr *)&addr, (int)sizeof(addr))) {
        LISA_LOGE(LOG_TAG, "%s connect failed: sockfd=%d, errno=%d", target->name, sockfd, errno);
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "%s connected: sockfd=%d", target->name, sockfd);

    if (url.port == 80) {
        (void)snprintf(host_header, sizeof(host_header), "%s", url.host);
    } else {
        (void)snprintf(host_header, sizeof(host_header), "%s:%u", url.host, url.port);
    }

    ret = snprintf(request,
                   sizeof(request),
                   "GET %s HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "User-Agent: arcs-4g-usb-speedtest/1.0\r\n"
                   "Accept: */*\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   url.path,
                   host_header);
    if (ret <= 0 || ret >= (int)sizeof(request)) {
        LISA_LOGE(LOG_TAG, "%s build HTTP request failed", target->name);
        ret = -1;
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "%s HTTP request built: %d bytes", target->name, ret);

    if (speedtest_send_all(modem, sockfd, request, (size_t)ret) != 0) {
        LISA_LOGE(LOG_TAG, "%s send HTTP request failed: sockfd=%d, errno=%d", target->name, sockfd, errno);
        ret = -1;
        goto cleanup;
    }
    LISA_LOGI(LOG_TAG, "%s HTTP request queued/sent: %d bytes", target->name, ret);

    memset(header_buf, 0, sizeof(header_buf));
    recv_window_ms = speedtest_now_ms();

    while (1) {
        uint32_t recv_start_ms = speedtest_now_ms();
        int recv_len = lisa_modem_socket_recv_on(modem, sockfd, recv_buf, SPEEDTEST_RECV_BUF_SIZE, SPEEDTEST_RECV_TIMEOUT_MS);
        uint32_t recv_done_ms = speedtest_now_ms();
        uint32_t recv_wait_ms = recv_done_ms - recv_start_ms;

        recv_window_calls++;
        recv_window_wait_ms += recv_wait_ms;
        if (recv_wait_ms > recv_window_max_wait_ms) {
            recv_window_max_wait_ms = recv_wait_ms;
        }

        if (recv_len < 0) {
            LISA_LOGE(LOG_TAG, "%s recv failed: ret=%d, errno=%d, total_body=%u, header_done=%d",
                      target->name,
                      recv_len,
                      errno,
                      total_body,
                      header_done ? 1 : 0);
            ret = -1;
            break;
        }
        if (recv_len == 0) {
            LISA_LOGI(LOG_TAG, "%s recv closed: total_body=%u, header_done=%d",
                      target->name,
                      total_body,
                      header_done ? 1 : 0);
            ret = 0;
            break;
        }
        recv_window_raw_bytes += (uint32_t)recv_len;

        if (!header_done) {
            size_t old_header_len = header_len;
            size_t copy_len = (size_t)recv_len;
            char *header_end;

            if (copy_len > sizeof(header_buf) - 1U - header_len) {
                copy_len = sizeof(header_buf) - 1U - header_len;
            }
            memcpy(header_buf + header_len, recv_buf, copy_len);
            header_len += copy_len;
            header_buf[header_len] = '\0';

            header_end = strstr(header_buf, "\r\n\r\n");
            if (!header_end) {
                if (copy_len < (size_t)recv_len || header_len >= sizeof(header_buf) - 1U) {
                    LISA_LOGE(LOG_TAG, "%s HTTP header too large", target->name);
                    ret = -1;
                    break;
                }
                continue;
            }

            header_done = true;
            http_status = speedtest_parse_http_status(header_buf);
            content_length = speedtest_parse_content_length(header_buf);
            LISA_LOGI(LOG_TAG, "%s HTTP status=%d, content_length=%d", target->name, http_status, content_length);
            if (http_status != 200) {
                LISA_LOGE(LOG_TAG, "%s unexpected HTTP status: %d", target->name, http_status);
                ret = -1;
                break;
            }

            size_t header_total = (size_t)(header_end - header_buf) + 4U;
            size_t header_bytes_from_current = header_total > old_header_len ? header_total - old_header_len : 0;
            if (header_bytes_from_current > (size_t)recv_len) {
                LISA_LOGE(LOG_TAG, "%s bad HTTP header offset", target->name);
                ret = -1;
                break;
            }

            ret = speedtest_count_body(target->name,
                                       recv_buf + header_bytes_from_current,
                                       (size_t)recv_len - header_bytes_from_current,
                                       content_length,
                                       &total_body,
                                       &body_start_ms);
            if (ret != 0) {
                ret = 0;
                break;
            }
            speedtest_log_recv_progress(target->name,
                                        recv_done_ms,
                                        &recv_window_ms,
                                        &recv_window_body_start,
                                        &recv_window_raw_bytes,
                                        &recv_window_calls,
                                        &recv_window_wait_ms,
                                        &recv_window_max_wait_ms,
                                        total_body,
                                        content_length,
                                        header_done);
            continue;
        }

        ret = speedtest_count_body(target->name,
                                   recv_buf,
                                   (size_t)recv_len,
                                   content_length,
                                   &total_body,
                                   &body_start_ms);
        if (ret != 0) {
            ret = 0;
            break;
        }
        speedtest_log_recv_progress(target->name,
                                    recv_done_ms,
                                    &recv_window_ms,
                                    &recv_window_body_start,
                                    &recv_window_raw_bytes,
                                    &recv_window_calls,
                                    &recv_window_wait_ms,
                                    &recv_window_max_wait_ms,
                                    total_body,
                                    content_length,
                                    header_done);
    }

    if (!header_done || total_body == 0) {
        LISA_LOGE(LOG_TAG, "%s no HTTP body received", target->name);
        ret = -1;
        goto cleanup;
    }

    if (content_length > 0 && total_body != (uint32_t)content_length) {
        LISA_LOGE(LOG_TAG, "%s size mismatch: pulled=%u, content_length=%d",
                  target->name,
                  total_body,
                  content_length);
        ret = -1;
        goto cleanup;
    }

    speedtest_log_result(target->name, total_body, speedtest_now_ms() - body_start_ms);
    ret = 0;

cleanup:
    if (sockfd >= 0) {
        int close_ret = lisa_modem_socket_close_on(modem, sockfd);

        LISA_LOGI(LOG_TAG, "%s socket closed: sockfd=%d, ret=%d", target->name, sockfd, close_ret);
    }
    speedtest_log_signal(modem, target->name, "after");
    if (recv_buf) {
        vPortFree(recv_buf);
    }
    return ret;
}

int main(int argc, char **argv)
{
    static const speedtest_target_t targets[] = {
        {"1M", SPEEDTEST_URL_1M},
        {"5M", SPEEDTEST_URL_5M},
        {"customer", SPEEDTEST_URL_CUSTOMER},
    };
    lisa_modem_t *modem;

    (void)argc;
    (void)argv;

    LISA_LOGI(LOG_TAG, "EC801E USB AT 4G HTTP downlink speed test");
    modem = modem_usb_at_open();
    if (!modem) {
        LISA_LOGE(LOG_TAG, "open USB AT modem failed");
        goto exit;
    }

    LISA_LOGI(LOG_TAG, "USB AT modem ready, waiting network settle");
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (uint32_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
        (void)speedtest_pull_one(modem, &targets[i]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    modem_usb_at_close(modem);
    LISA_LOGI(LOG_TAG, "speed test complete");

exit:
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
