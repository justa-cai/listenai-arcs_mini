/**
 * @file lisa_modem_module.h
 * @brief Lisa Modem Module Network Interface
 */

#ifndef LISA_MODEM_MODULE_H
#define LISA_MODEM_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "at_client.h"
#include "lisa_modem_status.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct modem_driver_ops;

typedef struct lisa_modem lisa_modem_t;

/* ===== Lifecycle ===== */

lisa_modem_t *lisa_modem_create(at_client_t *client, const char *name);
lisa_modem_t *lisa_modem_create_with_driver(at_client_t *client, const char *name,
                                            const struct modem_driver_ops *ops);
lisa_modem_t *lisa_modem_create_with_transport(at_transport_t *transport, bool owns_transport);
lisa_modem_t *lisa_modem_create_with_transport_and_driver(at_transport_t *transport, bool owns_transport,
                                                          const struct modem_driver_ops *ops);
lisa_modem_t *lisa_modem_create_uart(const char *uart_dev, uint32_t baudrate);
lisa_modem_t *lisa_modem_create_uart_with_driver(const char *uart_dev, uint32_t baudrate,
                                                 const struct modem_driver_ops *ops);
void lisa_modem_destroy(lisa_modem_t *modem);

/* ===== Accessors ===== */

lisa_modem_t *lisa_modem_get_default(void);
at_client_t *lisa_modem_get_client(lisa_modem_t *modem);
bool lisa_modem_is_present(void);
bool lisa_modem_get_status_on(lisa_modem_t *modem, lisa_modem_status_t *status);
bool lisa_modem_get_status(lisa_modem_status_t *status);

/* ===== Instance APIs ===== */

bool lisa_modem_dns_resolve_on(lisa_modem_t *modem, const char *domain, char *ip_addr, size_t size);
bool lisa_modem_get_imei_on(lisa_modem_t *modem, char *imei, size_t size);
bool lisa_modem_get_iccid_on(lisa_modem_t *modem, char *iccid, size_t size);
bool lisa_modem_get_signal_quality_on(lisa_modem_t *modem, int *rssi, int *ber);
bool lisa_modem_start_gnss_on(lisa_modem_t *modem);
bool lisa_modem_get_gps_location_on(lisa_modem_t *modem, double *lat, double *lon);

int  lisa_modem_socket_open_on(lisa_modem_t *modem, int domain, int type, int protocol);
bool lisa_modem_socket_connect_on(lisa_modem_t *modem, int sockfd, const struct sockaddr *addr, int addrlen);
int  lisa_modem_socket_close_on(lisa_modem_t *modem, int sockfd);
int  lisa_modem_socket_send_on(lisa_modem_t *modem, int sockfd, const void *data, size_t length,
                               uint32_t timeout_ms);
int  lisa_modem_socket_sendto_on(lisa_modem_t *modem, int sockfd, const void *data, size_t length, int flags,
                                 const struct sockaddr *dest_addr, int addrlen, uint32_t timeout_ms);
int  lisa_modem_socket_recv_on(lisa_modem_t *modem, int sockfd, void *buffer, size_t length,
                               uint32_t timeout_ms);
int  lisa_modem_socket_recvfrom_on(lisa_modem_t *modem, int sockfd, void *buffer, size_t length, int flags,
                                   struct sockaddr *src_addr, int *addrlen, uint32_t timeout_ms);

int lisa_modem_setsockopt_on(lisa_modem_t *modem, int sockfd, int level, int optname,
                             const void *optval, int optlen);
int lisa_modem_ioctlsocket_on(lisa_modem_t *modem, int sockfd, long cmd, void *arg);
int lisa_modem_getpeername_on(lisa_modem_t *modem, int sockfd, struct sockaddr *addr, int *addrlen);

/* ===== Default Modem Compatibility APIs ===== */

bool lisa_modem_dns_resolve(const char *domain, char *ip_addr, size_t size);
bool lisa_modem_get_imei(char *imei, size_t size);
bool lisa_modem_get_iccid(char *iccid, size_t size);
bool lisa_modem_get_signal_quality(int *rssi, int *ber);
bool lisa_modem_start_gnss(void);
bool lisa_modem_get_gps_location(double *lat, double *lon);

int  lisa_modem_socket_open(int domain, int type, int protocol);
bool lisa_modem_socket_connect(int sockfd, const struct sockaddr *addr, int addrlen);
int  lisa_modem_socket_close(int sockfd);
int  lisa_modem_socket_send(int sockfd, const void *data, size_t length, uint32_t timeout_ms);
int  lisa_modem_socket_sendto(int sockfd, const void *data, size_t length, int flags,
                              const struct sockaddr *dest_addr, int addrlen, uint32_t timeout_ms);
int  lisa_modem_socket_recv(int sockfd, void *buffer, size_t length, uint32_t timeout_ms);
int  lisa_modem_socket_recvfrom(int sockfd, void *buffer, size_t length, int flags,
                                struct sockaddr *src_addr, int *addrlen, uint32_t timeout_ms);

int lisa_modem_setsockopt(int sockfd, int level, int optname, const void *optval, int optlen);
int lisa_modem_ioctlsocket(int sockfd, long cmd, void *arg);
int lisa_modem_getpeername(int sockfd, struct sockaddr *addr, int *addrlen);

/* ===== Legacy Compatibility APIs ===== */

int  lisa_modem_tcp_socket(bool is_ssl);
void lisa_modem_tcp_deinit(int tcp_id);
bool lisa_modem_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl);
int  lisa_modem_tcp_connect_with_addr(int tcp_id, struct sockaddr *addr, int len);
int  lisa_modem_tcp_closesocket(int tcp_id);
int  lisa_modem_tcp_send(int tcp_id, const char *data, size_t length, uint32_t timeout_ms);
int  lisa_modem_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms);

int  lisa_modem_udp_socket(int domain, int type, int protocol);
void lisa_modem_udp_deinit(int udp_id);
int  lisa_modem_udp_closesocket(int udp_id);
int  lisa_modem_udp_sendto(int udp_id, const char *data, size_t length, int flags,
                           const struct sockaddr *dest_addr, int addrlen);
int  lisa_modem_udp_recvform(int udp_id, char *buffer, size_t length, int flags,
                             struct sockaddr *src_addr, int *addrlen);

/* ===== Legacy Init ===== */

bool lisa_modem_module_init(const char *uart_dev);
bool lisa_modem_module_deinit(void);

#ifdef LISA_MODEM_TEST
uint32_t lisa_modem_test_socket_generation(lisa_modem_t *modem, int sockfd);
bool lisa_modem_test_socket_generation_matches(lisa_modem_t *modem, int sockfd, uint32_t generation);
bool lisa_modem_test_is_in_dispatcher_control(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LISA_MODEM_MODULE_H */
