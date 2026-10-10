/*
 * sip_osal_freertos.c - OSAL 的设备侧实现（FreeRTOS + lwip BSD socket）
 *
 * 只在本文件出现 lwip / FreeRTOS 头；core/*.c 依赖 sip_osal.h。
 * UDP socket 用法与 user_sntp.c / gamepad_udp.c 一致：
 * SO_REUSEADDR + SO_RCVTIMEO 当轮询节拍，停止时 closesocket 解除阻塞。
 */
#define TAG "sip"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/netif.h"

#include "lisa_log.h"

#include "sip_osal.h"

static int s_log_level = SIP_LOG_INFO;

void sip_osal_set_log_level(int level)
{
    s_log_level = level;
}

int sip_osal_udp_open(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return -1;
    }
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        closesocket(fd);
        return -1;
    }
    /* 超时值在每次 recv 前设置（见 sip_osal_udp_recv），这里只绑地址。 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(fd);
        return -1;
    }
    return fd;
}

int sip_osal_udp_send(int fd, const char *ip, uint16_t port,
                      const void *data, size_t len)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        return -1;
    }
    int sent = sendto(fd, data, len, 0,
                      (const struct sockaddr *)&addr, sizeof(addr));
    return sent;
}

int sip_osal_udp_recv(int fd, void *buf, size_t len, uint32_t timeout_ms,
                      char *from_ip, uint16_t *from_port)
{
    /* lwip 的 SO_RCVTIMEO=0 表示无限等待 —— timeout 0 的语义是"立即返回"，
     * 映射成 1ms（曾因整层干等把音频循环拖成 ~178ms/圈）。 */
    if (timeout_ms == 0) {
        timeout_ms = 1;
    }
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000u;
    tv.tv_usec = (timeout_ms % 1000u) * 1000u;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int n = recvfrom(fd, buf, len, 0, (struct sockaddr *)&from, &fromlen);
    if (n < 0) {
        return 0; /* lwip 超时返回 -1/EWOULDBLOCK：统一折叠为 0（超时） */
    }
    if (from_ip) {
        /* 单线程调用，inet_ntoa 的静态缓冲安全。 */
        const char *s = inet_ntoa(from.sin_addr);
        snprintf(from_ip, 16, "%s", s ? s : "?");
    }
    if (from_port) {
        *from_port = ntohs(from.sin_port);
    }
    return n;
}

void sip_osal_udp_close(int fd)
{
    if (fd >= 0) {
        closesocket(fd);
    }
}

uint32_t sip_osal_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

uint32_t sip_osal_rand(void)
{
    /* xorshift32；首次调用用 tick 与本静态量地址混合做种子。 */
    static uint32_t state;
    if (state == 0) {
        state = sip_osal_now_ms() ^ (uint32_t)(uintptr_t)&state;
        if (state == 0) {
            state = 0x9E3779B9u;
        }
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void sip_osal_sleep_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1u));
}

int sip_osal_get_local_ip(const char *peer_ip, char *out, size_t outlen)
{
    if (!peer_ip || !out || outlen < 8) {
        return -1;
    }
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));
    peer.sin_family = AF_INET;
    peer.sin_port = htons(5060);
    peer.sin_addr.s_addr = inet_addr(peer_ip);
    if (peer.sin_addr.s_addr == INADDR_NONE ||
        connect(fd, (const struct sockaddr *)&peer, sizeof(peer)) != 0) {
        closesocket(fd);
        return -1;
    }
    struct sockaddr_in local;
    socklen_t slen = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&local, &slen) != 0) {
        closesocket(fd);
        return -1;
    }
    closesocket(fd);
    const char *s = inet_ntoa(local.sin_addr);
    if (!s || !s[0]) {
        return -1;
    }
    snprintf(out, outlen, "%s", s);
    return 0;
}

void sip_osal_log(int level, const char *tag, const char *fmt, ...)
{
    if (level > s_log_level) {
        return;
    }
    char line[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    line[n] = '\0';
    switch (level) {
    case SIP_LOG_ERR:
        LISA_LOGE(TAG, "%s", line);
        break;
    case SIP_LOG_WARN:
        LISA_LOGW(TAG, "%s", line);
        break;
    default:
        LISA_LOGI(TAG, "%s", line);
        break;
    }
    (void)tag;
}
