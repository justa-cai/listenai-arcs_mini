#ifndef LWIP_DNS_H
#define LWIP_DNS_H

#include "lwip/tcpip.h"

void dns_setserver(u8_t numdns, const ip_addr_t *dnsserver);
const ip_addr_t *dns_getserver(u8_t numdns);

static inline void ip_addr_set_ip4_u32(ip_addr_t *ipaddr, uint32_t val)
{
    ipaddr->addr = val;
}

static inline uint32_t ip_addr_get_ip4_u32(const ip_addr_t *ipaddr)
{
    return ipaddr->addr;
}

#endif
