#ifndef _HTTPDNS_CACHE_H_
#define _HTTPDNS_CACHE_H_

#include <stdint.h>
#include <stddef.h>
#include "lwip/ip_addr.h"

#define DNS_CACHE_SIZE      16
#define DNS_CACHE_TTL_SEC   600
#define DNS_HOST_MAXLEN     128

/**
 * @brief Lookup hostname in DNS cache
 *
 * @param host hostname to lookup
 * @param addr output IP address
 * @return 0 on cache hit, -1 on cache miss or expired
 */
int httpdns_cache_lookup(const char *host, ip_addr_t *addr);

/**
 * @brief Add hostname to DNS cache
 *
 * @param host hostname
 * @param addr IP address
 * @param ttl  time to live in seconds
 */
void httpdns_cache_add(const char *host, const ip_addr_t *addr, uint32_t ttl);

#endif /* _HTTPDNS_CACHE_H_ */
