/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"

#include "net_probe_checks.h"
#include "net_probe.h"

#define ECM_PUBLIC_DNS_IP           "223.5.5.5"
#define ECM_DNS_PORT                53
#define ECM_NTP_PORT                123
#define ECM_HTTP_PORT               80
#define ECM_PROBE_DOMAIN            "www.baidu.com"
#define ECM_NTP_HOST                "ntp1.aliyun.com"
#define ECM_HTTP_HOST               "www.baidu.com"
#define ECM_PROBE_RETRY_DELAY_MS    5000
#define ECM_PROBE_TX_ID             0x1234

struct ec801e_probe_state {
    bool udp_dns_passed;
    bool udp_ntp_passed;
    bool tcp_connect_passed;
    bool http_get_passed;
};

static bool g_probe_task_started;

static size_t encode_dns_name(uint8_t *buf, size_t buf_size, const char *domain)
{
    size_t used = 0;
    const char *label = domain;
    const char *dot;

    while (*label != '\0') {
        size_t label_len;

        dot = strchr(label, '.');
        label_len = dot ? (size_t)(dot - label) : strlen(label);
        if (label_len == 0 || label_len > 63 || (used + label_len + 1) >= buf_size) {
            return 0;
        }

        buf[used++] = (uint8_t)label_len;
        memcpy(&buf[used], label, label_len);
        used += label_len;

        if (dot == NULL) {
            break;
        }
        label = dot + 1;
    }

    if (used >= buf_size) {
        return 0;
    }

    buf[used++] = 0x00;
    return used;
}

static void reset_probe_state(struct ec801e_probe_state *state)
{
    memset(state, 0, sizeof(*state));
}

static void log_probe_summary(const struct ec801e_probe_state *state)
{
    printf("[NET] Summary UDP_DNS=%s UDP_NTP=%s TCP_CONNECT=%s HTTP_GET=%s\r\n",
           state->udp_dns_passed ? "PASS" : "PENDING",
           state->udp_ntp_passed ? "PASS" : "PENDING",
           state->tcp_connect_passed ? "PASS" : "PENDING",
           state->http_get_passed ? "PASS" : "PENDING");
}

static void log_dns_servers(void)
{
    for (uint8_t i = 0; i < DNS_MAX_SERVERS; i++) {
        const ip_addr_t *dns_server = dns_getserver(i);

        if (dns_server != NULL && !ip_addr_isany(dns_server)) {
            printf("[NET] DNS[%u]       : %s\r\n", i, ipaddr_ntoa(dns_server));
        } else {
            printf("[NET] DNS[%u]       : <unset>\r\n", i);
        }
    }
}

