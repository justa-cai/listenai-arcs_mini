#ifndef MOCK_NETIF_H
#define MOCK_NETIF_H

#include "fff.h"
#include "netif/ethernet.h"

DECLARE_FAKE_VALUE_FUNC(err_t, ethernet_output, struct netif *, struct pbuf *, const struct eth_addr *,
                        const struct eth_addr *, uint16_t);

void mock_netif_reset(void);

#endif
