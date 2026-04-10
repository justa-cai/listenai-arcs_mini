#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lisa_log.h"

#include "httpdns.h"

#define TAG "httpdns"

int httpdns_http_get(const char *server_ip, int port, const char *path,
                     char *rsp_buf, size_t rsp_buf_len, int *rsp_len)
{
    int sock = -1;
    int ret = -1;
    char *req_buf = NULL;
    struct sockaddr_in server_addr;
    struct timeval tv;
    int total_recv = 0;

    req_buf = malloc(HTTPDNS_REQ_MAXLEN);
    if (!req_buf) {
        LOGE("http_get malloc failed");
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("socket create failed");
        goto out;
    }

    tv.tv_sec = HTTPDNS_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE("connect to %s:%d failed", server_ip, port);
        goto out;
    }

    int req_len = snprintf(req_buf, HTTPDNS_REQ_MAXLEN,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, server_ip);

    if (send(sock, req_buf, req_len, 0) != req_len) {
        LOGE("send request failed");
        goto out;
    }

    total_recv = 0;
    while (total_recv < (int)rsp_buf_len - 1) {
        int n = recv(sock, rsp_buf + total_recv, rsp_buf_len - 1 - total_recv, 0);
        if (n <= 0) {
            break;
        }
        total_recv += n;
    }
    rsp_buf[total_recv] = '\0';

    if (total_recv <= 0) {
        LOGE("recv response failed");
        goto out;
    }

    *rsp_len = total_recv;
    ret = 0;

out:
    free(req_buf);
    if (sock >= 0) {
        close(sock);
    }
    return ret;
}

char *httpdns_http_get_body(char *response)
{
    char *body = strstr(response, "\r\n\r\n");
    if (body) {
        return body + 4;
    }
    return NULL;
}

int http_dns_resolve(const char *name, ip_addr_t *addr, u8_t addrtype, err_t *err)
{
    (void)addrtype;

    LOGI("httpdns hook called: name=%s", name ? name : "(null)");

    if (!name) {
        return 0;
    }

    if (ipaddr_aton(name, addr)) {
        *err = ERR_OK;
        return 1;
    }

    const char *server = httpdns_get_server();
    if (strcmp(name, server) == 0) {
        return 0;
    }

    LOGI("httpdns resolve: %s", name);

    if (httpdns_resolve(name, addr) == 0) {
        *err = ERR_OK;
        return 1;
    }

    LOGW("httpdns failed for %s, fallback to lwip dns", name);

    return 0;
}