static void log_netif_info(const struct netif *netif)
{
    printf("[NET] IPv4 Address : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
    printf("[NET] IPv4 Netmask : %s\r\n", ipaddr_ntoa(&netif->netmask));
    printf("[NET] IPv4 Gateway : %s\r\n", ipaddr_ntoa(&netif->gw));
    printf("[NET] MAC          : %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
           netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);
    log_dns_servers();
}

static int run_udp_dns_probe(const char *dns_ip)
{
    int sock = -1;
    int ret = -1;
    uint8_t query[64];
    uint8_t response[256];
    size_t query_len;
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    struct sockaddr_in server_addr;
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    memset(query, 0, sizeof(query));
    query[0] = (uint8_t)(ECM_PROBE_TX_ID >> 8);
    query[1] = (uint8_t)(ECM_PROBE_TX_ID & 0xff);
    query[2] = 0x01;
    query[5] = 0x01;

    query_len = 12;
    query_len += encode_dns_name(&query[query_len], sizeof(query) - query_len, ECM_PROBE_DOMAIN);
    if (query_len == 12 || (query_len + 4) > sizeof(query)) {
        printf("[NET] Failed to encode DNS query for %s\r\n", ECM_PROBE_DOMAIN);
        return -1;
    }

    query[query_len++] = 0x00;
    query[query_len++] = 0x01;
    query[query_len++] = 0x00;
    query[query_len++] = 0x01;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[NET] socket create failed: %d\r\n", sock);
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ECM_DNS_PORT);
    server_addr.sin_addr.s_addr = inet_addr(dns_ip);

    printf("[NET] Sending UDP DNS probe to %s:%d for %s ...\r\n",
           dns_ip, ECM_DNS_PORT, ECM_PROBE_DOMAIN);
    ret = sendto(sock, query, query_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("[NET] sendto failed: %d\r\n", ret);
        close(sock);
        return -1;
    }

    ret = recvfrom(sock, response, sizeof(response), 0, (struct sockaddr *)&from_addr, &from_len);
    if (ret > 0 && ec801e_validate_dns_response(response, (size_t)ret, ECM_PROBE_TX_ID) == 0) {
        printf("[NET] UDP DNS probe succeeded, received %d bytes from %s:%d\r\n",
               ret, inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
        close(sock);
        return 0;
    }

    printf("[NET] recvfrom failed or invalid DNS response: %d\r\n", ret);
    close(sock);
    return -1;
}

static int resolve_host_ipv4(const char *host, char *ip_buf, size_t ip_buf_size)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *it;
    int ret;

    if (host == NULL || ip_buf == NULL || ip_buf_size == 0) {
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(host, NULL, &hints, &result);
    if (ret != 0 || result == NULL) {
        printf("[NET] getaddrinfo failed for %s: %d\r\n", host, ret);
        return -1;
    }

    for (it = result; it != NULL; it = it->ai_next) {
        const struct sockaddr_in *addr = (const struct sockaddr_in *)it->ai_addr;
        const char *ip = inet_ntoa(addr->sin_addr);

        if (ip != NULL) {
            strncpy(ip_buf, ip, ip_buf_size - 1);
            ip_buf[ip_buf_size - 1] = '\0';
            freeaddrinfo(result);
            printf("[NET] Resolved %s -> %s\r\n", host, ip_buf);
            return 0;
        }
    }

    freeaddrinfo(result);
    return -1;
}

static int run_udp_ntp_probe(const char *ip)
{
    int sock;
    int ret;
    uint8_t request[48] = { 0 };
    uint8_t response[48] = { 0 };
    uint32_t unix_time = 0;
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[NET] NTP socket create failed: %d\r\n", sock);
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    request[0] = 0x1b;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ECM_NTP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    printf("[NET] Sending UDP NTP probe to %s:%u ...\r\n", ip, ECM_NTP_PORT);
    ret = sendto(sock, request, sizeof(request), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("[NET] NTP sendto failed: %d\r\n", ret);
        close(sock);
        return -1;
    }

    ret = recvfrom(sock, response, sizeof(response), 0, NULL, NULL);
    if (ret > 0 && ec801e_validate_ntp_response(response, (size_t)ret, &unix_time) == 0) {
        printf("[NET] UDP NTP probe succeeded, unix_time=%u\r\n", unix_time);
        close(sock);
        return 0;
    }

    printf("[NET] UDP NTP probe failed: %d\r\n", ret);
    close(sock);
    return -1;
}

static int run_tcp_connect_probe(const char *ip, uint16_t port)
{
    int sock;
    int ret;
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[NET] TCP socket create failed: %d\r\n", sock);
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    printf("[NET] Sending TCP connect probe to %s:%u ...\r\n", ip, port);
    ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == 0) {
        printf("[NET] TCP connect probe succeeded: %s:%u\r\n", ip, port);
        close(sock);
        return 0;
    }

    printf("[NET] TCP connect probe failed: %d\r\n", ret);
    close(sock);
    return -1;
}

static int run_http_get_probe(const char *host, const char *ip, uint16_t port)
{
    int sock;
    int ret;
    int status = 0;
    char request[192];
    char response[256];
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[NET] HTTP socket create failed: %d\r\n", sock);
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    printf("[NET] Sending HTTP GET probe to %s (%s:%u) ...\r\n", host, ip, port);
    ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret != 0) {
        printf("[NET] HTTP connect failed: %d\r\n", ret);
        close(sock);
        return -1;
    }

    ret = snprintf(request, sizeof(request),
                   "GET / HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "Connection: close\r\n"
                   "User-Agent: ec801e-ecm-probe/1.0\r\n"
                   "\r\n",
                   host);
    if (ret <= 0 || (size_t)ret >= sizeof(request)) {
        close(sock);
        return -1;
    }

    ret = send(sock, request, (size_t)ret, 0);
    if (ret < 0) {
        printf("[NET] HTTP send failed: %d\r\n", ret);
        close(sock);
        return -1;
    }

    ret = recv(sock, response, sizeof(response) - 1, 0);
    if (ret <= 0) {
        printf("[NET] HTTP recv failed: %d\r\n", ret);
        close(sock);
        return -1;
    }

    response[ret] = '\0';
    if (ec801e_parse_http_status(response, &status) == 0 &&
        (status == 200 || status == 301 || status == 302)) {
        printf("[NET] HTTP GET probe succeeded with status %d\r\n", status);
        close(sock);
        return 0;
    }

    printf("[NET] HTTP GET probe failed, invalid status line: %.32s\r\n", response);
    close(sock);
    return -1;
}

