/**
 * @file lisa_4g_module.h
 * @brief Lisa 4G Module Network Interface Header
 *
 * This file provides the API interface for 4G module network operations,
 * including TCP/UDP socket management, DNS resolution, and data transmission.
 * It encapsulates the ML307 4G module functionality.
 */

#ifndef LISA_4G_MODULE_H
#define LISA_4G_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Network structures from lwip */
struct sockaddr;

/**
 * @brief Resolve domain name to IP address using 4G module
 *
 * @param domain    Domain name to resolve (e.g., "www.example.com")
 * @param ip_addr   Buffer to store resolved IP address string
 * @param size      Size of the ip_addr buffer
 *
 * @return 0 on success, negative value on error
 */
bool lisa_4g_dns_resolve(const char *domain, char *ip_addr, size_t size);

/**
 * @brief Initialize a TCP socket on 4G module
 *
 * @param is_ssl    true to enable SSL/TLS encryption, false for plain TCP
 *
 * @return TCP socket ID (>=0) on success, negative value on error
 */
int lisa_4g_tcp_socket(bool is_ssl);

/**
 * @brief Deinitialize and close a TCP socket
 *
 * @param tcp_id    TCP socket ID returned by lisa_4g_tcp_socket()
 */
void lisa_4g_tcp_deinit(int tcp_id);

/**
 * @brief Connect to a remote TCP server via 4G module
 *
 * @param tcp_id    TCP socket ID
 * @param host      Remote server hostname or IP address
 * @param port      Remote server port number
 * @param is_ssl    true for SSL/TLS connection, false for plain TCP
 *
 * @return true on successful connection, false on failure
 */
bool lisa_4g_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl);

int lisa_4g_tcp_connect_with_addr(int tcp_id, struct sockaddr* addr, int len);

/**
 * @brief Disconnect TCP connection
 *
 * @param tcp_id    TCP socket ID to disconnect
 *
 * @return 0 on success, negative value on error
 */
int lisa_4g_tcp_closesocket(int tcp_id);

/**
 * @brief Control socket I/O mode (e.g., blocking/non-blocking)
 *
 * @param sockfd    Socket file descriptor
 *
 * @return 0 on success, negative value on error
 *
 * @note Currently not implemented, always returns 0
 */
int lisa_4g_ioctlsocket(int sockfd);

/**
 * @brief Send data over TCP connection
 *
 * @param tcp_id        TCP socket ID
 * @param data          Pointer to data buffer to send
 * @param length        Number of bytes to send
 * @param timeout_ms    Send timeout in milliseconds
 *
 * @return Number of bytes sent on success, negative value on error
 */
int lisa_4g_tcp_send(int tcp_id, const char *data, size_t length, uint32_t timeout_ms);

/**
 * @brief Receive data from TCP connection
 *
 * @param tcp_id        TCP socket ID
 * @param buffer        Buffer to store received data
 * @param length        Maximum number of bytes to receive
 * @param timeout_ms    Receive timeout in milliseconds
 *
 * @return Number of bytes received on success, 0 if connection closed, negative on error
 */
int lisa_4g_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms);

/**
 * @brief Create a UDP socket on 4G module
 *
 * @param domain    Socket domain (e.g., AF_INET)
 * @param type      Socket type (e.g., SOCK_DGRAM)
 * @param protocol  Protocol number (usually 0)
 *
 * @return UDP socket ID (>=0) on success, negative value on error
 */
int lisa_4g_udp_socket(int domain, int type, int protocol);

/**
 * @brief Set socket options
 *
 * @param sockfd    Socket file descriptor
 * @param level     Protocol level (e.g., SOL_SOCKET)
 * @param optname   Option name to set
 * @param optval    Pointer to option value
 * @param optlen    Size of option value
 *
 * @return 0 on success, negative value on error
 *
 * @note Currently not implemented, always returns 0
 */
int lisa_4g_setsockopt(int sockfd, int level, int optname, const void *optval, int optlen);

/**
 * @brief Deinitialize and close a UDP socket
 *
 * @param udp_id    UDP socket ID returned by lisa_4g_udp_socket()
 */
void lisa_4g_udp_deinit(int udp_id);

/**
 * @brief Disconnect UDP socket
 *
 * @param udp_id    UDP socket ID to disconnect
 *
 * @return 0 on success, negative value on error
 */
int lisa_4g_udp_closesocket(int udp_id);

/**
 * @brief Send data to a specific destination via UDP
 *
 * @param udp_id        UDP socket ID
 * @param data          Pointer to data buffer to send
 * @param length        Number of bytes to send
 * @param flags         Send flags (usually 0)
 * @param dest_addr     Destination address structure
 * @param addrlen       Size of destination address structure
 *
 * @return Number of bytes sent on success, negative value on error
 */
int lisa_4g_udp_sendto(int udp_id, const char *data, size_t length, int flags,
                       const struct sockaddr *dest_addr, int addrlen);

/**
 * @brief Receive data from UDP socket and get source address
 *
 * @param udp_id        UDP socket ID
 * @param buffer        Buffer to store received data
 * @param length        Maximum number of bytes to receive
 * @param flags         Receive flags (usually 0)
 * @param src_addr      Pointer to store source address (can be NULL)
 * @param addrlen       Pointer to address length (can be NULL)
 *
 * @return Number of bytes received on success, negative value on error
 */
int lisa_4g_udp_recvform(int udp_id, char *buffer, size_t length, int flags,
                         struct sockaddr *src_addr, int *addrlen);

/**
 * @brief Initialize the 4G module
 *
 * @return true on successful initialization, false on failure
 */
bool lisa_4g_module_init(const char *uart_dev);

/**
 * @brief Deinitialize the 4G module
 *
 * @return true on successful deinitialization, false on failure
 */
bool lisa_4g_module_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* LISA_4G_MODULE_H */
