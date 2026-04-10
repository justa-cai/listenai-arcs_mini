#ifndef LWIP_ETHARP_H
#define LWIP_ETHARP_H

#include <string.h>
#include "lwip/tcpip.h"

#define ETHARP_HWADDR_LEN 6
#define ETH_HWADDR_LEN ETHARP_HWADDR_LEN
#define SIZEOF_ETH_HDR 14
#define SIZEOF_ETHARP_HDR 28
#define ETHTYPE_IP 0x0800
#define ETHTYPE_ARP 0x0806
#define ARP_REQUEST 1

struct eth_addr {
    uint8_t addr[ETHARP_HWADDR_LEN];
};

struct eth_hdr {
    struct eth_addr dest;
    struct eth_addr src;
    uint16_t type;
};

struct etharp_hdr {
    uint16_t hwtype;
    uint16_t proto;
    uint8_t hwlen;
    uint8_t protolen;
    uint16_t opcode;
    struct eth_addr shwaddr;
    struct eth_addr dhwaddr;
    ip4_addr_t sipaddr;
    ip4_addr_t dipaddr;
};

struct etharp_stats {
    int xmit;
};

extern struct etharp_stats etharp;

extern const struct eth_addr ethbroadcast;
extern const struct eth_addr ethzero;

#define SMEMCPY memcpy
#define IPADDR_WORDALIGNED_COPY_FROM_IP4_ADDR_T(dst, src) memcpy((dst), (src), sizeof(ip4_addr_t))
#define PP_HTONS(x) lwip_htons(x)
#define ETHARP_STATS_INC(x) do { (void)(x); } while (0)

err_t etharp_output(struct netif *netif, struct pbuf *p);

#endif
