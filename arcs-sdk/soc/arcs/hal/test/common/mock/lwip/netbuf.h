#ifndef LWIP_NETBUF_H
#define LWIP_NETBUF_H

#include "lwip/tcpip.h"

struct netbuf {
    struct pbuf *p;
    struct pbuf *ptr;
};

void netbuf_delete(struct netbuf *buf);

#endif
