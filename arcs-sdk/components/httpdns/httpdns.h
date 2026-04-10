#ifndef _HTTPDNS_H_
#define _HTTPDNS_H_

#include <stdint.h>
#include <stddef.h>
#include "lwip/ip_addr.h"
#include "httpdns_cache.h"

#define HTTPDNS_REQ_MAXLEN  512
#define HTTPDNS_RSP_MAXLEN  2048
#define HTTPDNS_TIMEOUT_SEC 5

/**
 * @brief Simple HTTP GET request
 *
 * @param server_ip   server IP address string
 * @param port        server port
 * @param path        request path
 * @param rsp_buf     buffer to store response
 * @param rsp_buf_len size of rsp_buf
 * @param rsp_len     output actual response length
 * @return 0 on success, -1 on failure
 */
int httpdns_http_get(const char *server_ip, int port, const char *path,
                     char *rsp_buf, size_t rsp_buf_len, int *rsp_len);

/**
 * @brief Extract HTTP body from response
 *
 * @param response HTTP response string
 * @return pointer to body, or NULL if not found
 */
char *httpdns_http_get_body(char *response);

/**
 * @brief Get HTTPDNS server IP address (provider-specific)
 *
 * @return server IP address string
 */
const char *httpdns_get_server(void);

/**
 * @brief Resolve hostname using HTTPDNS (provider-specific)
 *
 * @param host hostname to resolve
 * @param addr output IP address
 * @return 0 on success, -1 on failure
 */
int httpdns_resolve(const char *host, ip_addr_t *addr);

#endif /* _HTTPDNS_H_ */