static int run_udp_dns_stage(void)
{
    bool have_dhcp_dns = false;

    for (uint8_t i = 0; i < DNS_MAX_SERVERS; i++) {
        const ip_addr_t *dns_server = dns_getserver(i);

        if (dns_server == NULL || ip_addr_isany(dns_server) || !IP_IS_V4(dns_server)) {
            continue;
        }

        have_dhcp_dns = true;
        if (run_udp_dns_probe(ipaddr_ntoa(dns_server)) == 0) {
            return 0;
        }
    }

    printf("[NET] %s, trying AliDNS fallback\r\n",
           have_dhcp_dns ? "DHCP-provided DNS servers did not answer"
                         : "No DHCP DNS server available");
    return run_udp_dns_probe(ECM_PUBLIC_DNS_IP);
}

static int run_udp_ntp_stage(void)
{
    char ntp_ip[16];

    if (resolve_host_ipv4(ECM_NTP_HOST, ntp_ip, sizeof(ntp_ip)) < 0) {
        return -1;
    }
    return run_udp_ntp_probe(ntp_ip);
}

static int run_tcp_connect_stage(void)
{
    char http_ip[16];

    if (resolve_host_ipv4(ECM_HTTP_HOST, http_ip, sizeof(http_ip)) < 0) {
        return -1;
    }
    return run_tcp_connect_probe(http_ip, ECM_HTTP_PORT);
}

static int run_http_get_stage(void)
{
    char http_ip[16];

    if (resolve_host_ipv4(ECM_HTTP_HOST, http_ip, sizeof(http_ip)) < 0) {
        return -1;
    }
    return run_http_get_probe(ECM_HTTP_HOST, http_ip, ECM_HTTP_PORT);
}

static void net_probe_task(void *arg)
{
    struct netif *netif;
    bool announced_waiting = false;
    bool link_ready = false;
    bool probe_ok = false;
    struct ec801e_probe_state state;

    (void)arg;
    reset_probe_state(&state);

    while (1) {
        netif = netif_default;

        if (netif == NULL || !netif_is_up(netif) || !dhcp_supplied_address(netif)) {
            if (!announced_waiting) {
                printf("[NET] Waiting for ECM netif and DHCP lease...\r\n");
                announced_waiting = true;
            }
            if (link_ready) {
                printf("[NET] ECM netif lost, waiting for reconnect...\r\n");
                link_ready = false;
                probe_ok = false;
                reset_probe_state(&state);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!link_ready) {
            printf("[NET] ECM netif is ready\r\n");
            log_netif_info(netif);
            announced_waiting = false;
            link_ready = true;
        }

        if (!state.udp_dns_passed) {
            state.udp_dns_passed = (run_udp_dns_stage() == 0);
            printf("[NET] UDP_DNS=%s\r\n", state.udp_dns_passed ? "PASS" : "FAIL");
            if (!state.udp_dns_passed) {
                printf("[NET] Probe failed, retrying in 5 seconds\r\n");
                vTaskDelay(pdMS_TO_TICKS(ECM_PROBE_RETRY_DELAY_MS));
                continue;
            }
            log_probe_summary(&state);
        }

        if (!state.udp_ntp_passed) {
            state.udp_ntp_passed = (run_udp_ntp_stage() == 0);
            printf("[NET] UDP_NTP=%s\r\n", state.udp_ntp_passed ? "PASS" : "FAIL");
            if (!state.udp_ntp_passed) {
                printf("[NET] Probe failed, retrying in 5 seconds\r\n");
                vTaskDelay(pdMS_TO_TICKS(ECM_PROBE_RETRY_DELAY_MS));
                continue;
            }
            log_probe_summary(&state);
        }

        if (!state.tcp_connect_passed) {
            state.tcp_connect_passed = (run_tcp_connect_stage() == 0);
            printf("[NET] TCP_CONNECT=%s\r\n", state.tcp_connect_passed ? "PASS" : "FAIL");
            if (!state.tcp_connect_passed) {
                printf("[NET] Probe failed, retrying in 5 seconds\r\n");
                vTaskDelay(pdMS_TO_TICKS(ECM_PROBE_RETRY_DELAY_MS));
                continue;
            }
            log_probe_summary(&state);
        }

        if (!state.http_get_passed) {
            state.http_get_passed = (run_http_get_stage() == 0);
            printf("[NET] HTTP_GET=%s\r\n", state.http_get_passed ? "PASS" : "FAIL");
            if (!state.http_get_passed) {
                printf("[NET] Probe failed, retrying in 5 seconds\r\n");
                vTaskDelay(pdMS_TO_TICKS(ECM_PROBE_RETRY_DELAY_MS));
                continue;
            }
            log_probe_summary(&state);
        }

        if (!probe_ok) {
            probe_ok = true;
            printf("[NET] ECM internet probe passed\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void ec801e_net_probe_start(void)
{
    if (g_probe_task_started) {
        return;
    }

    g_probe_task_started = true;
    xTaskCreate(net_probe_task, "ecm_probe", 4096, NULL, CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}
