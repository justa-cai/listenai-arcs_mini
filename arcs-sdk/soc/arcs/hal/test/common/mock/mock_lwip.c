#include "mock_lwip.h"

const struct eth_addr ethbroadcast = { .addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
const struct eth_addr ethzero = { .addr = {0, 0, 0, 0, 0, 0} };
struct etharp_stats etharp = {0};

DEFINE_FAKE_VALUE_FUNC(u16_t, lwip_standard_chksum, const void *, int);
DEFINE_FAKE_VALUE_FUNC(uint16_t, lwip_htons_impl, uint16_t);

DEFINE_FAKE_VOID_FUNC(tcpip_init, tcpip_init_done_fn, void *);
DEFINE_FAKE_VALUE_FUNC(err_t, tcpip_input, struct pbuf *, struct netif *);

DEFINE_FAKE_VALUE_FUNC(struct pbuf *, pbuf_alloc, pbuf_layer, u16_t, pbuf_type);
DEFINE_FAKE_VALUE_FUNC(struct pbuf *, pbuf_alloc_wait, pbuf_layer, u16_t, pbuf_type);
DEFINE_FAKE_VALUE_FUNC(struct pbuf *, pbuf_alloced_custom, pbuf_layer, u16_t, pbuf_type,
                       struct pbuf_custom *, void *, u16_t);
DEFINE_FAKE_VALUE_FUNC(u8_t, pbuf_header, struct pbuf *, int16_t);
DEFINE_FAKE_VALUE_FUNC(u8_t, pbuf_free, struct pbuf *);
DEFINE_FAKE_VOID_FUNC(pbuf_ref, struct pbuf *);
DEFINE_FAKE_VOID_FUNC(pbuf_cat, struct pbuf *, struct pbuf *);

DEFINE_FAKE_VALUE_FUNC(struct netif *, netif_find, const char *);
DEFINE_FAKE_VALUE_FUNC(int, netif_is_up, struct netif *);
DEFINE_FAKE_VALUE_FUNC(int, netif_is_link_up, struct netif *);
DEFINE_FAKE_VALUE_FUNC(const ip4_addr_t *, netif_ip4_addr, struct netif *);
DEFINE_FAKE_VALUE_FUNC(const ip4_addr_t *, netif_ip4_netmask, struct netif *);
DEFINE_FAKE_VALUE_FUNC(const ip4_addr_t *, netif_ip4_gw, struct netif *);
DEFINE_FAKE_VOID_FUNC(netif_set_addr, struct netif *, const ip4_addr_t *, const ip4_addr_t *, const ip4_addr_t *);

DEFINE_FAKE_VALUE_FUNC(err_t, netifapi_netif_add, struct netif *, const ip4_addr_t *, const ip4_addr_t *,
                       const ip4_addr_t *, void *, netif_init_fn, netif_input_fn);
DEFINE_FAKE_VOID_FUNC(netifapi_netif_set_up, struct netif *);
DEFINE_FAKE_VOID_FUNC(netifapi_netif_set_down, struct netif *);
DEFINE_FAKE_VOID_FUNC(netifapi_netif_set_default, struct netif *);
DEFINE_FAKE_VALUE_FUNC(err_t, netifapi_dhcp_start, struct netif *);
DEFINE_FAKE_VOID_FUNC(netifapi_dhcp_stop, struct netif *);
DEFINE_FAKE_VALUE_FUNC(err_t, netifapi_dhcp_release, struct netif *);

DEFINE_FAKE_VALUE_FUNC(int, lwip_socket, int, int, int);
DEFINE_FAKE_VALUE_FUNC(int, lwip_getsockopt, int, int, int, void *, socklen_t *);
DEFINE_FAKE_VALUE_FUNC(int, lwip_close, int);

DEFINE_FAKE_VALUE_FUNC(int, socket, int, int, int);
DEFINE_FAKE_VALUE_FUNC(int, getsockopt, int, int, int, void *, socklen_t *);
DEFINE_FAKE_VALUE_FUNC(int, close, int);

DEFINE_FAKE_VALUE_FUNC(void *, memp_malloc, int);
DEFINE_FAKE_VALUE_FUNC(int, sys_mbox_trypost, int *, void *);

DEFINE_FAKE_VOID_FUNC(netbuf_delete, struct netbuf *);

DEFINE_FAKE_VOID_FUNC(dns_setserver, u8_t, const ip_addr_t *);
DEFINE_FAKE_VALUE_FUNC(const ip_addr_t *, dns_getserver, u8_t);

DEFINE_FAKE_VALUE_FUNC(int, dhcp_supplied_address, struct netif *);
DEFINE_FAKE_VOID_FUNC(dhcps_start, struct netif *);
DEFINE_FAKE_VOID_FUNC(dhcps_stop);

DEFINE_FAKE_VALUE_FUNC(err_t, etharp_output, struct netif *, struct pbuf *);

void mock_lwip_reset(void)
{
    RESET_FAKE(lwip_socket);
    RESET_FAKE(lwip_getsockopt);
    RESET_FAKE(lwip_close);
    RESET_FAKE(socket);
    RESET_FAKE(getsockopt);
    RESET_FAKE(close);
    RESET_FAKE(lwip_standard_chksum);
    RESET_FAKE(lwip_htons_impl);
    RESET_FAKE(netif_is_up);
    RESET_FAKE(netif_is_link_up);
    RESET_FAKE(netif_ip4_addr);
    RESET_FAKE(pbuf_alloc);
    RESET_FAKE(pbuf_free);
    RESET_FAKE(tcpip_init);
}
