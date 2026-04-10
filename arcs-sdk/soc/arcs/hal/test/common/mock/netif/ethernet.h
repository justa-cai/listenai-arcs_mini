#ifndef NETIF_ETHERNET_H
#define NETIF_ETHERNET_H

#include "lwip/tcpip.h"
#include "lwip/etharp.h"

err_t ethernet_output(struct netif *netif, struct pbuf *p, const struct eth_addr *src,
                      const struct eth_addr *dst, uint16_t ethertype);

#endif
