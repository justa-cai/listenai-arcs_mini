/*
 * sip_osal_posix.c - OSAL 的 Ubuntu 主机侧实现（glibc）
 *
 * 与 sip_osal_freertos.c 行为一一对应；日志直接走 printf。
 * 编译进 src/middleware/sip/host/CMakeLists.txt 的 sip_peer。
 */
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

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
        close(fd);
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
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
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        return -1;
    }
    return (int)sendto(fd, data, len, 0,
                       (const struct sockaddr *)&addr, sizeof(addr));
}

int sip_osal_udp_recv(int fd, void *buf, size_t len, uint32_t timeout_ms,
                      char *from_ip, uint16_t *from_port)
{
    /* POSIX 的 SO_RCVTIMEO={0,0} 同样是无限等待；timeout 0 用
     * MSG_DONTWAIT 表达"立即返回"。 */
    int flags = 0;
    if (timeout_ms == 0) {
        flags = MSG_DONTWAIT;
    } else {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000u;
        tv.tv_usec = (timeout_ms % 1000u) * 1000u;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(fd, buf, len, flags, (struct sockaddr *)&from, &fromlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        return -1;
    }
    if (from_ip) {
        if (!inet_ntop(AF_INET, &from.sin_addr, from_ip, 16)) {
            from_ip[0] = '?';
            from_ip[1] = '\0';
        }
    }
    if (from_port) {
        *from_port = ntohs(from.sin_port);
    }
    return (int)n;
}

void sip_osal_udp_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

uint32_t sip_osal_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

uint32_t sip_osal_rand(void)
{
    static uint32_t state;
    if (state == 0) {
        state = (uint32_t)clock() ^ (uint32_t)(uintptr_t)&state;
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
    struct timespec ts;
    ts.tv_sec = ms / 1000u;
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
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
    if (inet_pton(AF_INET, peer_ip, &peer.sin_addr) != 1 ||
        connect(fd, (const struct sockaddr *)&peer, sizeof(peer)) != 0) {
        close(fd);
        return -1;
    }
    struct sockaddr_in local;
    socklen_t slen = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&local, &slen) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    if (!inet_ntop(AF_INET, &local.sin_addr, out, (socklen_t)outlen)) {
        return -1;
    }
    return 0;
}

void sip_osal_log(int level, const char *tag, const char *fmt, ...)
{
    if (level > s_log_level) {
        return;
    }
    static const char *names[] = {"E", "W", "I", "D"};
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][%s] ", names[level & 3], tag);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}
