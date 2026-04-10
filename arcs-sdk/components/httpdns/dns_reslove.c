#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"

int lwip_custom_dns_resolve(const char *name, ip_addr_t *addr, u8_t addrtype, err_t *err)
{
#if CONFIG_HTTPDNS
    return http_dns_resolve(name, addr, addrtype, err);
#endif

    return 0;
}
