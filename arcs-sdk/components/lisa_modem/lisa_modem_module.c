#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "ml307_tcp.h"
#include "ml307_modem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lisa_log.h"
#include "ml307_udp.h"
#include "netdev.h"

#define TAG "lisa_modem"

bool lisa_modem_dns_resolve(const char *domain, char *ip_addr, size_t size)
{
    LISA_LOGI(TAG, "%s---", __func__);

    const int max_retries = 3;
    for (int retry = 0; retry < max_retries; retry++) {
        if (retry > 0) {
            LISA_LOGW(TAG, "DNS resolve retry %d/%d for domain: %s", retry, max_retries - 1, domain);
        }

        if (ml307_dns_resolve(domain, ip_addr, size)) {
            if (retry > 0) {
                LISA_LOGI(TAG, "DNS resolve succeeded on retry %d", retry);
            }
            return true;
        }

        // if (retry < max_retries - 1) {
        //     vTaskDelay(pdMS_TO_TICKS(1000));
        // }
    }

    LISA_LOGE(TAG, "DNS resolve failed after %d attempts for domain: %s", max_retries, domain);
    return false;
}

int lisa_modem_tcp_socket(bool is_ssl)
{
    LISA_LOGI(TAG, "%s---", __func__);
    return ml307_tcp_init(is_ssl);
}

void lisa_modem_tcp_deinit(int tcp_id)
{

}

int lisa_modem_tcp_connect_with_addr(int tcp_id, struct sockaddr* addr, int len)
{
    // LISA_LOGI(TAG, "%s---", __func__);
    char host[128];           /* Host name or IP */
    int port;                 /* Port number */
    
    /* Extract host and port from sockaddr_in */
    struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
    port = ntohs(addr_in->sin_port);

    /* Convert IP address to string */
    inet_ntop(AF_INET, &addr_in->sin_addr, host, sizeof(host));
    LISA_LOGI(TAG, "lisa_modem_tcp_connect_with_addr %s:%d", host, port);

    return (ml307_tcp_connect(tcp_id, host, port, 0) == 1 ? 0 : -1);
}

bool lisa_modem_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl)
{
    LISA_LOGI(TAG, "%s---", __func__);
    return ml307_tcp_connect(tcp_id, host, port, is_ssl);
}

int lisa_modem_tcp_closesocket(int tcp_id)
{
    LISA_LOGI(TAG, "%s---", __func__);
    return ml307_tcp_disconnect(tcp_id);
}

int lisa_modem_ioctlsocket(int sockfd, long cmd, void *arg)
{
    LISA_LOGI(TAG, "%s: sockfd=%d, cmd=%ld", __func__, sockfd, cmd);

    /* Handle FIONBIO (set non-blocking mode) */
    if (cmd == FIONBIO) {
        int *nonblocking = (int *)arg;
        if (!nonblocking) {
            LISA_LOGE(TAG, "Invalid argument for FIONBIO");
            return -1;
        }

        //1:阻塞， 0：非阻塞
        bool blocking = (*nonblocking == 0);
        LISA_LOGI(TAG, "Setting %s mode", blocking ? "blocking" : "non-blocking");

        /* Try setting for both TCP and UDP (one will fail, that's ok) */
        ml307_tcp_set_blocking(sockfd, blocking);
        ml307_udp_set_blocking(sockfd, blocking);

        return 0;
    }

    /* Handle FIONREAD (get number of bytes available) */
    if (cmd == FIONREAD) {
        /* Not implemented for 4G module */
        LISA_LOGW(TAG, "FIONREAD not supported");
        return -1;
    }

    LISA_LOGW(TAG, "Unsupported ioctl command: %ld", cmd);
    return -1;
}

int lisa_modem_tcp_send(int tcp_id, const char *data, size_t length, uint32_t timeout_ms)
{
    // LISA_LOGI(TAG, "%s---", __func__);
    return ml307_tcp_send(tcp_id, data, length);
}

int lisa_modem_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms)
{
    // LISA_LOGI(TAG, "%s---", __func__);
    int ret = ml307_tcp_recv(tcp_id, buffer, length, timeout_ms);

    return ret;
}

int lisa_modem_udp_socket(int domain, int type, int protocol)
{
    LISA_LOGI(TAG, "%s---", __func__);
    return ml307_udp_init();
}

