#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "ml307_tcp.h"
#include "ml307_modem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lisa_log.h"
#include "ml307_udp.h"

#define TAG "lisa_4g"

bool lisa_4g_dns_resolve(const char *domain, char *ip_addr, size_t size)
{
    LISA_LOGI(TAG, "%s-----", __func__);

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

int lisa_4g_tcp_socket(bool is_ssl)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    return ml307_tcp_init(is_ssl);
}

void lisa_4g_tcp_deinit(int tcp_id)
{

}

int lisa_4g_tcp_connect_with_addr(int tcp_id, struct sockaddr* addr, int len)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    char host[128];           /* Host name or IP */
    int port;                 /* Port number */
    
    /* Extract host and port from sockaddr_in */
    struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
    port = ntohs(addr_in->sin_port);

    /* Convert IP address to string */
    inet_ntop(AF_INET, &addr_in->sin_addr, host, sizeof(host));
    LISA_LOGI(TAG, "lisa_4g_tcp_connect_with_addr %s:%d", host, port);

    return (ml307_tcp_connect(tcp_id, host, port, 0) == 1 ? 0 : -1);
}

bool lisa_4g_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    return ml307_tcp_connect(tcp_id, host, port, is_ssl);
}

int lisa_4g_tcp_closesocket(int tcp_id)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    return ml307_tcp_disconnect(tcp_id);
}

int lisa_4g_ioctlsocket(int sockfd)
{

    return 0;
}

int lisa_4g_tcp_send(int tcp_id, const char *data, size_t length, uint32_t timeout_ms)
{
    // LISA_LOGI(TAG, "%s-----", __func__);
    return ml307_tcp_send(tcp_id, data, length);
}

int lisa_4g_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms)
{
    // LISA_LOGI(TAG, "%s-----", __func__);
    int ret = ml307_tcp_recv(tcp_id, buffer, length, timeout_ms);

    return ret;
}

int lisa_4g_udp_socket(int domain, int type, int protocol)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    return ml307_udp_init();
}

int lisa_4g_setsockopt(int sockfd, int level, int optname, const void *optval, int optlen)
{

    return 0;
}

void lisa_4g_udp_deinit(int udp_id)
{

}

int lisa_4g_udp_closesocket(int udp_id)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    return ml307_udp_disconnect(udp_id);
}

int lisa_4g_udp_sendto(int udp_id, const char *data, size_t length, int flags,
                      const struct sockaddr *dest_addr, int addrlen)
{
    LISA_LOGI(TAG, "%s-----", __func__);
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

int lisa_4g_udp_recvform(int udp_id, char *buffer, size_t length, int flags,
                        struct sockaddr *src_addr, int *addrlen)
{
    LISA_LOGI(TAG, "%s-----", __func__);
    return  ml307_udp_recv(udp_id, buffer, length, 0);
}

//todo:可传入串口参数
bool lisa_4g_module_init(const char *uart_dev)
{
    return ml307_modem_init(uart_dev);
}

bool lisa_4g_module_deinit(void)
{
    return ml307_modem_deinit();
}
