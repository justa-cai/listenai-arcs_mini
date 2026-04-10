#ifndef LWIP_NETIFAPI_H
#define LWIP_NETIFAPI_H

#include "lwip/tcpip.h"

err_t netifapi_netif_add(struct netif *netif,
                         const ip4_addr_t *ipaddr,
                         const ip4_addr_t *netmask,
                         const ip4_addr_t *gw,
                         void *state,
                         netif_init_fn init,
                         netif_input_fn input);
void netifapi_netif_set_up(struct netif *netif);
void netifapi_netif_set_down(struct netif *netif);
void netifapi_netif_set_default(struct netif *netif);
err_t netifapi_dhcp_start(struct netif *netif);
void netifapi_dhcp_stop(struct netif *netif);
err_t netifapi_dhcp_release(struct netif *netif);

#endif