int lisa_modem_setsockopt(int sockfd, int level, int optname, const void *optval, int optlen)
{
    // LISA_LOGI(TAG, "%s: sockfd=%d, level=%d, optname=%d", __func__, sockfd, level, optname);

    if (!optval || optlen <= 0) {
        LISA_LOGE(TAG, "Invalid optval or optlen");
        return -1;
    }

    /* Handle SOL_SOCKET level options */
    if (level == SOL_SOCKET) {
        switch (optname) {
            case SO_RCVTIMEO: {
                /* Receive timeout */
                struct timeval *tv = (struct timeval *)optval;
                if (optlen < sizeof(struct timeval)) {
                    LISA_LOGE(TAG, "Invalid optlen for SO_RCVTIMEO");
                    return -1;
                }

                uint32_t timeout_ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;
                // LISA_LOGI(TAG, "Setting SO_RCVTIMEO: %u ms", timeout_ms);

                /* Set timeout for both TCP and UDP (try both, one will fail) */
                ml307_tcp_set_recv_timeout(sockfd, timeout_ms);
                ml307_udp_set_recv_timeout(sockfd, timeout_ms);
                return 0;
            }

            case SO_SNDTIMEO: {
                /* Send timeout */
                struct timeval *tv = (struct timeval *)optval;
                if (optlen < sizeof(struct timeval)) {
                    LISA_LOGE(TAG, "Invalid optlen for SO_SNDTIMEO");
                    return -1;
                }

                uint32_t timeout_ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;
                // LISA_LOGI(TAG, "Setting SO_SNDTIMEO: %u ms", timeout_ms);

                /* Set timeout for both TCP and UDP (try both, one will fail) */
                ml307_tcp_set_send_timeout(sockfd, timeout_ms);
                ml307_udp_set_send_timeout(sockfd, timeout_ms);
                return 0;
            }

            case SO_REUSEADDR:
                /* 4G module doesn't support SO_REUSEADDR, but we can ignore it */
                // LISA_LOGI(TAG, "SO_REUSEADDR not supported, ignoring");
                return 0;

            case SO_KEEPALIVE:
                /* 4G module doesn't support SO_KEEPALIVE, but we can ignore it */
                // LISA_LOGI(TAG, "SO_KEEPALIVE not supported, ignoring");
                return 0;

            case SO_SSL_CONFIG: {
                /* SSL configuration for 4G TCP connections */
                if (optlen < sizeof(bool)) {
                    LISA_LOGE(TAG, "Invalid optlen for SO_SSL_CONFIG");
                    return -1;
                }
                bool ssl_use = *(bool *)optval;
                return ml307_tcp_set_ssl(sockfd, ssl_use);
            }

            default:
                LISA_LOGW(TAG, "Unsupported SOL_SOCKET option: %d", optname);
                return -1;
        }
    }

    /* Handle IPPROTO_TCP level options */
    if (level == IPPROTO_TCP) {
        switch (optname) {
            case TCP_NODELAY:
                /* 4G module doesn't support TCP_NODELAY, but we can ignore it */
                LISA_LOGI(TAG, "TCP_NODELAY not supported, ignoring");
                return 0;

            default:
                LISA_LOGW(TAG, "Unsupported IPPROTO_TCP option: %d", optname);
                return -1;
        }
    }

    LISA_LOGW(TAG, "Unsupported socket level: %d", level);
    return -1;
}

void lisa_modem_udp_deinit(int udp_id)
{

}

int lisa_modem_udp_closesocket(int udp_id)
{
    LISA_LOGI(TAG, "%s---", __func__);
    return ml307_udp_disconnect(udp_id);
}

int lisa_modem_udp_sendto(int udp_id, const char *data, size_t length, int flags,
                      const struct sockaddr *dest_addr, int addrlen)
{
    // LISA_LOGI(TAG, "%s---", __func__);
    /* Extract destination host and port */
    struct sockaddr_in *addr_in = (struct sockaddr_in *)dest_addr;
    int port = ntohs(addr_in->sin_port);
    char host[64];
    inet_ntop(AF_INET, &addr_in->sin_addr, host, sizeof(host));

    /* Connect to the destination (UDP "connection") */
    if (!ml307_udp_connect(udp_id, host, port)) {
        LISA_LOGI(TAG, "ML307 UDP connect failed: %s:%d", host, port);
        return -1;
    }

    return ml307_udp_send(udp_id, data, length);
}

int lisa_modem_udp_recvform(int udp_id, char *buffer, size_t length, int flags,
                        struct sockaddr *src_addr, int *addrlen)
{
    // LISA_LOGI(TAG, "%s---", __func__);
    return  ml307_udp_recv(udp_id, buffer, length, 0);
}

bool lisa_modem_module_init(const char *uart_dev)
{
    bool result;

    /* Initialize modem */
    result = ml307_modem_init(uart_dev);
    if (!result)
    {
        LISA_LOGE(TAG, "Failed to initialize modem");
        return false;
    }

    /* Register network device with priority 100 (lower priority than WiFi) */
    if (app_netdev_register("ml307", 500) != 0)
    {
        LISA_LOGE(TAG, "Failed to register 4G network device");
        ml307_modem_deinit();
        return false;
    }

    LISA_LOGI(TAG, "4G module initialized successfully");
    return true;
}

bool lisa_modem_module_deinit(void)
{
    /* Unregister network device first */
    netdev_unregister_by_name("ml307");

    /* Deinitialize modem */
    return ml307_modem_deinit();
}
