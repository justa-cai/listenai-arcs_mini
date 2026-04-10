#ifndef MOCK_LWIP_H
#define MOCK_LWIP_H

#include "fff.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/dns.h"

DECLARE_FAKE_VALUE_FUNC(u16_t, lwip_standard_chksum, const void *, int);
DECLARE_FAKE_VALUE_FUNC(uint16_t, lwip_htons_impl, uint16_t);

void mock_lwip_reset(void);

typedef void (*tcpip_init_done_fn)(void *);

DECLARE_FAKE_VOID_FUNC(tcpip_init, tcpip_init_done_fn, void *);
DECLARE_FAKE_VALUE_FUNC(err_t, tcpip_input, struct pbuf *, struct netif *);

DECLARE_FAKE_VALUE_FUNC(struct pbuf *, pbuf_alloc, pbuf_layer, u16_t, pbuf_type);
DECLARE_FAKE_VALUE_FUNC(struct pbuf *, pbuf_alloc_wait, pbuf_layer, u16_t, pbuf_type);
DECLARE_FAKE_VALUE_FUNC(struct pbuf *, pbuf_alloced_custom, pbuf_layer, u16_t, pbuf_type,
                        struct pbuf_custom *, void *, u16_t);
DECLARE_FAKE_VALUE_FUNC(u8_t, pbuf_header, struct pbuf *, int16_t);
DECLARE_FAKE_VALUE_FUNC(u8_t, pbuf_free, struct pbuf *);
DECLARE_FAKE_VOID_FUNC(pbuf_ref, struct pbuf *);
DECLARE_FAKE_VOID_FUNC(pbuf_cat, struct pbuf *, struct pbuf *);

DECLARE_FAKE_VALUE_FUNC(struct netif *, netif_find, const char *);
DECLARE_FAKE_VALUE_FUNC(int, netif_is_up, struct netif *);
DECLARE_FAKE_VALUE_FUNC(int, netif_is_link_up, struct netif *);
DECLARE_FAKE_VALUE_FUNC(const ip4_addr_t *, netif_ip4_addr, struct netif *);
DECLARE_FAKE_VALUE_FUNC(const ip4_addr_t *, netif_ip4_netmask, struct netif *);
DECLARE_FAKE_VALUE_FUNC(const ip4_addr_t *, netif_ip4_gw, struct netif *);
DECLARE_FAKE_VOID_FUNC(netif_set_addr, struct netif *, const ip4_addr_t *, const ip4_addr_t *, const ip4_addr_t *);

DECLARE_FAKE_VALUE_FUNC(err_t, netifapi_netif_add, struct netif *, const ip4_addr_t *, const ip4_addr_t *,
                        const ip4_addr_t *, void *, netif_init_fn, netif_input_fn);
DECLARE_FAKE_VOID_FUNC(netifapi_netif_set_up, struct netif *);
DECLARE_FAKE_VOID_FUNC(netifapi_netif_set_down, struct netif *);
DECLARE_FAKE_VOID_FUNC(netifapi_netif_set_default, struct netif *);
DECLARE_FAKE_VALUE_FUNC(err_t, netifapi_dhcp_start, struct netif *);
DECLARE_FAKE_VOID_FUNC(netifapi_dhcp_stop, struct netif *);
DECLARE_FAKE_VALUE_FUNC(err_t, netifapi_dhcp_release, struct netif *);

DECLARE_FAKE_VALUE_FUNC(int, lwip_socket, int, int, int);
DECLARE_FAKE_VALUE_FUNC(int, lwip_getsockopt, int, int, int, void *, socklen_t *);
DECLARE_FAKE_VALUE_FUNC(int, lwip_close, int);

DECLARE_FAKE_VALUE_FUNC(int, socket, int, int, int);
DECLARE_FAKE_VALUE_FUNC(int, getsockopt, int, int, int, void *, socklen_t *);
DECLARE_FAKE_VALUE_FUNC(int, close, int);

DECLARE_FAKE_VALUE_FUNC(void *, memp_malloc, int);
DECLARE_FAKE_VALUE_FUNC(int, sys_mbox_trypost, int *, void *);

DECLARE_FAKE_VOID_FUNC(netbuf_delete, struct netbuf *);

DECLARE_FAKE_VOID_FUNC(dns_setserver, u8_t, const ip_addr_t *);
DECLARE_FAKE_VALUE_FUNC(const ip_addr_t *, dns_getserver, u8_t);

DECLARE_FAKE_VALUE_FUNC(int, dhcp_supplied_address, struct netif *);
DECLARE_FAKE_VOID_FUNC(dhcps_start, struct netif *);
DECLARE_FAKE_VOID_FUNC(dhcps_stop);

DECLARE_FAKE_VALUE_FUNC(err_t, etharp_output, struct netif *, struct pbuf *);

extern const struct eth_addr ethbroadcast;
extern const struct eth_addr ethzero;

#endif
